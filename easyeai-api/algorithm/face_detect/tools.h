#ifndef TOOLS_H
#define TOOLS_H

#include <opencv2/opencv.hpp>

#include "face_detect.h"

struct Transform_info {
    int src_width;      // 原图宽度
    int src_height;     // 原图高度
    int target_width;   // 缩放后的目标宽度（模型输入宽度）
    int target_height;  // 缩放后的目标高度（模型输入高度）
    int top;            // 原图上方填充的像素数（原图坐标系）
    int bottom;         // 原图下方填充的像素数（原图坐标系）
    int left;           // 原图左侧填充的像素数（原图坐标系）
    int right;          // 原图右侧填充的像素数（原图坐标系）
    float ratio;        // 缩放比例，补边后图像宽度 / target_width，>= 1 表示原图被缩小
};

/**
 * @brief NMS 非极大值抑制
 * @details 该函数按置信度从高到低遍历候选框，与已保留框的 IoU 大于阈值的候选框被抑制。
 * @param boxes: 输入参数, 待处理的 det 候选框数组，函数内部会按置信度降序重排
 * @param threshold: 输入参数, IoU 阈值，与已保留框的 IoU 大于该值的候选框被抑制
 * @param filtered_output: 输出参数, 抑制后保留的 det 数组，函数内部会先清空再填充
 */
void nms_cpu(std::vector<det> &boxes, float threshold, std::vector<det> &filtered_output);

/**
 * @brief letterbox 居中补边并缩放
 * @details 该函数先按目标宽高比在原图四周对称填充（填充色为灰度 114），
 *          再把补边后的图像 resize 到 width x height。
 * @param src: 输入参数, 原图，函数不修改其内容
 * @param dst: 输出参数, 补边并缩放后的图像，尺寸为 width x height
 * @param width: 输入参数, 目标宽度（模型输入宽度）
 * @param height: 输入参数, 目标高度（模型输入高度）
 * @param t_info: 输出参数, 本次变换的参数，用于把模型输出的坐标还原到原图坐标系
 */
void letter_box(cv::Mat &src, cv::Mat &dst, int width, int height, struct Transform_info *t_info);

#endif  // TOOLS_H