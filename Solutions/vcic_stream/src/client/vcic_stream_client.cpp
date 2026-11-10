/**
 * SOME/IP + AVTP 融合方案 —— Client 端（接收端 / 显示端）
 *
 * 使用 VCIC 统一接口（ISO 17215）实现：
 *   - VCIC_client_create()           创建客户端
 *   - VCIC_client_subscribe_event()  订阅事件通知
 *   - VCIC_client_start_stream()     请求取流
 *   - VCIC_client_stop_stream()       停止取流
 *
 * 数据面：VCIC 帧回调 → GStreamer/MPP 解码 → NV12 DMA-BUF → display zero-copy 显示
 *
 * 用法：
 *   sudo ./vcic_stream_client <server_ip> [ifname] [local_ip] [fmt]
 * 示例：
 *   sudo ./vcic_stream_client 192.168.1.20 eth0 192.168.1.30 h264
 *
 * 需要权限：CAP_NET_RAW（AVTP 使用裸以太网 socket）
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <vector>
#include <pthread.h>

#include "log_manager_pro.h"
#include "vcic.h"
#include "gstDecoder.h"
#include "display.h"          /* display_primary_init, display_primary_commit, display_primary_uninit */

/* ======================== 日志句柄 ======================== */
static log_mgr_t    g_logMgr   = LOG_MGR_INVALID;
static log_handle_t h_client   = LOG_HANDLE_INVALID;
static log_handle_t h_dec      = LOG_HANDLE_INVALID;
static log_handle_t h_display  = LOG_HANDLE_INVALID;

/* ======================== 全局状态 ======================== */
static volatile sig_atomic_t g_running = 1;
/* 是否已进入关闭流程（用于二次 Ctrl+C 强制退出） */
static volatile sig_atomic_t g_shuttingDown = 0;

/* AVTP / VCIC 配置（必须与 Server 一致） */
static const uint8_t STREAM_ID[8] = {0x00, 0x1B, 0x21, 0xFF, 0xFE, 0x00, 0x00, 0x01};
static const uint8_t MCAST_MAC[6] = {0x91, 0xE0, 0xF0, 0x00, 0x0E, 0x80};

/* 帧重组缓冲区 */
static std::vector<uint8_t> g_reassemblyBuf;
static pthread_mutex_t g_reassemblyMutex = PTHREAD_MUTEX_INITIALIZER;

/* GstDecoder 解码器 */
static GstDecoder *g_pDecoder = NULL;

/* VCIC Client 实例 */
static VCIC_Client *g_vcicClient = NULL;

/* 统计计数 */
static uint32_t g_frag_cnt  = 0;
static uint32_t g_byte_cnt  = 0;
static uint32_t g_frame_cnt = 0;
static uint32_t g_disp_cnt  = 0;

/* ======================== 信号处理 ======================== */
/* 第一次信号 → 优雅关闭（设置 g_running=0）
 * 第二次信号 → 强制退出（说明清理阶段卡死，直接 _exit） */
static void on_signal(int sig)
{
    (void)sig;
    if (g_shuttingDown) {
        /* 已经在关闭流程中，再次收到信号 → 强制退出 */
        _exit(128 + sig);
    }
    g_shuttingDown = 1;
    g_running = 0;
}

/* ======================== 解码回调 → zero-copy 屏幕显示 ======================== */
/* 直接把解码输出的 NV12 DMA-BUF 提交到 primary 图层，
 * 回调返回后解码器即可复用该缓冲。 */
static void onDecoded(int dmabuf_fd, int width, int height,
                      int hor_stride, int ver_stride, void *userData)
{
    (void)userData;
    (void)ver_stride;

    if (dmabuf_fd < 0 || width <= 0 || height <= 0) {
        return;
    }

    int ret = display_primary_commit(dmabuf_fd, width, height,
                                     DISPLAY_FMT_NV12, hor_stride);
    if (ret != 0) {
        if ((g_disp_cnt % 300) == 0) {
            PRINT_ERROR(h_display, "display_primary_commit failed ret=%d (%dx%d fd=%d pitch=%d)\n",
                        ret, width, height, dmabuf_fd, hor_stride);
        }
        return;
    }

    g_disp_cnt++;
    PRINT_DEBUG(h_dec, "onDecoded: frame=%u %dx%d fd=%d\n",
               g_disp_cnt, width, height, dmabuf_fd);
    if ((g_disp_cnt % 300) == 1) {
        PRINT_INFO(h_client, "[client]: displayed frame %u (%dx%d fd=%d)\n",
                   g_disp_cnt, width, height, dmabuf_fd);
    }
}

