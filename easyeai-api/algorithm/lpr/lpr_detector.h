#ifndef __LPR_DETECTOR_H__
#define __LPR_DETECTOR_H__

#include "rknn3_api.h"
#include "opencv2/opencv.hpp"

/**< @brief 车牌检测器 */
typedef struct {
    rknn3_context rknn_ctx;
    rknn3_input_output_num io_num;
    rknn3_tensor *inputs;
    rknn3_tensor *outputs;
    int model_width;
    int model_height;
    int model_channel;
    int output_width;
    int output_height;
} rknn_lpr_detector_t;

/**< @brief 车牌检测结果 */
typedef struct {
    cv::Rect box;          // 检测框位置
    int layer_num;         // 车牌行数0表一行，1表2行
    float score;           // 置信度
    cv::Point key_pts[4];  // 车牌四个角点
} rknn_lpr_det_result_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  rknn 车牌检测器初始化函数
 *
 * @param[i/o]		p_det					车牌检测器模型上下文
 * @param[in]		p_model_path			车牌检测器模型地址
 * @return								模型初始化结果，0成功，负值失败
 */
int rknn_lpr_detector_init(rknn_lpr_detector_t *p_det, const char *p_model_path);

/**
 * @brief  rknn 车牌检测函数
 *
 * @param[in]		image					待检测车牌图片（BGR格式）
 * @param[i/o]		p_det					车牌检测器模型上下文
 * @param[in]		conf_thresh				置信度阈值
 * @param[in]		nms_thresh				nms阈值
 * @return									返回检测结果及坐标
 */
std::vector<rknn_lpr_det_result_t> rknn_lpr_detector_calc(
    cv::Mat image, rknn_lpr_detector_t *p_det, float conf_thresh, float nms_thresh);

/**
 * @brief  rknn  车牌检测器释放函数
 *
 * @param[i/o]		p_det					车牌检测器模型上下文
 * @return									模型释放结果
 */
int rknn_lpr_detector_deinit(rknn_lpr_detector_t *p_det);

#ifdef __cplusplus
}
#endif

#endif  //__LPR_DETECTOR_H__