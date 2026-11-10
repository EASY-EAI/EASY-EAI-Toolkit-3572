//=====================  C++  =====================
#include <string>
#include <list>
//=====================   C   =====================
#include <sys/mman.h>
#include <unistd.h>

#include "system.h"
//=====================  PRJ  =====================
#include "system_opt.h"
#include "rga_wrapper.h"
#include "display_pro.h"

/* display_pro.h 未引入 libdrm 头文件，取 DRM_PLANE_TYPE_PRIMARY 的数值 */
#define DISPLAY_PLANE_TYPE_PRIMARY 1

#include "analyzer.h"

/* 须在 OpenCV 之后包含：log_manager_pro.h 的 LOG_LEVEL_* 宏与 cv::LogLevel 冲突 */
#include "log_manager_pro.h"

extern log_handle_t g_log;

using namespace cv;


static Scalar colorArray[10]={
    Scalar(0, 0, 255, 255),
    Scalar(0, 255, 0, 255),
    Scalar(139,0,0,255),
    Scalar(0,100,0,255),
    Scalar(0,139,139,255),
    Scalar(0,206,209,255),
    Scalar(255,127,0,255),
    Scalar(72,61,139,255),
    Scalar(0,255,0,255),
    Scalar(0,0,255,255),
};
static int plot_one_box(Mat src, int x1, int x2, int y1, int y2, char *label, char colour)
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
static void paint_algorithm_result(Mat image, ChnResult_t result)
{
    // 把算法结果绘制在图像上
    char text[256];
    for (int algoIndex = 0; algoIndex < ALGOMAXNUM; algoIndex++){
        for (int j = 0; j < result.algoRes[algoIndex].resNumber; j++) {
            detect_result_t *det_result = &(result.algoRes[algoIndex].detect_Group.results[j]);
            if( det_result->prop < 0.4) {
                continue;
            }
            
            // 标出识别目标框
            sprintf(text, "%s %.1f%%", det_result->name, det_result->prop * 100);
#if 0
            printf("%s @ (%d %d %d %d) %f\n", det_result->name, det_result->box.left, det_result->box.top,
                   det_result->box.right, det_result->box.bottom, det_result->prop);
#endif
            int x1 = det_result->box.left;
            int y1 = det_result->box.top;
            int x2 = det_result->box.right;
            int y2 = det_result->box.bottom;
            // 标出识别目标定位标记
            plot_one_box(image, x1, x2, y1, y2, text, j%10);
        }
    }
}

/**
 * 每个窗口的画面先按 270° 顺时针旋转，再缩放到窗口大小，最后贴到整屏画布上。
 * 旋转方向与原 display 库的 HAL_TRANSFORM_ROT_270 保持一致。
 */
static bool blitToWindow(Mat &canvas, const Mat &src, const Rect &rect,
                         Mat &rotated, Mat &scaled, Mat &bgra)
{
    if (src.empty() || rect.width <= 0 || rect.height <= 0)
        return false;

    cv::rotate(src, rotated, cv::ROTATE_90_COUNTERCLOCKWISE);
    cv::resize(rotated, scaled, rect.size(), 0, 0, INTER_LINEAR);
    /* XRGB8888 在内存中的字节序为 B/G/R/X，Alpha 位对 primary 图层无影响 */
    cv::cvtColor(scaled, bgra, COLOR_BGR2BGRA);
    bgra.copyTo(canvas(rect));
    return true;
}


class Analyzer
{
public:
	Analyzer(int32_t maxChn);
	~Analyzer();

    static Analyzer *instance() { return m_pSelf; }
    static void createAnalyzer(int32_t maxChn);
    
    // --视频资源处理
    // 1.更新某路[视频]通道图像数据
    int32_t upDateVideoChannel(int chnId, char *imgData, ImgDesc_t imgDesc);
    // 2.取某路[视频]通道图像数据地址
    vChnObject *getVideoChnObject(int chnId);
    uint8_t* videoChannelData(vChnObject *pVideoObj, int &width, int &height);
    // 3.取某路[视频]通道的分析结果
    int32_t videoChannelAnalyRes(int chnId);

