#ifndef FACE_ALIGNMENT_H
#define FACE_ALIGNMENT_H

#include <opencv2/core.hpp>

/**
 * @brief 人脸对齐函数
 * @details 该函数根据 5 个人脸关键点做相似变换，把人脸对齐到固定的标准姿态。
 * @param img: 输入参数, 待对齐的图像（cv::Mat 按值传入，为浅拷贝）
 * @param points: 输入参数, 5 个人脸关键点，顺序为：左眼、右眼、鼻尖、左嘴角、右嘴角，
 *                坐标需与 img 处于同一坐标系；数组长度不足 5 会导致越界
 * @return: 对齐后的人脸图像，固定为 112x112、CV_8UC3
 */
cv::Mat face_alignment(cv::Mat img, cv::Point2f *points);

#endif  // FACE_ALIGNMENT_H