/* ======================== VCIC 帧回调 → 重组 → 解码 ======================== */
static int32_t on_vcic_frame(void *pObj, const uint8_t *data, uint32_t len,
                              uint32_t avtp_ts, uint8_t isLastFrag, uint8_t codec)
{
    (void)pObj;
    (void)avtp_ts;
    (void)codec;

    g_frag_cnt++;
    g_byte_cnt += len;

    pthread_mutex_lock(&g_reassemblyMutex);

    /* 追加分片到重组缓冲区 */
    if (data && len > 0) {
        g_reassemblyBuf.insert(g_reassemblyBuf.end(), data, data + len);
    }

    /* 收到最后一帧分片 → 重组完成，推送解码 */
    if (isLastFrag) {
        g_frame_cnt++;

        if (g_pDecoder && g_pDecoder->isRunning() && !g_reassemblyBuf.empty()) {
            g_pDecoder->pushData(g_reassemblyBuf.data(),
                                 (uint32_t)g_reassemblyBuf.size());
        }

        g_reassemblyBuf.clear();
    }

    pthread_mutex_unlock(&g_reassemblyMutex);

    /* 每 30 个分片打印一次统计 */
    if ((g_frag_cnt % 30) == 1) {
        PRINT_DEBUG(h_client, "on_vcic_frame: frag#%u len=%u ts=0x%08x last=%u | "
               "frames=%u bytes=%u displayed=%u\n",
               g_frag_cnt, len, avtp_ts, isLastFrag,
               g_frame_cnt, g_byte_cnt, g_disp_cnt);
    }

    return 0;
}

/* ======================== VCIC 事件回调 ======================== */
static int32_t on_cam_status(void *pObj, uint16_t eventId,
                             const uint8_t *data, uint32_t len)
{
    (void)pObj;
    if (len >= 2) {
        PRINT_DEBUG(h_client, "[client]: event CAM_STATUS(0x%04x) len=%u streaming=%u status=0x%02x\n",
                    eventId, len, data[0], data[1]);
    } else {
        PRINT_DEBUG(h_client, "[client]: event CAM_STATUS(0x%04x) len=%u\n", eventId, len);
    }
    return 0;
}

static int32_t on_frame_lost(void *pObj, uint16_t eventId,
                             const uint8_t *data, uint32_t len)
{
    (void)pObj;
    if (data && len > 0) {
        char hexbuf[64] = {0};
        int pos = 0;
        for (uint32_t i = 0; i < len && i < 16; i++) {
            pos += snprintf(hexbuf + pos, sizeof(hexbuf) - pos, " %02x", data[i]);
        }
        PRINT_ERROR(h_client, "[client]: event FRAME_LOST(0x%04x) len=%u raw:%s\n", eventId, len, hexbuf);
    } else {
        PRINT_ERROR(h_client, "[client]: event FRAME_LOST(0x%04x) len=%u\n", eventId, len);
    }

    /* 丢帧时清空重组缓冲区，避免残留数据导致解码错误 */
    pthread_mutex_lock(&g_reassemblyMutex);
    g_reassemblyBuf.clear();
    pthread_mutex_unlock(&g_reassemblyMutex);

    return 0;
}

