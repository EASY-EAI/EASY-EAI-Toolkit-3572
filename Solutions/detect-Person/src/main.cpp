#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>

#include "system_opt.h"
#include "person_detect.h"
#include "display.h"
#include "rga_util.h"
#include "rga_wrapper.h"

#include "camera/camera.h"

/* 须在 OpenCV 之后包含：log_manager_pro.h 的 LOG_LEVEL_* 宏与 cv::LogLevel 冲突 */
#include "log_manager_pro.h"

log_handle_t g_log = LOG_HANDLE_INVALID;

using namespace std;
using namespace cv;
static Scalar colorArray[10]={
    Scalar(255, 0, 0, 255),
    Scalar(0, 255, 0, 255),
    Scalar(0,0,139,255),
    Scalar(0,100,0,255),
    Scalar(139,139,0,255),
    Scalar(209,206,0,255),
    Scalar(0,127,255,255),
    Scalar(139,61,72,255),
    Scalar(0,255,0,255),
    Scalar(255,0,0,255),
};

typedef struct{
    int resNumber;
	detect_result_group_t detect_Group;    
}AlgoRes_t;

typedef struct{
	AlgoRes_t algoRes[1];
}Result_t;

Mat algorithm_image;
pthread_mutex_t img_lock;

typedef struct {
    int convFd;
    void *convBuf;
    size_t convSize;
    int dispFd;
    void *dispBuf;
    size_t dispSize;
    int width;
    int height;
} DisplayFrameBuf_t;

static void releaseFrameBuf(DisplayFrameBuf_t *buf)
{
    if (buf->convFd >= 0) {
        munmap(buf->convBuf, buf->convSize);
        close(buf->convFd);
    }
    if (buf->dispFd >= 0) {
        munmap(buf->dispBuf, buf->dispSize);
        close(buf->dispFd);
    }
    memset(buf, 0, sizeof(*buf));
    buf->convFd = -1;
    buf->dispFd = -1;
}

static int prepareFrameBuf(DisplayFrameBuf_t *buf, int width, int height)
{
    if (buf->convFd >= 0 && buf->width == width && buf->height == height)
        return 0;

    releaseFrameBuf(buf);

    buf->convSize = (size_t)width * height * 4;
    if (0 != alloc_dmabuf(buf->convSize, &buf->convFd, &buf->convBuf)) {
        buf->convSize = 0;
        return -1;
    }
    buf->dispSize = (size_t)width * height * 4;
    if (0 != alloc_dmabuf(buf->dispSize, &buf->dispFd, &buf->dispBuf)) {
        releaseFrameBuf(buf);
        return -1;
    }
    buf->width = width;
    buf->height = height;
    return 0;
}

/* BGR 源帧经 RGA 转 BGRX、旋转 270 度后送显，显示层负责缩放到整屏 */
static int commitFrameToDisplay(const Mat &frame, DisplayFrameBuf_t *buf)
{
    if (frame.empty() || NULL == frame.data)
        return -1;

    int srcW = frame.cols;
    int srcH = frame.rows;
    if (0 != prepareFrameBuf(buf, srcW, srcH))
        return -1;

    RgaBuffer srcFrame;
    memset(&srcFrame, 0, sizeof(srcFrame));
    srcFrame.mem_type = RGA_MEM_VIR;
    srcFrame.fd = -1;
    srcFrame.vir = frame.data;
    srcFrame.width = srcW;
    srcFrame.height = srcH;
    srcFrame.wstride = srcW;
    srcFrame.hstride = srcH;
    srcFrame.format = RGA_FMT_BGR_888;

    RgaBuffer convFrame;
    memset(&convFrame, 0, sizeof(convFrame));
    convFrame.mem_type = RGA_MEM_FD;
    convFrame.fd = buf->convFd;
    convFrame.width = srcW;
    convFrame.height = srcH;
    convFrame.wstride = srcW;
    convFrame.hstride = srcH;
    convFrame.format = RGA_FMT_BGRX_8888;

    RgaBuffer dispFrame;
    memset(&dispFrame, 0, sizeof(dispFrame));
    dispFrame.mem_type = RGA_MEM_FD;
    dispFrame.fd = buf->dispFd;
    dispFrame.width = srcH;
    dispFrame.height = srcW;
    dispFrame.wstride = srcH;
    dispFrame.hstride = srcW;
    dispFrame.format = RGA_FMT_BGRX_8888;

    if (0 != rga_convert(&srcFrame, &convFrame))
        return -1;
    if (0 != rga_rotate(&convFrame, &dispFrame, RGA_ROT_270))
        return -1;

    return display_primary_commit(buf->dispFd, dispFrame.width, dispFrame.height,
                                  DISPLAY_FMT_XRGB8888, dispFrame.wstride * 4);
}

