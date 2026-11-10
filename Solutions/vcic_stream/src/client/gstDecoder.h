/**
 * GStreamer + Rockchip MPP 硬件解码模块
 *
 * 管线：appsrc → h264parse/h265parse → mppvideodec → NV12 → appsink
 * 解码后的 NV12 帧拷贝进 DMA-BUF 环形缓冲，以 fd 形式回调上层，
 * 由 display 库 zero-copy 送显。
 *
 * 用法：
 *   GstDecoder dec("h264");
 *   dec.setDecodedCallback(onDecoded, userData);
 *   dec.start();
 *   dec.pushData(h264Data, len);
 *   ...
 *   dec.stop();
 */

#ifndef __GST_DECODER_H__
#define __GST_DECODER_H__

#include <stdint.h>
#include <stdbool.h>
#include <string>
#include <pthread.h>

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>

/* 解码回调：交付 NV12 DMA-BUF fd（调用方须在回调返回前使用完毕） */
typedef void (*DecodedCallback)(int dmabuf_fd, int width, int height,
                                int hor_stride, int ver_stride,
                                void *userData);

class GstDecoder
{
public:
    GstDecoder(const std::string &fmt = "h264",
               int width = 1920, int height = 1080);
    ~GstDecoder();

    /* 设置解码回调 */
    void setDecodedCallback(DecodedCallback cb, void *userData);

    /* 启动解码器 */
    int start();

    /* 停止解码器 */
    void stop();

    /* 推送 H.264/H.265 数据到解码器 */
    int pushData(const uint8_t *data, uint32_t len);

    /* 是否正在运行 */
    bool isRunning() const { return mRunning; }

private:
    static const int DMA_BUF_NUM = 4;

    struct DmaBuf {
        int     fd;
        void   *map;
        size_t  size;
        int     pitch;
        int     width;
        int     height;
    };

    std::string      mFmt;
    int              mWidth;
    int              mHeight;
    DecodedCallback  mCb;
    void            *mUserData;

    GstElement      *mPipeline;
    GstElement      *mAppSrcElem;
    GstElement      *mAppSinkElem;
    GstAppSrc       *mAppSrc;
    GstAppSink      *mAppSink;

    pthread_t        mDecTid;
    volatile bool    mRunning;
    pthread_mutex_t  mPushMutex;

    DmaBuf           mDmaBufs[DMA_BUF_NUM];
    int              mDmaBufIdx;
    int              mBufWidth;
    int              mBufHeight;
    uint32_t         mDropCnt;

    int prepareDmaBufs(int width, int height);
    void releaseDmaBufs();

    static void *decodeThread(void *para);
};

#endif /* __GST_DECODER_H__ */
