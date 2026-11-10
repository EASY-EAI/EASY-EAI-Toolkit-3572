#ifndef HELMET_DETECT_H
#define HELMET_DETECT_H

#include <opencv2/opencv.hpp>
#include "rknn3_api.h"
#include "postprocess.h"

typedef struct {
    rknn3_context rknn_ctx;         // rknn3 上下文句柄
    rknn3_input_output_num io_num;  // 输入输出张量数量
    rknn3_tensor *inputs;           // 输入张量数组
    rknn3_tensor *outputs;          // 输出张量数组
    int model_input_height;         // 模型输入高度
    int model_input_width;          // 模型输入宽度
} helmet_detect_ctx;

/**
 * @brief 安全帽检测初始化函数
 * @param ctx 输入参数, helmet_detect_ctx句柄
 * @param model_path 输入参数, 算法模型路径
 * @return 返回值, 0表示成功, 非0表示失败
 */
int helmet_detect_init(helmet_detect_ctx *ctx, const char *model_path);

/**
 * @brief 安全帽检测执行函数
 * @param ctx 输入参数, helmet_detect_ctx句柄
 * @param input_image 输入参数, 图像数据输入(cv::Mat是Opencv的类型)
 * @param detect_result_group 输出参数, 目标检测框输出
 * @return 返回值, 0表示成功, 非0表示失败
 */
int helmet_detect_run(helmet_detect_ctx *ctx, cv::Mat input_image, detect_result_group_t *detect_result_group);

/**
 * @brief 安全帽检测释放函数
 * @param ctx 输入参数, helmet_detect_ctx句柄
 * @return 返回值, 0表示成功, 非0表示失败
 */
int helmet_detect_release(helmet_detect_ctx *ctx);

#endif  // HELMET_DETECT_H
