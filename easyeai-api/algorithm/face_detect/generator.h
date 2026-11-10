#ifndef GENERATOR_H
#define GENERATOR_H

#include <string.h>
#include <vector>
#include <iostream>
#include <cmath>
#include <opencv2/opencv.hpp>

/**
 * @brief 生成 prior_data（先验框数据）
 * @details 该函数按模型输入尺寸生成全部先验框。返回值为二维数组：第一维是先验框个数，
 *          第二维是单个先验框的 4 个归一化分量，依次为 [中心点 x, 中心点 y, 宽, 高]。
 * @param width: 输入参数, 模型输入宽度（网络输入尺寸，非原图宽度）
 * @param height: 输入参数, 模型输入高度（网络输入尺寸，非原图高度）
 * @return: 二维浮点数数组，第一维是先验框个数，第二维是单个先验框的 4 个归一化分量
 */
std::vector<std::vector<float>> generate_prior_data(int width, int height);

/**
 * @brief 解码单个人脸框
 * @details 该函数根据单个先验框和模型 loc 预测值，解码出单个人脸框；
 *          输出坐标为相对模型输入的归一化值，需由调用方还原到原图坐标系。
 * @param loc: 输入参数, 单个先验框对应的 4 个 loc 预测值，只读
 * @param single_prior_data: 输入参数, 单个先验框数据 [中心点 x, 中心点 y, 宽, 高]，只读
 * @param rect: 输出参数, 解码得到的单个人脸框（左上角坐标 + 宽高）
 */
void decode_box(float *loc, const std::vector<float> &single_prior_data, cv::Rect_<float> &rect);

/**
 * @brief 解码单个人脸关键点
 * @details 该函数根据单个先验框和模型 predict 预测值，解码出 5 个人脸关键点，
 *          顺序为：左眼、右眼、鼻尖、左嘴角、右嘴角；输出坐标为相对模型输入的归一化值，
 *          需由调用方还原到原图坐标系。
 * @param predict: 输入参数, 单个先验框对应的 10 个 predict 预测值（5 点，每点 x/y 交替），只读
 * @param single_prior_data: 输入参数, 单个先验框数据 [中心点 x, 中心点 y, 宽, 高]，只读
 * @param landmark: 输出参数, 5 个人脸关键点；调用前必须已 resize(5)，否则函数打印错误后直接返回
 */
void decode_landmark(float *predict, const std::vector<float> &single_prior_data, std::vector<cv::Point2f> &landmark);

#endif  // GENERATOR_H
