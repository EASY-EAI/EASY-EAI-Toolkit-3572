/**
 * MIPI 摄像头采集 + GStreamer 硬件编码模块
 *
 * 采集部分复用 camera/ 目录下的 V4L2 实现（与 rtspIPCamera 一致），
 * 编码部分使用 GStreamer 的 mpph264enc / mpph265enc，不再直接调用 RK_MPI 接口。
 *
 * 数据流：
 *   V4L2 (/dev/videoN) → NV12 → appsrc → mpph264enc/mpph265enc → appsink → 码流回调
 *
 * 用法：
 *   CameraCapturer cap(62, 1920, 1080, 30, "h264");
 *   cap.setFrameCallback(onFrame, NULL);
 *   cap.start();
 *   ...
 *   cap.stop();
 */

#ifndef __CAMERA_CAPTURER_H__
#define __CAMERA_CAPTURER_H__

#include <stdint.h>
#include <stdbool.h>
#include <string>
#include <pthread.h>

#include <gst/gst.h>
#include <gst/app/gstappsink.h>

#include "frameTypes.h"

class CameraCapturer
{
public:
    CameraCapturer(int camIndex, int width, int height,
                   int framerate, const std::string &fmt = "h264");
    ~CameraCapturer();

    /* 设置帧回调（编码后的码流） */
    void setFrameCallback(FrameCallback cb, void *userData);

    /* 启动采集编码 */
    int start();

    /* 停止采集编码 */
    void stop();

    /* 是否正在运行 */
    bool isRunning() const { return mRunning; }

private:
    int           mCamIndex;
    int           mWidth;
    int           mHeight;
    int           mFramerate;
    std::string   mFmt;
    FrameCallback mCb;
    void         *mUserData;

    char         *mCamBuf;
    size_t        mCamBufSize;

    GstElement   *mPipeline;
    GstElement   *mAppsrc;
    GstElement   *mAppsink;

    pthread_t     mStreamTid;
    volatile bool mRunning;

    int gstInit();
    void gstDeinit();

    static void *streamThread(void *para);
    static GstFlowReturn onNewSample(GstAppSink *sink, gpointer userData);
};

#endif /* __CAMERA_CAPTURER_H__ */
