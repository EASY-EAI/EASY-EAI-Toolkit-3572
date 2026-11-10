#ifndef __LPR_CLASSIFIER_H__
#define __LPR_CLASSIFIER_H__

#include "rknn3_api.h"
#include "opencv2/opencv.hpp"

/**< @brief 车牌分类器 */
typedef struct {
    rknn3_context rknn_ctx;
    rknn3_input_output_num io_num;
    rknn3_tensor *inputs;
    rknn3_tensor *outputs;
    int model_width;
    int model_height;
    int model_channel;
} rknn_lpr_classifer_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  rknn 车牌分类初始化函数
 *
 * @param[in]		p_model_path			车牌分类器模型地址
 * @param[i/o]		p_cls					车牌分类器模型上下文
 * @return									模型初始化结果，0成功，负值失败
 */
int rknn_lpr_classifer_init(rknn_lpr_classifer_t *ctx, const char *p_model_path);

/**
 * @brief  rknn 车牌分类识别函数
 *
 * @param[in]		image					待分类图片（BGR格式）
 * @param[i/o]		p_cls					车牌分类器模型上下文
 * @param[i/o]		label					类别标签：类别数为3，0蓝/1绿/2黄
 * @param[i/o]		score					置信度评分
 * @return									模型初始化结果，0成功，负值失败
 */
int rknn_lpr_classifer_calc(cv::Mat image, rknn_lpr_classifer_t *p_cls, int &label, float &score);

/**
 * @brief  rknn  车牌分类释放函数
 *
 * @param[i/o]		p_cls					车牌分类器模型上下文
 * @return									模型释放结果
 */
int rknn_lpr_classifer_deinit(rknn_lpr_classifer_t *cls);

#ifdef __cplusplus
}
#endif

#endif  //__LPR_CLASSIFIER_H__