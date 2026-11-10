#ifndef DECODE_H
#define DECODE_H

#include <stdio.h>
#include <stdlib.h>

#include <opencv2/opencv.hpp>

#include "face_detect.h"

/**
 * @brief 解码人脸框和人脸关键点
 * @details 该函数从模型输出的置信度、位置和关键点张量中解码出人脸框与关键点，
 *          按置信度阈值丢弃低分候选框，并对剩余候选框执行 NMS 非极大值抑制。
 * @param confident: 输入参数, 模型输出的置信度张量，只读，函数不修改其内容
 * @param loc: 输入参数, 模型输出的位置张量，只读，函数不修改其内容
 * @param predict: 输入参数, 模型输出的关键点张量，只读，函数不修改其内容
 * @param score_threshold: 输入参数, 人脸框置信度阈值，置信度不大于该值的候选框被丢弃
 * @param nms_threshold: 输入参数, NMS 的 IoU 阈值，与已保留框的 IoU 大于该值的框被抑制
 * @param prior_data: 输入参数, 先验框数据，只读，函数不修改其内容
 * @param outputs: 输出参数, 人脸检测的结果输出，函数内部会先清空再填充
 */
void decode_box_and_landmark(rknn3_tensor *confident, rknn3_tensor *loc, rknn3_tensor *predict, float score_threshold,
    float nms_threshold, std::vector<std::vector<float>> &prior_data, std::vector<det> &outputs);

#endif  // DECODE_H