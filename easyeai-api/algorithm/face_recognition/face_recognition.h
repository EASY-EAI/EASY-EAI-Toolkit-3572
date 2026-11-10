#ifndef FACE_RECOGNITION_H
#define FACE_RECOGNITION_H

#include <opencv2/core.hpp>

#include "rknn3_api.h"

/**
 * @brief 人脸识别上下文结构体
 * @details 该结构体用于存储人脸识别的上下文信息，由 face_recognition_init 填充，
 *          其内部资源由 face_recognition_release 释放。
 */
typedef struct {
    rknn3_context rknn_ctx;         // rknn3 上下文句柄
    rknn3_input_output_num io_num;  // 输入输出张量数量
    rknn3_tensor *inputs;           // 输入张量数组
    rknn3_tensor *outputs;          // 输出张量数组
} face_recognition_ctx;

/**
 * @brief 人脸识别初始化函数
 * @details 该函数用于初始化人脸识别上下文，包括创建 rknn3 上下文和分配输入输出张量。
 * @param ctx: 输出参数, 人脸识别上下文, 内部完成 rknn3 上下文创建与输入输出张量分配
 * @param model_path: 输入参数, 算法模型文件路径
 * @return: 0表示成功，-1表示失败
 */
int face_recognition_init(face_recognition_ctx *ctx, const char *model_path);

/**
 * @brief 人脸特征码提取函数
 * @details 该函数对已对齐的人脸图像执行推理，输出 512 维人脸特征码。
 * @param ctx: 输入参数, 人脸识别上下文
 * @param face_image: 输入参数, 已对齐的人脸图像，宽高与通道数必须与模型输入一致
 * @param feature: 输出参数, 人脸特征码，指向 512 个 float 的缓冲区，由调用方预先分配
 * @return: 0表示成功，-1表示失败
 */
int face_recognition_run(face_recognition_ctx *ctx, cv::Mat *face_image, float (*feature)[512]);

/**
 * @brief 人脸特征码比对函数
 * @details 该函数计算两段特征码的余弦相似度，结果越接近 1 表示越相似。
 * @param feature_1: 输入参数, 人脸特征码1，长度为 output_len
 * @param feature_2: 输入参数, 人脸特征码2，长度为 output_len
 * @param output_len: 输入参数, 特征码长度，应等于特征维度 512
 * @return: 余弦相似度，取值范围 [-1, 1]；特征码全为 0 时结果为 NaN
 */
float face_recognition_comparison(float *feature_1, float *feature_2, int output_len);

/**
 * @brief 人脸识别释放函数
 * @details 释放 ctx 内部持有的资源：NPU 张量内存、张量属性、输入输出张量数组以及 rknn3 上下文，
 *          并将 ctx 清零；不释放 ctx 本身，可安全重复调用。
 * @param ctx: 输入参数, 人脸识别上下文
 * @return: 0表示成功
 */
int face_recognition_release(face_recognition_ctx *ctx);

#endif  // FACE_RECOGNITION_H