int plot_one_box(Mat src, int x1, int x2, int y1, int y2, char *label, char colour)
{
    int tl = round(0.002 * (src.rows + src.cols) / 2) + 1;
    rectangle(src, cv::Point(x1, y1), cv::Point(x2, y2), colorArray[(unsigned char)colour], 3);

    int tf = max(tl -1, 1);

    int base_line = 0;
    cv::Size t_size = getTextSize(label, FONT_HERSHEY_SIMPLEX, (float)tl/3, tf, &base_line);
    int x3 = x1 + t_size.width;
    int y3 = y1 - t_size.height - 3;

    rectangle(src, cv::Point(x1, y1), cv::Point(x3, y3), colorArray[(unsigned char)colour], -1);
    putText(src, label, cv::Point(x1, y1 - 2), FONT_HERSHEY_SIMPLEX, (float)tl/3, cv::Scalar(255, 255, 255, 255), tf, 8);
    return 0;
}

// 目标识别线程
void *detect_thread_entry(void *para)
{
    int ret;
    Result_t *pResult = (Result_t *)para;
	
    // 人员检测初始化
    person_detect_ctx ctx;
    memset(&ctx, 0, sizeof(ctx));
    if (0 != person_detect_init(&ctx, "person_detect.model")) {
        PRINT_ERROR(g_log, "person_detect_init failed, model: person_detect.model\n");
        return NULL;
    }
	
    Mat image;
    while(1)
    {
        if(algorithm_image.empty()) {
            usleep(5);
            continue;
        }
		
        pthread_mutex_lock(&img_lock);
        image = algorithm_image.clone();
        pthread_mutex_unlock(&img_lock);
        // 算法分析
        ret = person_detect_run(&ctx, image, &pResult->algoRes[0].detect_Group);
        if(0 != ret){
            usleep(1000);
            continue;
        }

        pResult->algoRes[0].resNumber = pResult->algoRes[0].detect_Group.count;
        if(pResult->algoRes[0].resNumber <= 0){
            memset(&pResult->algoRes[0], 0, sizeof(AlgoRes_t));
            usleep(1000);
            continue;
        }

        PRINT_INFO(g_log, "\n>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
        PRINT_INFO(g_log, "person number : %d\n", pResult->algoRes[0].resNumber);
		
        usleep(16*1000);
    }
    /* 人员检测释放 */
    person_detect_release(&ctx);
    return NULL;
}

int main(int argc, char **argv)
{
    log_mgr_t logMgr = log_manager_init("/userdata/logs/detect_person.ini");
    g_log = log_register(logMgr, "person");

    int ret = 0;
    if(1 == argc){
        PRINT_ERROR(g_log, "\nerr: Missing parameter!\n");
        PRINT_INFO(g_log, "================= [usage] ==================\n");
        PRINT_INFO(g_log, "example:\n");
        PRINT_INFO(g_log, "\t%s <23/31>\n", argv[0]);
        PRINT_INFO(g_log, "--------------------------------------------\n");
        return 0;
    }

    // 目标识别线程相关变量
    pthread_t mTid;
    Result_t Result;

    // camera原始图像相关变量
    int skip = 0;
    char *pbuf = NULL;
    Mat image;

    DisplayFrameBuf_t frameBuf;
    memset(&frameBuf, 0, sizeof(frameBuf));
    frameBuf.convFd = -1;
    frameBuf.dispFd = -1;

    // 1.初始化显示屏
    if (0 != display_primary_init()) {
        PRINT_ERROR(g_log, "display_primary_init failed\n");
        return -1;
    }
    if (0 != rga_init(RGA_LOG_MGR_INVALID)) {
        PRINT_ERROR(g_log, "rga_init failed\n");
        display_primary_uninit();
        return -1;
    }

    char timestamp[64] = {0};

    // 2.打开摄像头
#define CAMERA_WIDTH    1920
#define CAMERA_HEIGHT   1080
#define	IMGRATIO        3
#define	IMAGE_SIZE      (CAMERA_WIDTH*CAMERA_HEIGHT*IMGRATIO)
    int cameraIndex = atoi(argv[1]); //通常csi0是video23, csi1是video31
    ret = mipicamera_init(cameraIndex, CAMERA_WIDTH, CAMERA_HEIGHT, 0);
    if (ret) {
        PRINT_ERROR(g_log, "error: %s, %d\n", __func__, __LINE__);
        goto exit3;
    }
    
    pbuf = (char *)malloc(IMAGE_SIZE);
    //pbuf = dma_alloc(&dma_fd, IMAGE_SIZE);
    if (!pbuf) {
        PRINT_ERROR(g_log, "error: %s, %d\n", __func__, __LINE__);
        ret = -1;
        goto exit2;
    }
    
    //跳过前10帧
    skip = 10;
    while(skip--) {
        ret = mipicamera_getframe(cameraIndex, pbuf);
        if (ret) {
            PRINT_ERROR(g_log, "error: %s, %d\n", __func__, __LINE__);
            goto exit1;
        }
    }
	
    // 3.创建识别线程，以及图像互斥锁
    pthread_mutex_init(&img_lock, NULL);
    memset(&Result, 0, sizeof(Result_t));
    CreateNormalThread(detect_thread_entry, &Result, &mTid);

    // 4.(取流 + 显示)循环
    while(1) {
        // 4.1、取流
        pthread_mutex_lock(&img_lock);
        ret = mipicamera_getframe(cameraIndex, pbuf);
        if(ret) {
            PRINT_ERROR(g_log, "error: %s, %d\n", __func__, __LINE__);
            pthread_mutex_unlock(&img_lock);
            continue;
        }
        algorithm_image = Mat(CAMERA_HEIGHT, CAMERA_WIDTH, CV_8UC3, pbuf);
        image = algorithm_image.clone();
        pthread_mutex_unlock(&img_lock);

        // 4.2、显示
        char text[256];
        for (int i = 0; i < Result.algoRes[0].resNumber; i++) {
            detect_result_t *det_result = &(Result.algoRes[0].detect_Group.results[i]);
            if( det_result->prop < 0.4) {
                continue;
            }

           // 标出识别目标框
           sprintf(text, "%s %.1f%%", det_result->name, det_result->prop * 100);
            PRINT_INFO(g_log, "%s @ (%d %d %d %d) %f\n", det_result->name, det_result->box.left, det_result->box.top,
    			   det_result->box.right, det_result->box.bottom, det_result->prop);
            int x1 = det_result->box.left;
            int y1 = det_result->box.top;
            int x2 = det_result->box.right;
            int y2 = det_result->box.bottom;
            // 标出识别目标定位标记
            /*
            rectangle(image, cv::Point(x1, y1), cv::Point(x2, y2), cv::Scalar(255, 0, 0, 255), 3);
            putText(image, text, cv::Point(x1, y1 + 12), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0));
            */
            plot_one_box(image, x1, x2, y1, y2, text, i%10);
        }

        if (0 != commitFrameToDisplay(image, &frameBuf)) {
            PRINT_ERROR(g_log, "error: commit frame to display failed\n");
            usleep(15*1000);
            continue;
        }
        usleep(15*1000); //15ms
    }

exit1:
    //dma_free(&dma_fd, pbuf, IMAGE_SIZE);
    free(pbuf);
    pbuf = NULL;
    
exit2:
    mipicamera_exit(cameraIndex);
    
exit3:
    releaseFrameBuf(&frameBuf);
    rga_deinit();
    display_primary_uninit();
    
    return ret;
}

