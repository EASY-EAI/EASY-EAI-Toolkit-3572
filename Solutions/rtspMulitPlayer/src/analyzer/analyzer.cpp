//=====================  C++  =====================
#include <string>
#include <list>
//=====================   C   =====================
#include <sys/mman.h>

#include "system.h"
//=====================  PRJ  =====================
#include "system_opt.h"
#include "rga_wrapper.h"
#include "rga_util.h"
#include "display.h"

#include "analyzer.h"

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
    int convFd;        // 格式转换中间缓冲（BGRX8888）
    void *convBuf;
    size_t convSize;
    int dispFd;        // 送显缓冲（BGRX8888，已旋转）
    void *dispBuf;
    size_t dispSize;
    int width;         // 当前缓冲对应的源图尺寸
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
    // 270 度旋转后宽高互换
    buf->dispSize = (size_t)height * width * 4;
    if (0 != alloc_dmabuf(buf->dispSize, &buf->dispFd, &buf->dispBuf)) {
        releaseFrameBuf(buf);
        return -1;
    }
    buf->width = width;
    buf->height = height;
    return 0;
}

static int commitFrameToDisplay(Mat &frame, DisplayFrameBuf_t *buf)
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

    // 缩放由显示层完成：整帧 -> 整屏
    return display_primary_commit(buf->dispFd, dispFrame.width, dispFrame.height,
                                  DISPLAY_FMT_XRGB8888, dispFrame.wstride * 4);
}

static void *imgDisplay_thread(void *para)
{
    Analyzer *pSelf = (Analyzer *)para;

    if (0 != display_primary_init()) {
        fprintf(stderr, "display_primary_init failed\n");
        pthread_exit(NULL);
    }

    // --无信号通道显示内容
    bool bShowNoSig = true;
    Mat noSignal_img = imread("./noSignal.jpg", 1);
    // --有信号通道显示设置
    int videoDuration = 10;//秒
    int preTimeStamp = get_timeval_ms();
    int curTimeStamp = preTimeStamp;

    int chnId = 0;
    Mat image;
    ChnResult_t result;
    DisplayFrameBuf_t frameBuf;
    memset(&frameBuf, 0, sizeof(frameBuf));
    frameBuf.convFd = -1;
    frameBuf.dispFd = -1;

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

        // 每隔videoDuration秒切换一次通道
        curTimeStamp = get_timeval_ms();
        if(videoDuration*1000 <= (curTimeStamp-preTimeStamp)){
            chnId++;
            chnId%=pSelf->mMaxChnNum;

            preTimeStamp = curTimeStamp;
        }

        vChnObject *pVideoObj = pSelf->getVideoChnObject(chnId);
        if(pVideoObj){
            pthread_rwlock_rdlock(&pVideoObj->imgLock);
            pVideoObj->image.copyTo(image);
            memset(&result, 0, sizeof(ChnResult_t));
            memcpy(&result, &pVideoObj->chnResult, sizeof(ChnResult_t));
            pthread_rwlock_unlock(&pVideoObj->imgLock);

            // 绘制分析结果到待显示图像
            paint_algorithm_result(image, result);

            if (0 != commitFrameToDisplay(image, &frameBuf))
                fprintf(stderr, "commit channel %d failed\n", chnId);
            bShowNoSig = true;

        }else if(bShowNoSig){
            if (0 != commitFrameToDisplay(noSignal_img, &frameBuf))
                fprintf(stderr, "commit no-signal image failed\n");
            bShowNoSig = false;
        }

        msleep(15);
    }

    releaseFrameBuf(&frameBuf);
    display_primary_uninit();
    pthread_exit(NULL);
}

Analyzer *Analyzer::m_pSelf = NULL;
Analyzer::Analyzer(int32_t maxChn) :
    mAnalyzeThreadWorking(false),
    mDisplayThreadWorking(false),
    mMaxChnNum(maxChn)
{
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
                    fprintf(stderr, "imgAnalyze_thread exit: No thread with the given ID was found.\n");
                    break;
                case EINVAL: // 线程不可连接或已经有其他线程在等待它
                    fprintf(stderr, "imgAnalyze_thread exit: Thread is detached or already being waited on.\n");
                    break;
                case EDEADLK: // 死锁 - 线程尝试join自己
                    fprintf(stderr, "imgAnalyze_thread exit: Deadlock detected - thread is trying to join itself.\n");
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
                    fprintf(stderr, "imgDisplay_thread exit: No thread with the given ID was found.\n");
                    break;
                case EINVAL: // 线程不可连接或已经有其他线程在等待它
                    fprintf(stderr, "imgDisplay_thread exit: Thread is detached or already being waited on.\n");
                    break;
                case EDEADLK: // 死锁 - 线程尝试join自己
                    fprintf(stderr, "imgDisplay_thread exit: Deadlock detected - thread is trying to join itself.\n");
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
    srcImage.pBuf = imgData;
    
    dstImage.fmt = RK_FORMAT_BGR_888;
    dstImage.width = targetObj->image.cols;
    dstImage.height = targetObj->image.rows;
    dstImage.hor_stride = targetObj->image.cols;
    dstImage.ver_stride = targetObj->image.rows;
    dstImage.rotation = HAL_TRANSFORM_ROT_0;
    dstImage.pBuf = (void *)targetObj->image.data;
    
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
    // 1. 创建通道对象
    vChnObject* newChnObj = new vChnObject;
    if(!newChnObj)
        return NULL;
    
    // 2. 初始化图像数据读写锁
    pthread_rwlock_init(&newChnObj->imgLock, nullptr);
    
    // 3. 创建图像缓存
    newChnObj->chnId = chnId;
    newChnObj->image = Mat(imgHeight, imgWidth, CV_8UC3, Scalar(0, 255, 0));
    memset(&newChnObj->chnResult, 0, sizeof(ChnResult_t));
    
    return newChnObj;
}


int32_t Analyzer::releaseVideoChnObject(vChnObject *pObj)
{
    if(NULL == pObj)
        return -1;
    
    // 1. 销毁Mat资源（OpenCV会自动管理）
    pthread_rwlock_wrlock(&pObj->imgLock);
    pObj->image.release();
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
    // 初始化 RGA 会话（送显前的格式转换与旋转）
    rga_init(RGA_LOG_MGR_INVALID);

    // 创建图像分析器
    Analyzer::createAnalyzer(maxChn);
    
    // 模型初始化
    algorithm_init();

    return 0;
}

int videoOutHandle(char *imgData, ImgDesc_t imgDesc)
{
    Analyzer *pAnalyzer = Analyzer::instance();

    if(pAnalyzer){
        pAnalyzer->upDateVideoChannel(imgDesc.chnId, imgData, imgDesc);
    }
    
    return 0;
}

