#ifndef __GESTURES_POSE_H__
#define __GESTURES_POSE_H__

#include "opencv2/opencv.hpp"
#include "rknn3_api.h"

/**< @brief gestures pose模型结构使*/
typedef struct {
    rknn3_context rknn_ctx;         // rknn3 上下文句柄
    rknn3_input_output_num io_num;  // 输入输出张量数量
    rknn3_tensor *inputs;           // 输入张量数组
    rknn3_tensor *outputs;          // 输出张量数组
    int model_channel;
    int model_width;
    int model_height;
    int input_image_width;
    int input_image_height;
    int cls_num;
} rknn_gestures_pose_context_t;

/**< @brief gestures pose检测和分割结果 */
typedef struct {
    int cls_id;
    int left;
    int top;
    int right;
    int bottom;
    float prop;
    float keypoints[21][3];  // keypoints x,y,conf
} rknn_gestures_pose_result_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  rknn gestures pose 初始化函敿
 *
 * @param[in]		p_model_path			gestures pose rknn模型地址
 * @param[i/o]		p_gestures_pose			gestures pose模型上下斿
 * @param[in]		cls_num					类别敿
 * @return									模型初始化结果，0成功，负值失贿
 */
int rknn_gestures_pose_init(rknn_gestures_pose_context_t *ctx, const char *p_model_path, int cls_num);

/**
 * @brief  rknn gestures pose计算函数
 *
 * @param[in]		image					待检测图牿
 * @param[in]		p_gestures_pose			gestures pose模型上下斿
 * @param[in]		nms_threshold			NMS阈倿
 * @param[in]		conf_threshold			置信度阈倿
 * @return									检测和姿态估计结枿
 */
std::vector<rknn_gestures_pose_result_t> rknn_gestures_pose_calc(
    cv::Mat image, rknn_gestures_pose_context_t *ctx, float nms_threshold, float conf_threshold);

/**
 * @brief  rknn gestures pose释放函数
 *
 * @param[i/o]		p_gestures_pose			gestures pose模型上下斿
 * @return									模型释放结果
 */
int rknn_gestures_pose_deinit(rknn_gestures_pose_context_t *ctx);

#ifdef __cplusplus
}
#endif

#endif  //__RKNN_gestures_POSE_H__