    // --音频资源处理
    // 1.更新某路[音频]通道数据
    // 2.取某路[音频]通道数据地址
    // 3.取某路[音频]通道的分析结果
    

    bool mAnalyzeThreadWorking;
    bool mDisplayThreadWorking;
    pthread_mutex_t mVideoChnLock;
    //pthread_mutex_t mAudioChnLock;
    int32_t mMaxChnNum;
protected:
    vChnObject *createVideoChnObject(int32_t chnId, int32_t imgWidth, int32_t imgHeight);
    int32_t releaseVideoChnObject(vChnObject *pObj);
    int32_t delAllVideoChannel();

    //aChnObject *searchAudioChnObject(int chnId);
    //aChnObject *createAudioChnObject();
    //int32_t releaseAudioChnObject(aChnObject *pObj);
    //int32_t delAllAudioChannel();

    
private:
    static Analyzer *m_pSelf;
    
    // 解码器输出数据 - RGB格式
	std::list<vChnObject*> m_VideoChannellist;
	//std::list<aChnObject*> m_MediaAudioChannellist;

	pthread_t mAnalyzeTid;
	pthread_t mDisplayTid;
};

static void *imgAnalyze_thread(void *para)
{
    Analyzer *pSelf = (Analyzer *)para;

    int chnId = 0;
    Mat image;
    ChnResult_t result;
    pSelf->mAnalyzeThreadWorking = true;
    while(1){
        if(!pSelf->mAnalyzeThreadWorking){
            msleep(5);
            break;
        }
        
        if(NULL == pSelf){
            msleep(5);
            break;
        }
        
        vChnObject *pVideoObj = pSelf->getVideoChnObject(chnId);
        if(pVideoObj){
            // 取出待分析图像
            pthread_rwlock_rdlock(&pVideoObj->imgLock);
            pVideoObj->image.copyTo(image);
            pthread_rwlock_unlock(&pVideoObj->imgLock);

            // 此步骤操作会比较耗时，因此在给pVideoObj->chnResult赋值时需要重新判断pVideoObj是否存在
            result = algorithm_process(chnId, image);
        }        
        pVideoObj = pSelf->getVideoChnObject(chnId);
        if(pVideoObj){
            // 其实这里还是有可能会在切(不同分辨率)流时，会导致应用崩溃
            memcpy(&pVideoObj->chnResult, &result, sizeof(ChnResult_t));
        }
        
        chnId++;
        chnId%=pSelf->mMaxChnNum;
        msleep(20);
    }
    
    pthread_exit(NULL);
}

typedef struct {
    DispDev_t dev;
    int connIndex;
    int planeHandle;
    int screenWidth;
    int screenHeight;
    int bufHandle;
    int bufFd;
    void *bufMap;
    size_t bufSize;
} DisplayCtx_t;

