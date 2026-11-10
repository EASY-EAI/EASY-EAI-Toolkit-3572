#ifndef __LPR_RECOGNIZER_H__
#define __LPR_RECOGNIZER_H__

#include "rknn3_api.h"
#include "opencv2/opencv.hpp"

/**< @brief 车牌识别器 */
typedef struct {
    rknn3_context rknn_ctx;         // RKNN3 上下文
    rknn3_input_output_num io_num;  // 输入输出张量个数
    rknn3_tensor *inputs;           // 输入张量（含设备内存与属性）
    rknn3_tensor *outputs;          // 输出张量（含设备内存与属性）
    int model_channel;
    int model_width;
    int model_height;
    int output_width;   // 字符类别数
    int output_height;  // 时间步数
} rknn_lpr_recognizer_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  rknn 车牌字符识别初始化函数
 *
 * @param[i/o]		p_rec					车牌字符识别模型上下文
 * @param[in]		p_model_path			车牌字符识别模型地址
 * @return								模型初始化结果，0成功，负值失败
 */
int rknn_lpr_recognizer_init(rknn_lpr_recognizer_t *p_rec, const char *p_model_path);

/**
 * @brief  rknn 车牌字符识别函数
 *
 * @param[in]		image					待识别车牌图片（BGR格式）
 * @param[i/o]		p_rec					车牌字符识别模型上下文
 * @param[i/o]		char_list				车牌号
 * @param[i/o]		score					置信度评分
 * @return									模型初始化结果，0成功，负值失败
 */
int rknn_lpr_recognizer_calc(
    cv::Mat image, rknn_lpr_recognizer_t *p_rec, std::vector<std::string> &char_list, float &score);

/**
 * @brief  rknn  车牌字符识别释放函数
 *
 * @param[i/o]		p_rec					车牌字符识别模型上下文
 * @return									模型释放结果
 */
int rknn_lpr_recognizer_deinit(rknn_lpr_recognizer_t *p_rec);

#ifdef __cplusplus
}
#endif

#endif  //__LPR_RECOGNIZER_H__