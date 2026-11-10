#ifndef FIRE_DETECT_H
#define FIRE_DETECT_H

#include "postprocess.h"
#include "rknn3_api.h"
#include <opencv2/opencv.hpp>

typedef struct {
    rknn3_context rknn_ctx;         // rknn3 上下文句柄
    rknn3_input_output_num io_num;  // 输入输出张量数量
    rknn3_tensor *inputs;           // 输入张量数组
    rknn3_tensor *outputs;          // 输出张量数组
    int model_input_height;         // 模型输入高度
    int model_input_width;          // 模型输入宽度
} fire_detect_ctx;

/**
 * @brief 火焰初始化函数
 * @param ctx 输入参数,fire_detect_ctx句柄
 * @param model_path 输入参数,算法模型路径
 * @return 0表示成功，非0表示失败
 */
int fire_detect_init(fire_detect_ctx *ctx, const char *model_path);

/**
 * @brief 火焰检测运行函数
 * @param ctx 输入参数,fire_detect_ctx句柄
 * @param input_image 输入参数,输入图像
 * @param detect_result_group 输出参数,检测结果
 * @return 0表示成功，非0表示失败
 */
int fire_detect_run(fire_detect_ctx *ctx, cv::Mat input_image, detect_result_group_t *detect_result_group);

/**
 * @brief 火焰检测释放函数
 * @param ctx 输入参数,fire_detect_ctx句柄
 * @return 0表示成功，非0表示失败
 */
int fire_detect_release(fire_detect_ctx *ctx);

#endif  // FIRE_DETECT_H