static int initDisplay(DisplayCtx_t *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->connIndex = -1;
    ctx->planeHandle = -1;
    ctx->bufHandle = -1;
    ctx->bufFd = -1;

    if (0 != disp_init(&ctx->dev)) {
        PRINT_ERROR(g_log, "disp_init failed\n");
        return -1;
    }

    // 与 display_primary_init 的选择策略一致：第一个可用连接器的 primary 图层
    int planeId = 0;
    for (int i = 0; i < ctx->dev.connectorCount && ctx->connIndex < 0; i++) {
        if (!ctx->dev.connectors[i].enabled)
            continue;
        for (int p = 0; p < ctx->dev.connectors[i].planeCount; p++) {
            if (ctx->dev.connectors[i].planes[p].type == DISPLAY_PLANE_TYPE_PRIMARY) {
                ctx->connIndex = i;
                planeId = ctx->dev.connectors[i].planes[p].id;
                break;
            }
        }
    }
    if (ctx->connIndex < 0) {
        PRINT_ERROR(g_log, "no usable connector with primary plane\n");
        disp_uninit(&ctx->dev);
        return -1;
    }

    ctx->planeHandle = disp_init_planes(ctx->dev.connectors[ctx->connIndex].index, planeId);
    if (ctx->planeHandle < 0) {
        PRINT_ERROR(g_log, "disp_init_planes failed\n");
        disp_uninit(&ctx->dev);
        return -1;
    }

    ctx->screenWidth = ctx->dev.connectors[ctx->connIndex].screenWidth;
    ctx->screenHeight = ctx->dev.connectors[ctx->connIndex].screenHeight;

    ctx->bufSize = (size_t)ctx->screenWidth * ctx->screenHeight * 4;
    if (0 != alloc_dmabuf(ctx->bufSize, &ctx->bufFd, &ctx->bufMap)) {
        PRINT_ERROR(g_log, "alloc_dmabuf failed (%zu bytes)\n", ctx->bufSize);
        disp_uninit_planes(ctx->planeHandle);
        disp_uninit(&ctx->dev);
        return -1;
    }

    /*
     * 显示缓冲区只登记一次并保持存活：display_primary_commit 每帧导入后立即
     * 释放 framebuffer，本平台上会导致图层被摘下（画面不可见），因此直接用
     * display_pro 的常驻导入接口。
     */
    DispImportDesc_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.dmabuf_fd = ctx->bufFd;
    desc.width = ctx->screenWidth;
    desc.height = ctx->screenHeight;
    desc.format = DISP_PIX_FMT_XRGB8888;
    desc.pitches[0] = (uint32_t)ctx->screenWidth * 4;
    ctx->bufHandle = disp_import_dmabuf(&desc);
    if (ctx->bufHandle < 0) {
        PRINT_ERROR(g_log, "disp_import_dmabuf failed\n");
        munmap(ctx->bufMap, ctx->bufSize);
        close(ctx->bufFd);
        disp_uninit_planes(ctx->planeHandle);
        disp_uninit(&ctx->dev);
        return -1;
    }

    disp_plane_set_zpos(ctx->planeHandle, 0);
    disp_plane_set_src_region(ctx->planeHandle, 0, 0, ctx->screenWidth, ctx->screenHeight);
    disp_plane_set_crtc_region(ctx->planeHandle, 0, 0, ctx->screenWidth, ctx->screenHeight);
    return 0;
}

static void releaseDisplay(DisplayCtx_t *ctx)
{
    if (ctx->bufHandle >= 0)
        disp_free_buffer(ctx->bufHandle);
    if (ctx->bufFd >= 0) {
        munmap(ctx->bufMap, ctx->bufSize);
        close(ctx->bufFd);
    }
    if (ctx->planeHandle >= 0)
        disp_uninit_planes(ctx->planeHandle);
    disp_uninit(&ctx->dev);

    memset(ctx, 0, sizeof(*ctx));
    ctx->connIndex = -1;
    ctx->planeHandle = -1;
    ctx->bufHandle = -1;
    ctx->bufFd = -1;
}