/* ======================== main ======================== */
int main(int argc, char *argv[])
{
    const char *server_ip = NULL;
    const char *ifname    = "eth0";
    const char *local_ip  = "";
    const char *fmt       = "h264";

    if (argc < 2) {
        printf("Usage: %s <server_ip> [ifname] [local_ip] [fmt]\n", argv[0]);
        printf("  server_ip : VCIC server address\n");
        printf("  ifname    : network interface for AVTP (default eth0)\n");
        printf("  local_ip  : optional local bind IP (default any)\n");
        printf("  fmt       : h264|h265 (default h264, must match server)\n");
        return -1;
    }
    server_ip = argv[1];
    if (argc >= 3) ifname = argv[2];
    if (argc >= 4) local_ip = argv[3];
    if (argc >= 5) fmt = argv[4];

    /* 确保 stdout/stderr 无缓冲，便于调试 */
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    /* 初始化日志系统 */
    g_logMgr = log_manager_init("/userdata/logs/vcic_stream_client.ini");
    h_client  = log_register(g_logMgr, "client");
    h_dec     = log_register(g_logMgr, "dec");
    h_display = log_register(g_logMgr, "display");

    /* 使用 sigaction 替代 signal，不设 SA_RESTART 以确保中断系统调用 */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;  /* 不设 SA_RESTART，让 poll/sleep 被信号中断 */
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    /* 屏蔽 SIGINT/SIGTERM，使后续子线程继承屏蔽，
     * 信号只投递到主线程 */
    sigset_t blockMask;
    sigemptyset(&blockMask);
    sigaddset(&blockMask, SIGINT);
    sigaddset(&blockMask, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &blockMask, NULL);

    /* ---------------- 初始化屏幕显示（新 display 接口，支持 zero-copy NV12） ---------------- */
    fprintf(stderr, "[client]: initializing display...\n");
    PRINT_INFO(h_client, "[client]: initializing display...\n");

    if (display_primary_init() != 0) {
        fprintf(stderr, "[client]: display_primary_init failed\n");
        PRINT_ERROR(h_client, "[client]: display_primary_init failed\n");
        return -1;
    }
    fprintf(stderr, "[client]: display initialized\n");

    /* ---------------- 初始化 GStreamer/MPP 解码器 ---------------- */
    fprintf(stderr, "[client]: initializing GStreamer MPP decoder (%s)...\n", fmt);
    PRINT_INFO(h_client, "[client]: initializing GStreamer MPP decoder (%s)...\n", fmt);
    g_pDecoder = new GstDecoder(fmt);
    g_pDecoder->setDecodedCallback(onDecoded, NULL);
    if (g_pDecoder->start() != 0) {
        fprintf(stderr, "[client]: GstDecoder start failed\n");
        PRINT_ERROR(h_client, "[client]: GstDecoder start failed\n");
        delete g_pDecoder;
        g_pDecoder = NULL;
        display_primary_uninit();
        return -1;
    }
    fprintf(stderr, "[client]: GstDecoder started OK\n");

    /* ---------------- 创建 VCIC Client ---------------- */
    VCIC_ClientConfig_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    strncpy(cfg.ifname, ifname, sizeof(cfg.ifname) - 1);
    if (local_ip[0] != '\0')
        strncpy(cfg.local_ip, local_ip, sizeof(cfg.local_ip) - 1);
    cfg.service_id   = VCIC_SERVICE_ID;
    cfg.instance_id  = VCIC_INSTANCE_ID;
    cfg.local_port   = 30492;  /* 客户端本地端口，避免与 server 冲突 */
    strncpy(cfg.server_ip, server_ip, sizeof(cfg.server_ip) - 1);
    cfg.server_port  = VCIC_APP_PORT;
    memcpy(cfg.stream_id, STREAM_ID, 8);
    memcpy(cfg.multicast_mac, MCAST_MAC, 6);

    fprintf(stderr, "[client]: creating VCIC client (server=%s:%u if=%s local=%s)...\n",
            server_ip, VCIC_APP_PORT, ifname, local_ip[0] ? local_ip : "auto");
    g_vcicClient = VCIC_client_create(&cfg);
    if (!g_vcicClient) {
        fprintf(stderr, "[client]: VCIC_client_create failed\n");
        PRINT_ERROR(h_client, "[client]: VCIC_client_create failed\n");
        goto cleanup_decoder;
    }
    fprintf(stderr, "[client]: VCIC client created OK\n");

    /* 订阅事件 */
    VCIC_client_subscribe_event(g_vcicClient, NULL, VCIC_EVENT_CAM_STATUS, on_cam_status);
    VCIC_client_subscribe_event(g_vcicClient, NULL, VCIC_EVENT_FRAME_LOST, on_frame_lost);

    PRINT_INFO(h_client, "[client]: VCIC Client connected to %s:%u\n", server_ip, VCIC_APP_PORT);

    /* 启动取流 */
    fprintf(stderr, "[client]: starting stream...\n");
    if (VCIC_client_start_stream(g_vcicClient, NULL, on_vcic_frame) != 0) {
        fprintf(stderr, "[client]: VCIC_client_start_stream failed\n");
        PRINT_ERROR(h_client, "[client]: VCIC_client_start_stream failed\n");
        goto cleanup_vcic;
    }

    fprintf(stderr, "[client]: stream started, waiting for video frames...\n\n");
    PRINT_INFO(h_client, "[client]: stream started, waiting for video frames...\n\n");

    /* 主线程解除屏蔽 —— 确保信号只投递到主线程 */
    pthread_sigmask(SIG_UNBLOCK, &blockMask, NULL);

    /* ---------------- 主循环 ---------------- */
    while (g_running) {
        /* 用 poll 替代 sleep，信号到达时立即返回 */
        poll(NULL, 0, 2000);
        if (g_frame_cnt > 0) {
            PRINT_INFO(h_client, "[client]: stats: frags=%u frames=%u bytes=%u displayed=%u\n",
                   g_frag_cnt, g_frame_cnt, g_byte_cnt, g_disp_cnt);
            PRINT_DEBUG(h_client, "main loop alive: frags=%u frames=%u bytes=%u displayed=%u\n",
                   g_frag_cnt, g_frame_cnt, g_byte_cnt, g_disp_cnt);
        } else {
            PRINT_DEBUG(h_client, "main loop alive: waiting for frames...\n");
        }
    }

    /* ---------------- 清理 ---------------- */
    fprintf(stderr, "[client]: shutting down... (再按 Ctrl+C 可强制退出)\n");
    PRINT_INFO(h_client, "[client]: shutting down...\n");
    VCIC_client_stop_stream(g_vcicClient);

cleanup_vcic:
    if (g_vcicClient) {
        VCIC_client_unsubscribe_event(g_vcicClient, VCIC_EVENT_CAM_STATUS);
        VCIC_client_unsubscribe_event(g_vcicClient, VCIC_EVENT_FRAME_LOST);
        VCIC_client_destroy(g_vcicClient);
        g_vcicClient = NULL;
    }
    fprintf(stderr, "[client]: VCIC Client stopped\n");
    PRINT_INFO(h_client, "[client]: VCIC Client stopped\n");

cleanup_decoder:
    if (g_pDecoder) {
        g_pDecoder->stop();
        delete g_pDecoder;
        g_pDecoder = NULL;
    }

    display_primary_uninit();
    fprintf(stderr, "[client]: display released\n");
    PRINT_INFO(h_client, "[client]: display released\n");

    return 0;
}
