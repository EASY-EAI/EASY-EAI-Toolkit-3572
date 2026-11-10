#ifndef BSD_H
#define BSD_H

#include "bsd_postprocess.h"
#include "rknn3_api.h"
#include <opencv2/opencv.hpp>

typedef struct {
    rknn3_context rknn_ctx;         // rknn3 上下文句柄
    rknn3_input_output_num io_num;  // 输入输出张量数量
    rknn3_tensor *inputs;           // 输入张量数组
    rknn3_tensor *outputs;          // 输出张量数组
    int model_height;
    int model_width;
    int channel;
} bsd_ctx;

/*
 * bsd初始化函数
 * ctx:输入参数,rknn_context句柄
 * path:输入参数,算法模型路径
 */
int bsd_init(bsd_ctx *ctx, const char *path);

/*
 * bsd执行函数
 * ctx:输入参数,rknn_context句柄
 * input_image:输入参数,图像数据输入(cv::Mat是Opencv的类型)
 * output_dets:输出参数，目标检测框输出
 */
int bsd_run(bsd_ctx *ctx, cv::Mat input_image, detect_result_group_t *detect_result_group);

/*
 * bsd释放函数
 * ctx:输入参数,rknn_context句柄
 */
int bsd_release(bsd_ctx *ctx);

#endif