static void *imgDisplay_thread(void *para)
{
    Analyzer *pSelf = (Analyzer *)para;

    DisplayCtx_t ctx;
    if (0 != initDisplay(&ctx))
        pthread_exit(NULL);

    // --无信号通道显示内容
    Mat noSignal_img = imread("./noSignal.jpg", 1);

    // 2x2 宫格布局
    const int cols = 2, rows = 2;
    int win_width = ctx.screenWidth / cols;
    int win_height = ctx.screenHeight / rows;
    Mat canvas(ctx.screenHeight, ctx.screenWidth, CV_8UC4, ctx.bufMap);
    Mat rotated, scaled, bgra;
    Mat image;

    pSelf->mDisplayThreadWorking = true;
    while(1){
        if(!pSelf->mDisplayThreadWorking){
            msleep(5);
            break;
        }
        
        if(NULL == pSelf){
            msleep(5);
            break;
        }

        canvas.setTo(Scalar(0, 0, 0, 0));
        for(int chnId = 0; chnId < pSelf->mMaxChnNum; chnId++){
            int win_x = (chnId % cols) * win_width;
            int win_y = (chnId / cols) * win_height;
            if (win_x + win_width > ctx.screenWidth || win_y + win_height > ctx.screenHeight)
                break;

            Rect rect(win_x, win_y, win_width, win_height);
            vChnObject *pVideoObj = pSelf->getVideoChnObject(chnId);
            if(pVideoObj){
                pthread_rwlock_rdlock(&pVideoObj->imgLock);
                pVideoObj->image.copyTo(image);
                ChnResult_t result;
                memset(&result, 0, sizeof(ChnResult_t));
                memcpy(&result, &pVideoObj->chnResult, sizeof(ChnResult_t));
                pthread_rwlock_unlock(&pVideoObj->imgLock);

                paint_algorithm_result(image, result);
                blitToWindow(canvas, image, rect, rotated, scaled, bgra);
            }else if(!noSignal_img.empty()){
                blitToWindow(canvas, noSignal_img, rect, rotated, scaled, bgra);
            }
        }

        dma_sync_cpu_to_device(ctx.bufFd);
        if (0 != disp_commit(ctx.planeHandle, ctx.bufHandle))
            PRINT_ERROR(g_log, "disp_commit failed\n");

        msleep(15);
    }

    releaseDisplay(&ctx);

    pthread_exit(NULL);
}

Analyzer *Analyzer::m_pSelf = NULL;
Analyzer::Analyzer(int32_t maxChn) :
    mAnalyzeThreadWorking(false),
    mDisplayThreadWorking(false),
    mMaxChnNum(maxChn)
{
    //rga_init();
    
    /*初始化通道锁*/
    pthread_mutex_init(&mVideoChnLock, NULL);
    //pthread_mutex_init(&mAudioChnLock, NULL);
    
    /*创建线程*/
    if(0 != CreateJoinThread(imgAnalyze_thread, this, &mAnalyzeTid)){
        return ;
    }
    
    if(0 != CreateJoinThread(imgDisplay_thread, this, &mDisplayTid)){
        return ;
    }
}
Analyzer::~Analyzer()
{
    /*回收线程*/
    // 1，等待取流线程跑起来
    int timeOut_ms = 1000; //设置n(ms)超时，超时就不等了
    while(1){
        if(((true == mDisplayThreadWorking)&&(true == mAnalyzeThreadWorking))||(timeOut_ms <= 0)){
            break;
        }
        timeOut_ms--;
        usleep(1000);
    }
    // 2，退出线程并等待其结束
    mAnalyzeThreadWorking = false;
    // --[等待分析线程结束]--
    while(1) {
        usleep(20*1000);
        int32_t exitCode = pthread_join(mAnalyzeTid, NULL);
        if(0 == exitCode){
            break;
        }else if(0 != exitCode){
            switch (exitCode) {
                case ESRCH:  // 没有找到线程ID
                    PRINT_ERROR(g_log, "imgAnalyze_thread exit: No thread with the given ID was found.\n");
                    break;
                case EINVAL: // 线程不可连接或已经有其他线程在等待它
                    PRINT_ERROR(g_log, "imgAnalyze_thread exit: Thread is detached or already being waited on.\n");
                    break;
                case EDEADLK: // 死锁 - 线程尝试join自己
                    PRINT_ERROR(g_log, "imgAnalyze_thread exit: Deadlock detected - thread is trying to join itself.\n");
                    break;
            }
            continue;
        }
    }
    mDisplayThreadWorking = false;
    // --[等待显示线程结束]--
    while(1) {
        usleep(20*1000);
        int32_t exitCode = pthread_join(mDisplayTid, NULL);
        if(0 == exitCode){
            break;
        }else if(0 != exitCode){
            switch (exitCode) {
                case ESRCH:  // 没有找到线程ID
                    PRINT_ERROR(g_log, "imgDisplay_thread exit: No thread with the given ID was found.\n");
                    break;
                case EINVAL: // 线程不可连接或已经有其他线程在等待它
                    PRINT_ERROR(g_log, "imgDisplay_thread exit: Thread is detached or already being waited on.\n");
                    break;
                case EDEADLK: // 死锁 - 线程尝试join自己
                    PRINT_ERROR(g_log, "imgDisplay_thread exit: Deadlock detected - thread is trying to join itself.\n");
                    break;
            }
            continue;
        }
    }

    /*回收视频资源*/
    delAllVideoChannel();
    pthread_mutex_destroy(&mVideoChnLock);

    /*回收音频资源*/
    //delAllAudioChannel();
    //pthread_mutex_destroy(&mAudioChnLock);
    
    //rga_unInit();
}
void Analyzer::createAnalyzer(int32_t maxChn)
{
    if(m_pSelf == NULL) {
        m_pSelf = new Analyzer(maxChn);
   }
}

