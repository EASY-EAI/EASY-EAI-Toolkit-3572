#ifndef FACE_DETECT_H
#define FACE_DETECT_H

#include <stdio.h>
#include <stdlib.h>

#include <opencv2/opencv.hpp>

#include "rknn3_api.h"

/**
 * @brief 人脸检测结果类
 * @details 该类用于存储单个人脸的检测结果，包括人脸框、5 个人脸关键点和置信度。
 */
class det {
public:
    det() { landmarks.resize(5); }
    ~det() {}

    bool operator<(const det &t) const { return score < t.score; }

    bool operator>(const det &t) const { return score > t.score; }

    cv::Rect_<float> box;                // 人脸框，原图坐标系，x/y 为左上角坐标，width/height 为宽高
    std::vector<cv::Point2f> landmarks;  // 5 个人脸关键点，原图坐标系，顺序为：左眼、右眼、鼻尖、左嘴角、右嘴角
    float score;                         // 人脸框置信度，越大表示越可能是人脸

    /**
     * @brief 打印人脸检测结果
     * @details 该函数向标准输出打印单个人脸检测结果，依次输出人脸框的左上角坐标、
     *          右下角坐标、置信度以及全部关键点坐标。
     */
    void print() {
        printf("finalbox(x1y1x2y2) %f %f %f %f, score %f\n", box.x, box.y, box.br().x, box.br().y, score);
        printf("landmarks ");
        int point_number = landmarks.size();
        for (int i = 0; i < point_number; i++) {
            printf("%f %f, ", landmarks[i].x, landmarks[i].y);
        }
        printf("\n");
    }
};

/**
 * @brief 人脸检测上下文结构体
 * @details 该结构体用于存储人脸检测的上下文信息，由 face_detect_init 填充，
 *          其内部资源由 face_detect_release 释放；类型别名为 face_detect_ctx_t。
 */
typedef struct face_detect_ctx {
    rknn3_context rknn_ctx;         // rknn3 上下文句柄
    rknn3_input_output_num io_num;  // 输入输出张量数量
    rknn3_tensor *inputs;           // 输入张量数组
    rknn3_tensor *outputs;          // 输出张量数组
} face_detect_ctx_t;

/**
 * @brief 人脸检测初始化函数
 * @details 该函数用于初始化人脸检测上下文，包括创建 rknn3 上下文和分配输入输出张量。
 * @param ctx: 输出参数, 人脸检测上下文, 内部完成 rknn3 上下文创建与输入输出张量分配
 * @param model_path: 输入参数, 算法模型文件路径
 * @return: 0表示成功，-1表示失败
 */
int face_detect_init(face_detect_ctx *ctx, const char *model_path);

/**
 * @brief 人脸检测运行函数
 * @details 该函数用于运行人脸检测算法，包括图像预处理、模型推理和结果解码；
 *          输出的结果均已还原到原图坐标系。
 * @param ctx: 输入参数, 人脸检测上下文
 * @param input_image: 输入参数, Opencv Mat 格式图像，函数不修改其内容
 * @param result: 输出参数, 人脸检测的结果输出，函数内部会先清空再填充
 * @return: 输出的人脸数量，<0表示失败
 */
int face_detect_run(face_detect_ctx *ctx, cv::Mat &input_image, std::vector<det> &result);

/**
 * @brief 人脸检测释放函数
 * @details 释放 ctx 内部持有的资源：NPU 张量内存、张量属性、输入输出张量数组以及 rknn3 上下文。
 *          不释放 ctx 本身——ctx 由调用方分配（栈上或堆上均可）并负责释放。
 *          可安全重复调用，也可在 face_detect_init 失败后调用。
 * @param ctx: 输入参数, 人脸检测上下文
 * @return: 0表示成功
 */
int face_detect_release(face_detect_ctx *ctx);

#endif  // FACE_DETECT_H
