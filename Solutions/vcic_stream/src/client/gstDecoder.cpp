/**
 * GStreamer + Rockchip MPP 硬件解码模块 —— 实现
 *
 * 流程：
 *   1. 构建管线 appsrc → h264parse/h265parse → mppvideodec → NV12 → appsink
 *   2. pushData() 把 H.264/H.265 码流送入 appsrc（appsrc 内部队列解耦接收与解码）
 *   3. 解码线程从 appsink 取 NV12 帧，按行拷贝到 DMA-BUF 环形缓冲
 *   4. 回调上层交付 DMA-BUF fd，由 display 库 zero-copy 送显
 *
 * 关键设计：
 *   - 送流由 appsrc 内部队列管理，队列满时阻塞以保持实时性
 *   - 使用 4 块 DMA-BUF 轮转，避免显示层正在读取的缓冲被下一帧覆盖
 *   - display 接口要求 NV12 的 UV 平面紧邻 Y 平面，故按 pitch*height 重新排布
 */

#include "gstDecoder.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#include <gst/video/video.h>

#include "log_manager_pro.h"
#include "rga_wrapper.h"

#define PULL_TIMEOUT_MS  100
#define ALIGN_UP(v, a)   (((v) + (a) - 1) & ~((a) - 1))
#define DMABUF_PITCH_ALIGN 64

/* ======================== 日志句柄 ======================== */
static log_mgr_t    s_mgr  = LOG_MGR_INVALID;
static log_handle_t s_hLog = LOG_HANDLE_INVALID;
static log_handle_t getLogHandle()
{
    if (s_hLog < 0) {
        if (s_mgr < 0) s_mgr = log_manager_init("/userdata/logs/vcic_stream_client.ini");
        s_hLog = log_register(s_mgr, "gstdec");
    }
    return s_hLog;
}

/* ======================== 构造 / 析构 ======================== */

GstDecoder::GstDecoder(const std::string &fmt, int width, int height)
    : mFmt(fmt)
    , mWidth(width)
    , mHeight(height)
    , mCb(NULL)
    , mUserData(NULL)
    , mPipeline(NULL)
    , mAppSrcElem(NULL)
    , mAppSinkElem(NULL)
    , mAppSrc(NULL)
    , mAppSink(NULL)
    , mDecTid(0)
    , mRunning(false)
    , mDmaBufIdx(0)
    , mBufWidth(0)
    , mBufHeight(0)
    , mDropCnt(0)
{
    memset(mDmaBufs, 0, sizeof(mDmaBufs));
    for (int i = 0; i < DMA_BUF_NUM; i++) {
        mDmaBufs[i].fd = -1;
    }
    pthread_mutex_init(&mPushMutex, NULL);
}

GstDecoder::~GstDecoder()
{
    stop();
    pthread_mutex_destroy(&mPushMutex);
}

void GstDecoder::setDecodedCallback(DecodedCallback cb, void *userData)
{
    mCb = cb;
    mUserData = userData;
}

/* ======================== DMA-BUF 环形缓冲 ======================== */

int GstDecoder::prepareDmaBufs(int width, int height)
{
    if (mDmaBufs[0].fd >= 0 && mBufWidth == width && mBufHeight == height) {
        return 0;
    }

    releaseDmaBufs();

    int pitch = ALIGN_UP(width, DMABUF_PITCH_ALIGN);
    size_t size = (size_t)pitch * height * 3 / 2;

    for (int i = 0; i < DMA_BUF_NUM; i++) {
        int fd = -1;
        void *map = NULL;
        if (alloc_dmabuf(size, &fd, &map) != 0) {
            PRINT_ERROR(getLogHandle(), "alloc_dmabuf fail (%zu bytes)\n", size);
            releaseDmaBufs();
            return -1;
        }
        mDmaBufs[i].fd     = fd;
        mDmaBufs[i].map    = map;
        mDmaBufs[i].size   = size;
        mDmaBufs[i].pitch  = pitch;
        mDmaBufs[i].width  = width;
        mDmaBufs[i].height = height;
    }

    mBufWidth   = width;
    mBufHeight  = height;
    mDmaBufIdx  = 0;

    PRINT_INFO(getLogHandle(), "DMA-BUF ring ready: %dx%d pitch=%d buf=%d\n",
               width, height, pitch, DMA_BUF_NUM);
    return 0;
}

void GstDecoder::releaseDmaBufs()
{
    for (int i = 0; i < DMA_BUF_NUM; i++) {
        if (mDmaBufs[i].fd >= 0) {
            if (mDmaBufs[i].map) {
                munmap(mDmaBufs[i].map, mDmaBufs[i].size);
            }
            close(mDmaBufs[i].fd);
        }
        memset(&mDmaBufs[i], 0, sizeof(mDmaBufs[i]));
        mDmaBufs[i].fd = -1;
    }
    mBufWidth  = 0;
    mBufHeight = 0;
    mDmaBufIdx = 0;
}

