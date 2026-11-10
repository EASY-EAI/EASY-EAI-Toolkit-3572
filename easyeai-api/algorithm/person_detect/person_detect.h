#ifndef PERSON_DETECT_H
#define PERSON_DETECT_H

#include "rknn3_api.h"

#include <opencv2/opencv.hpp>

#include "postprocess.h"

/**
 * @brief 人员检测上下文结构体
 * @details 该结构体用于存储人员检测的上下文信息，由 person_detect_init 填充，
 *          其内部资源由 person_detect_release 释放。
 */
typedef struct {
    rknn3_context rknn_ctx;         // rknn3 上下文句柄
    rknn3_input_output_num io_num;  // 输入输出张量数量
    rknn3_tensor *inputs;           // 输入张量数组
    rknn3_tensor *outputs;          // 输出张量数组
} person_detect_ctx;

/**
 * @brief 人员检测初始化函数
 * @details 该函数用于初始化人员检测上下文，包括创建 rknn3 上下文和分配输入输出张量。
 * @param ctx: 输出参数, 人员检测上下文, 内部完成 rknn3 上下文创建与输入输出张量分配
 * @param model_path: 输入参数, 算法模型文件路径
 * @return: 0表示成功，-1表示失败
 */
int person_detect_init(person_detect_ctx *ctx, const char *model_path);

/**
 * @brief 人员检测执行函数
 * @details 该函数对输入图像依次执行预处理、模型推理和后处理，输出的目标框坐标
 *          已还原到原图坐标系。
 * @param ctx: 输入参数, 人员检测上下文
 * @param input_image: 输入参数, Opencv Mat 格式图像（按值传入，为浅拷贝）
 * @param detect_result_group: 输出参数, 目标检测结果，函数内部会先清零再填充
 * @return: 0表示成功，-1表示失败；检测到的目标数量见 detect_result_group->count
 */
int person_detect_run(person_detect_ctx *ctx, cv::Mat input_image, detect_result_group_t *detect_result_group);

/**
 * @brief 人员检测释放函数
 * @details 释放 ctx 内部持有的资源：NPU 张量内存、张量属性、输入输出张量数组以及 rknn3 上下文，
 *          并将 ctx 清零；不释放 ctx 本身，可安全重复调用。
 * @param ctx: 输入参数, 人员检测上下文
 * @return: 0表示成功
 */
int person_detect_release(person_detect_ctx *ctx);

#endif  // PERSON_DETECT_H
