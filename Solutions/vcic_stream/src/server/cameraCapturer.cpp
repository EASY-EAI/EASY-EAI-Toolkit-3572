/**
 * MIPI 摄像头采集 + GStreamer 硬件编码模块 —— 实现
 *
 * 采集：camera/ 目录下的 V4L2 实现（mipicamera_*），与 rtspIPCamera 一致。
 * 编码：GStreamer mpph264enc / mpph265enc，避免依赖 RK_MPI_* 接口。
 */

#include "cameraCapturer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "log_manager_pro.h"
#include "camera/camera.h"
#include <rga/RgaApi.h>

/* ======================== 日志句柄 ======================== */
static log_mgr_t    s_mgr  = LOG_MGR_INVALID;
static log_handle_t s_hLog = LOG_HANDLE_INVALID;
static log_handle_t getLogHandle()
{
    if (s_hLog < 0) {
        if (s_mgr < 0) s_mgr = log_manager_init("/userdata/logs/vcic_stream_server.ini");
        s_hLog = log_register(s_mgr, "camcapture");
    }
    return s_hLog;
}

/* ======================== 构造 / 析构 ======================== */

CameraCapturer::CameraCapturer(int camIndex, int width, int height,
                               int framerate, const std::string &fmt)
    : mCamIndex(camIndex)
    , mWidth(width)
    , mHeight(height)
    , mFramerate(framerate > 0 ? framerate : 30)
    , mFmt(fmt)
    , mCb(NULL)
    , mUserData(NULL)
    , mCamBuf(NULL)
    , mCamBufSize(0)
    , mPipeline(NULL)
    , mAppsrc(NULL)
    , mAppsink(NULL)
    , mStreamTid(0)
    , mRunning(false)
{
}

CameraCapturer::~CameraCapturer()
{
    stop();
}

void CameraCapturer::setFrameCallback(FrameCallback cb, void *userData)
{
    mCb = cb;
    mUserData = userData;
}

/* ======================== GStreamer 编码管线 ======================== */