/* ======================== 解码线程 ======================== */

void *GstDecoder::decodeThread(void *para)
{
    GstDecoder *self = (GstDecoder *)para;
    uint32_t frame_cnt = 0;
    uint32_t skip_cnt  = 0;

    PRINT_INFO(getLogHandle(), "decode thread started\n");

    while (self->mRunning) {
        GstSample *sample = gst_app_sink_try_pull_sample(
                self->mAppSink, PULL_TIMEOUT_MS * GST_MSECOND);
        if (!sample) {
            if (gst_app_sink_is_eos(self->mAppSink)) {
                PRINT_INFO(getLogHandle(), "appsink reach eos\n");
                break;
            }
            continue;
        }

        GstCaps   *caps = gst_sample_get_caps(sample);
        GstBuffer *buf  = gst_sample_get_buffer(sample);
        GstVideoInfo info;

        if (!caps || !buf || !gst_video_info_from_caps(&info, caps)) {
            gst_sample_unref(sample);
            continue;
        }

        int width  = GST_VIDEO_INFO_WIDTH(&info);
        int height = GST_VIDEO_INFO_HEIGHT(&info);
        if (width <= 0 || height <= 0) {
            gst_sample_unref(sample);
            continue;
        }

        if (self->prepareDmaBufs(width, height) != 0) {
            gst_sample_unref(sample);
            continue;
        }

        int    y_stride  = GST_VIDEO_INFO_PLANE_STRIDE(&info, 0);
        int    uv_stride = GST_VIDEO_INFO_PLANE_STRIDE(&info, 1);
        gsize  y_offset  = GST_VIDEO_INFO_PLANE_OFFSET(&info, 0);
        gsize  uv_offset = GST_VIDEO_INFO_PLANE_OFFSET(&info, 1);

        /* 实际缓冲布局以 GstVideoMeta 为准 */
        GstVideoMeta *meta = gst_buffer_get_video_meta(buf);
        if (meta && meta->n_planes >= 2) {
            y_stride  = meta->stride[0];
            uv_stride = meta->stride[1];
            y_offset  = meta->offset[0];
            uv_offset = meta->offset[1];
        }

        GstMapInfo map;
        if (!gst_buffer_map(buf, &map, GST_MAP_READ)) {
            skip_cnt++;
            gst_sample_unref(sample);
            continue;
        }

        gsize buf_size = map.size;
        bool ok = y_stride > 0 && uv_stride > 0
               && y_offset + (gsize)(height - 1) * y_stride + width <= buf_size
               && uv_offset + (gsize)(height / 2 - 1) * uv_stride + width <= buf_size;

        if (ok) {
            DmaBuf &dst = self->mDmaBufs[self->mDmaBufIdx];
            uint8_t *y_dst  = (uint8_t *)dst.map;
            uint8_t *uv_dst = y_dst + (size_t)dst.pitch * height;

            for (int i = 0; i < height; i++) {
                memcpy(y_dst + (size_t)i * dst.pitch,
                       map.data + y_offset + (size_t)i * y_stride, width);
            }
            for (int i = 0; i < height / 2; i++) {
                memcpy(uv_dst + (size_t)i * dst.pitch,
                       map.data + uv_offset + (size_t)i * uv_stride, width);
            }

            gst_buffer_unmap(buf, &map);

            /* 使 CPU 写入对显示设备可见 */
            dma_sync_cpu_to_device(dst.fd);

            int fd    = dst.fd;
            int pitch = dst.pitch;
            self->mDmaBufIdx = (self->mDmaBufIdx + 1) % DMA_BUF_NUM;

            frame_cnt++;
            if (self->mCb) {
                self->mCb(fd, width, height, pitch, height, self->mUserData);
            }

            if ((frame_cnt % 30) == 0) {
                PRINT_DEBUG(getLogHandle(), "decode alive: cnt=%u %dx%d fd=%d pitch=%d skip=%u\n",
                            frame_cnt, width, height, fd, pitch, skip_cnt);
            }
        } else {
            gst_buffer_unmap(buf, &map);
            skip_cnt++;
            if ((skip_cnt % 30) == 1) {
                PRINT_WARN(getLogHandle(), "decoded frame layout unexpected (%dx%d y_off=%zu y_stride=%d uv_off=%zu uv_stride=%d size=%zu)\n",
                           width, height, y_offset, y_stride, uv_offset, uv_stride, buf_size);
            }
        }

        gst_sample_unref(sample);
    }

    PRINT_DEBUG(getLogHandle(), "decode thread exiting (cnt=%u skip=%u)\n", frame_cnt, skip_cnt);
    return NULL;
}

/* ======================== start / stop ======================== */

