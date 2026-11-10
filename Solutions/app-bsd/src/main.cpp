//=====================  C++  =====================
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
//======================  C  ======================
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
//=====================  PRJ  =====================
#include "capturer/capturer.h"
#include "analyzer/analyzer.h"
#include "log_manager_pro.h"

/* 进程级日志句柄，各模块通过 PRINT_* 宏使用 */
log_handle_t g_log = LOG_HANDLE_INVALID;

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
static const SrcCfg_t SrcCfg_tab[] = {
	{
		.srcType   = "AHD",
		.loaction  = "1",
		.videoEncType = "NV12",
		.audioEncType = "null",
	}, {
		.srcType   = "AHD",
		.loaction  = "2",
		.videoEncType = "NV12",
		.audioEncType = "null",
	}, {
		.srcType   = "AHD",
		.loaction  = "3",
		.videoEncType = "NV12",
		.audioEncType = "null",
	}, {
		.srcType   = "AHD",
		.loaction  = "4",
		.videoEncType = "NV12",
		.audioEncType = "null",
/* 目前demo仅适配了4路，另外4路注释不要打开
	}, {
		.srcType   = "AHD",
		.loaction  = "12",
		.videoEncType = "NV12",
		.audioEncType = "null",
	}, {
		.srcType   = "AHD",
		.loaction  = "13",
		.videoEncType = "NV12",
		.audioEncType = "null",
	}, {
		.srcType   = "AHD",
		.loaction  = "14",
		.videoEncType = "NV12",
		.audioEncType = "null",
	}, {
		.srcType   = "AHD",
		.loaction  = "15",
		.videoEncType = "NV12",
		.audioEncType = "null",
*/
    }
};



static volatile sig_atomic_t g_exitRequested = 0;
static void handleSigInt(int signo)
{
    (void)signo;
    g_exitRequested = 1;
}
int main(int argc, char **argv)
{
#if 0
    struct sigaction act={0};
    sigemptyset(&act.sa_mask);
    act.sa_handler = handleSigInt;
    act.sa_flags = 0;
    if (sigaction(SIGINT, &act, NULL) != 0) {
        perror("sigaction(SIGINT)");
        return -1;
    }
#endif
    
    int ret = -1;
    int chnNums = ARRAY_SIZE(SrcCfg_tab);
    if(chnNums <= 0){
        return -1;
    }

    /* 初始化日志系统 */
    log_mgr_t logMgr = log_manager_init("/userdata/logs/app_bsd.ini");
    g_log = log_register(logMgr, "app");

    /* Initialize algotithm model */
    ret = analyzer_init(chnNums);
    if(0 != ret){
        PRINT_ERROR(g_log, "Initialize algotithm model faild ! ret = %d\n", ret);
        return ret;
    }

    
    Capturer *pCapturer[32] = {NULL};
    for(int i = 0; i <chnNums; i++) {
        pCapturer[i] = new Capturer(i, SrcCfg_tab[i]);
        if(pCapturer[i]){
            if(0 != pCapturer[i]->init()){
                PRINT_ERROR(g_log, "playChn[%d] init faild\n", i);
                delete pCapturer[i];
                pCapturer[i] = NULL;
            }
        }
    }

    while (!g_exitRequested) {
        sleep(1);
    }
    PRINT_INFO(g_log, "SIGINT received, shutting down...\n");

    for(int i = 0; i <chnNums; i++) {
        if(pCapturer[i]){
            delete pCapturer[i];
            pCapturer[i] = NULL;
        }
    }

    analyzer_exit();

    //sleep(3);

    return 0;
}