int32_t Analyzer::upDateVideoChannel(int chnId, char *imgData, ImgDesc_t imgDesc)
{
    if(chnId < 0)
        return -1;

    pthread_mutex_lock(&mVideoChnLock);
    vChnObject* targetObj = nullptr;
    for (auto it = m_VideoChannellist.begin(); it != m_VideoChannellist.end(); ++it) {
        // 找到目标对象
        if ((*it)->chnId == chnId) {
            targetObj = *it;  
            
            // 图像信息改变，销毁原来图像缓存
            if((targetObj->image.cols != imgDesc.width)||(targetObj->image.rows != imgDesc.height)){
                if(0 == releaseVideoChnObject(targetObj)){
                    // 从链表中移除chnObj
                    it = m_VideoChannellist.erase(it);
                }else{
                    pthread_mutex_unlock(&mVideoChnLock);
                    return -2;
                }
            }
            
            break;
        }
    }
    
    // 需要创建一个[视频]通道对象
    if (!targetObj) {
        targetObj = createVideoChnObject(chnId, imgDesc.width, imgDesc.height);
        if(!targetObj)
            return -3;
        
        m_VideoChannellist.push_back(targetObj);
    }
    pthread_mutex_unlock(&mVideoChnLock);

    // 更新[视频]通道图像数据
    Image srcImage, dstImage;
    memset(&srcImage, 0, sizeof(srcImage));
    memset(&dstImage, 0, sizeof(dstImage));
    
    srcImage.fmt = rgaFmt(imgDesc.fmt);
    srcImage.width = imgDesc.width;
    srcImage.height = imgDesc.height;
    srcImage.hor_stride = imgDesc.horStride;
    srcImage.ver_stride = imgDesc.verStride;
    srcImage.rotation = HAL_TRANSFORM_ROT_0;
    srcImage.fd = -1;
    srcImage.pBuf = imgData;
    
    dstImage.fmt = RK_FORMAT_BGR_888;
    dstImage.width = targetObj->image.cols;
    dstImage.height = targetObj->image.rows;
    dstImage.hor_stride = targetObj->image.cols;
    dstImage.ver_stride = targetObj->image.rows;
    dstImage.rotation = HAL_TRANSFORM_ROT_0;
    dstImage.fd = targetObj->dma.fd;
    dstImage.pBuf = (void *)targetObj->dma.pBuffer;
    
    pthread_rwlock_wrlock(&targetObj->imgLock);
    srcImg_ConvertTo_dstImg(&dstImage, &srcImage);
    pthread_rwlock_unlock(&targetObj->imgLock);
    return 0;
}