int GstDecoder::start()
{
    if (mRunning) {
        return 0;
    }

    if (!gst_is_initialized()) {
        gst_init(NULL, NULL);
    }

    bool isH265 = (mFmt == "h265");
    const char *parser   = isH265 ? "h265parse" : "h264parse";
    const char *srcCaps  = isH265
        ? "video/x-h265,stream-format=byte-stream,alignment=au"
        : "video/x-h264,stream-format=byte-stream,alignment=au";

    char desc[1024];
    snprintf(desc, sizeof(desc),
             "appsrc name=src is-live=true format=time do-timestamp=true block=true "
             "max-bytes=8388608 caps=%s ! "
             "%s ! mppvideodec ! video/x-raw,format=NV12 ! "
             "appsink name=sink sync=false drop=true max-buffers=4",
             srcCaps, parser);

    GError *err = NULL;
    mPipeline = gst_parse_launch(desc, &err);
    if (!mPipeline) {
        PRINT_ERROR(getLogHandle(), "gst_parse_launch fail: %s\n",
                    err ? err->message : "unknown");
        fprintf(stderr, "[gstdec] gst_parse_launch fail: %s\n",
                err ? err->message : "unknown");
        if (err) g_error_free(err);
        return -1;
    }

    mAppSrcElem  = gst_bin_get_by_name(GST_BIN(mPipeline), "src");
    mAppSinkElem = gst_bin_get_by_name(GST_BIN(mPipeline), "sink");
    if (!mAppSrcElem || !mAppSinkElem) {
        PRINT_ERROR(getLogHandle(), "pipeline missing appsrc/appsink\n");
        stop();
        return -1;
    }
    mAppSrc  = GST_APP_SRC(mAppSrcElem);
    mAppSink = GST_APP_SINK(mAppSinkElem);

    GstStateChangeReturn sret = gst_element_set_state(mPipeline, GST_STATE_PLAYING);
    if (sret == GST_STATE_CHANGE_FAILURE) {
        PRINT_ERROR(getLogHandle(), "pipeline set PLAYING fail\n");
        stop();
        return -1;
    }

    mRunning = true;
    if (pthread_create(&mDecTid, NULL, decodeThread, this) != 0) {
        mRunning = false;
        mDecTid = 0;
        PRINT_ERROR(getLogHandle(), "create decode thread fail\n");
        stop();
        return -1;
    }

    fprintf(stderr, "[gstdec] GstDecoder started: fmt=%s %dx%d\n",
            mFmt.c_str(), mWidth, mHeight);
    PRINT_INFO(getLogHandle(), "GstDecoder started: fmt=%s %dx%d\n",
               mFmt.c_str(), mWidth, mHeight);

    return 0;
}

void GstDecoder::stop()
{
    mRunning = false;

    /* 先把管线切到 NULL：唤醒可能阻塞在 appsrc 队列上的 pushData */
    if (mPipeline) {
        gst_element_set_state(mPipeline, GST_STATE_NULL);
    }

    if (mDecTid) {
        pthread_join(mDecTid, NULL);
        mDecTid = 0;
    }

    pthread_mutex_lock(&mPushMutex);
    if (mAppSinkElem) {
        gst_object_unref(mAppSinkElem);
        mAppSinkElem = NULL;
    }
    if (mAppSrcElem) {
        gst_object_unref(mAppSrcElem);
        mAppSrcElem = NULL;
    }
    if (mPipeline) {
        gst_object_unref(mPipeline);
        mPipeline = NULL;
    }
    mAppSrc  = NULL;
    mAppSink = NULL;
    pthread_mutex_unlock(&mPushMutex);

    releaseDmaBufs();

    PRINT_INFO(getLogHandle(), "GstDecoder stopped\n");
}

/* ======================== pushData ======================== */
/* appsrc 内部队列负责解耦；队列满时阻塞送流线程，保证实时性。 */
int GstDecoder::pushData(const uint8_t *data, uint32_t len)
{
    if (!data || len == 0) {
        return -1;
    }

    pthread_mutex_lock(&mPushMutex);
    if (!mRunning || !mAppSrc) {
        pthread_mutex_unlock(&mPushMutex);
        return -1;
    }

    GstBuffer *buffer = gst_buffer_new_allocate(NULL, len, NULL);
    if (!buffer) {
        pthread_mutex_unlock(&mPushMutex);
        PRINT_ERROR(getLogHandle(), "gst_buffer_new_allocate fail (len=%u)\n", len);
        return -1;
    }
    gst_buffer_fill(buffer, 0, data, len);

    GstFlowReturn ret = gst_app_src_push_buffer(mAppSrc, buffer);
    pthread_mutex_unlock(&mPushMutex);

    if (ret != GST_FLOW_OK) {
        mDropCnt++;
        if ((mDropCnt % 30) == 1) {
            PRINT_WARN(getLogHandle(), "push-buffer fail ret=%d (len=%u drop=%u)\n",
                       ret, len, mDropCnt);
        }
        return -1;
    }

    return 0;
}