int CameraCapturer::gstInit()
{
    const char *encoder = (mFmt == "h265") ? "mpph265enc" : "mpph264enc";
    const char *parser  = (mFmt == "h265") ? "h265parse"  : "h264parse";
    /* 统一输出 AnnexB 字节流，VCIC 客户端可直接送硬件解码器 */
    const char *streamCaps = (mFmt == "h265")
        ? "video/x-h265,stream-format=byte-stream,alignment=au"
        : "video/x-h264,stream-format=byte-stream,alignment=au";

    char desc[768];
    snprintf(desc, sizeof(desc),
             "appsrc name=src is-live=true block=true format=time "
             "caps=video/x-raw,format=NV12,width=%d,height=%d,framerate=%d/1 ! "
             "%s ! %s config-interval=1 ! %s ! "
             "appsink name=sink emit-signals=true sync=false drop=true max-buffers=2",
             mWidth, mHeight, mFramerate, encoder, parser, streamCaps);

    mPipeline = gst_parse_launch(desc, NULL);
    if (!mPipeline) {
        PRINT_ERROR(getLogHandle(), "gst_parse_launch failed: %s\n", desc);
        return -1;
    }

    mAppsrc  = gst_bin_get_by_name(GST_BIN(mPipeline), "src");
    mAppsink = gst_bin_get_by_name(GST_BIN(mPipeline), "sink");
    if (!mAppsrc || !mAppsink) {
        PRINT_ERROR(getLogHandle(), "appsrc/appsink not found in pipeline\n");
        gstDeinit();
        return -1;
    }

    g_signal_connect(mAppsink, "new-sample", G_CALLBACK(onNewSample), this);

    if (gst_element_set_state(mPipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        PRINT_ERROR(getLogHandle(), "failed to set pipeline to PLAYING\n");
        gstDeinit();
        return -1;
    }

    return 0;
}

void CameraCapturer::gstDeinit()
{
    if (mPipeline) {
        gst_element_set_state(mPipeline, GST_STATE_NULL);
    }
    if (mAppsink) {
        gst_object_unref(mAppsink);
        mAppsink = NULL;
    }
    if (mAppsrc) {
        gst_object_unref(mAppsrc);
        mAppsrc = NULL;
    }
    if (mPipeline) {
        gst_object_unref(mPipeline);
        mPipeline = NULL;
    }
}

GstFlowReturn CameraCapturer::onNewSample(GstAppSink *sink, gpointer userData)
{
    CameraCapturer *self = (CameraCapturer *)userData;
    GstSample *sample = gst_app_sink_pull_sample(sink);
    if (!sample) {
        return GST_FLOW_ERROR;
    }

    GstBuffer *buffer = gst_sample_get_buffer(sample);
    GstMapInfo map;
    if (buffer && gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        if (self->mCb && map.size > 0) {
            FrameDesc_t desc;
            memset(&desc, 0, sizeof(desc));
            desc.width  = self->mWidth;
            desc.height = self->mHeight;
            strncpy(desc.strFmt, self->mFmt.c_str(), sizeof(desc.strFmt) - 1);
            self->mCb((const uint8_t *)map.data, (uint32_t)map.size,
                      &desc, self->mUserData);
        }
        gst_buffer_unmap(buffer, &map);
    }

    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

/* ======================== 取流线程 ======================== */

void *CameraCapturer::streamThread(void *para)
{
    CameraCapturer *self = (CameraCapturer *)para;
    GstClockTime duration = gst_util_uint64_scale_int(1, GST_SECOND, self->mFramerate);
    GstClockTime pts = 0;
    uint32_t frame_cnt = 0;

    PRINT_INFO(getLogHandle(), "capture thread started (cam=%d %dx%d@%d %s)\n",
               self->mCamIndex, self->mWidth, self->mHeight,
               self->mFramerate, self->mFmt.c_str());

    while (self->mRunning) {
        if (mipicamera_getframe(self->mCamIndex, self->mCamBuf) != 0) {
            usleep(1000);
            continue;
        }

        GstBuffer *buffer = gst_buffer_new_allocate(NULL, self->mCamBufSize, NULL);
        if (!buffer) {
            usleep(1000);
            continue;
        }
        gst_buffer_fill(buffer, 0, self->mCamBuf, self->mCamBufSize);

        GST_BUFFER_PTS(buffer)      = pts;
        GST_BUFFER_DURATION(buffer) = duration;
        pts += duration;

        GstFlowReturn ret;
        g_signal_emit_by_name(self->mAppsrc, "push-buffer", buffer, &ret);
        gst_buffer_unref(buffer);

        if (ret != GST_FLOW_OK) {
            if (self->mRunning) {
                PRINT_WARN(getLogHandle(), "appsrc push-buffer ret=%s\n",
                           gst_flow_get_name(ret));
                usleep(5000);
            }
            continue;
        }

        frame_cnt++;
        if ((frame_cnt % 30) == 0) {
            PRINT_DEBUG(getLogHandle(), "capture thread alive: cnt=%u\n", frame_cnt);
        }
    }

    /* 通知编码器刷新尾帧 */
    g_signal_emit_by_name(self->mAppsrc, "end-of-stream", NULL);

    PRINT_DEBUG(getLogHandle(), "capture thread exiting (cnt=%u)\n", frame_cnt);
    return NULL;
}

/* ======================== start / stop ======================== */

int CameraCapturer::start()
{
    if (mRunning) {
        return 0;
    }

    if (!gst_is_initialized()) {
        gst_init(NULL, NULL);
    }

    mCamBufSize = (size_t)mWidth * mHeight * 3 / 2;  /* NV12 */
    mCamBuf = (char *)malloc(mCamBufSize);
    if (!mCamBuf) {
        PRINT_ERROR(getLogHandle(), "malloc camera buffer failed (size=%zu)\n", mCamBufSize);
        return -1;
    }

    if (mipicamera_init(mCamIndex, mWidth, mHeight, 0) != 0) {
        PRINT_ERROR(getLogHandle(), "mipicamera_init failed (cam=%d)\n", mCamIndex);
        free(mCamBuf);
        mCamBuf = NULL;
        return -1;
    }
    mipicamera_set_format(mCamIndex, RK_FORMAT_YCbCr_420_SP);

    if (gstInit() != 0) {
        mipicamera_exit(mCamIndex);
        free(mCamBuf);
        mCamBuf = NULL;
        return -1;
    }

    mRunning = true;
    pthread_create(&mStreamTid, NULL, streamThread, this);

    PRINT_INFO(getLogHandle(), "CameraCapturer started: cam=%d %dx%d@%d %s\n",
               mCamIndex, mWidth, mHeight, mFramerate, mFmt.c_str());
    return 0;
}

void CameraCapturer::stop()
{
    if (!mRunning) {
        return;
    }

    mRunning = false;

    if (mStreamTid) {
        pthread_join(mStreamTid, NULL);
        mStreamTid = 0;
    }

    gstDeinit();
    mipicamera_exit(mCamIndex);

    free(mCamBuf);
    mCamBuf = NULL;

    PRINT_INFO(getLogHandle(), "CameraCapturer stopped\n");
}