vChnObject *Analyzer::getVideoChnObject(int chnId)
{
    if(chnId < 0)
        return NULL;

    vChnObject* targetObj = nullptr;
    pthread_mutex_lock(&mVideoChnLock);
    for (auto it = m_VideoChannellist.begin(); it != m_VideoChannellist.end(); ++it) {
        // 找到目标对象
        if ((*it)->chnId == chnId) {
            targetObj = *it;
            break;
        }
    }
    pthread_mutex_unlock(&mVideoChnLock);

    return targetObj;
}


vChnObject *Analyzer::createVideoChnObject(int32_t chnId, int32_t imgWidth, int32_t imgHeight)
{
    vChnObject *newChnObj = new vChnObject;
    if (!newChnObj) {
        return NULL;
    }

    pthread_rwlock_init(&newChnObj->imgLock, nullptr);

    newChnObj->dma.pBuffer = NULL;
    newChnObj->dma.fd = -1;
    newChnObj->dma.size = imgWidth * imgHeight * 3;

    if (alloc_dmabuf((size_t)newChnObj->dma.size, &newChnObj->dma.fd, &newChnObj->dma.pBuffer) != 0) {
        PRINT_ERROR(g_log, "alloc_dmabuf failed (%d bytes)\n", newChnObj->dma.size);
        pthread_rwlock_destroy(&newChnObj->imgLock);
        delete newChnObj;
        return NULL;
    }

    newChnObj->chnId = chnId;
    /* Mat 复用 dma-buf mmap 内存，与 RGA importbuffer_fd 等一致 */
    newChnObj->image = Mat(imgHeight, imgWidth, CV_8UC3, newChnObj->dma.pBuffer);
    newChnObj->image.setTo(Scalar(0, 255, 0));

    memset(&newChnObj->chnResult, 0, sizeof(ChnResult_t));
    return newChnObj;
}


int32_t Analyzer::releaseVideoChnObject(vChnObject *pObj)
{
    if(NULL == pObj)
        return -1;
    
    // 1. 销毁Mat资源（OpenCV会自动管理）和dma内存
    pthread_rwlock_wrlock(&pObj->imgLock);
    pObj->image.release();
    if (pObj->dma.pBuffer && pObj->dma.size > 0) {
        munmap(pObj->dma.pBuffer, pObj->dma.size);
        pObj->dma.pBuffer = NULL;
        pObj->dma.size = 0;
        close(pObj->dma.fd);
    }
    pthread_rwlock_unlock(&pObj->imgLock);
    
    // 2. 销毁读写锁
    pthread_rwlock_destroy(&pObj->imgLock);
    
    // 3. 销毁通道对象
    delete pObj;
    
    return 0;
}

int32_t Analyzer::delAllVideoChannel()
{
    pthread_mutex_lock(&mVideoChnLock);
    for (auto it = m_VideoChannellist.begin(); it != m_VideoChannellist.end(); ++it) {
        if(0 == releaseVideoChnObject(*it)){
            it = m_VideoChannellist.erase(it);
        }
    }
    pthread_mutex_unlock(&mVideoChnLock);
    return 0;
}


int analyzer_init(int32_t maxChn)
{
    // 创建图像分析器
    Analyzer::createAnalyzer(maxChn);
    
    // 模型初始化
    algorithm_init();

    return 0;
}

void analyzer_exit()
{
    Analyzer *pAnalyzer = Analyzer::instance();
    if(pAnalyzer){
        delete pAnalyzer;
    }
    
    // 模型初始化
    algorithm_unInit();
}

int videoOutHandle(char *imgData, ImgDesc_t imgDesc)
{
    Analyzer *pAnalyzer = Analyzer::instance();

    if(pAnalyzer){
        pAnalyzer->upDateVideoChannel(imgDesc.chnId, imgData, imgDesc);
    }
    
    return 0;
}

