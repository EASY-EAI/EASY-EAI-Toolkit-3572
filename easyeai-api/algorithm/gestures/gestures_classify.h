#ifndef __RKNN_MOBILENET_V4_H__
#define __RKNN_MOBILENET_V4_H__

#include "opencv2/opencv.hpp"
#include "rknn3_api.h"

/**< @brief mobilenet v4图像分类 */
typedef struct {
    rknn3_context rknn_ctx;         // rknn3 上下文句柄
    rknn3_input_output_num io_num;  // 输入输出张量数量
    rknn3_tensor *inputs;           // 输入张量数组
    rknn3_tensor *outputs;          // 输出张量数组
    int model_channel;
    int model_width;
    int model_height;
} rknn_gestures_classify_context_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  rknn gestures_classify模型初始化
 *
 * @param[in]		p_model_path			gestures_classify rknn模型地址
 * @param[i/o]		p_mv4			        gestures_classify 模型上下文
 * @return									模型初始化结果，0成功，负值失败
 */
int rknn_gestures_classify_init(rknn_gestures_classify_context_t *p_mv4, const char *p_model_path);

/**
 * @brief  rknn gestures_classify图像分类
 *
 * @param[in]		p_mv4			        gestures_classify模型上下文
 * @param[in]		image					待分类图片
 * @param[i/o]		label			        分类类别
 * @param[i/o]		score			        分类置信度
 * @return									计算结果状态：0成功，负值失败
 */
int rknn_gestures_classify_calc(rknn_gestures_classify_context_t *p_mv4, cv::Mat image, int &label, float &score);

/**
 * @brief  rknn gestures_classify模型释放
 *
 * @param[i/o]		p_mv4			        gestures_classify模型上下文
 * @return									模型释放结果：0成功，负值失败
 */
int rknn_gestures_classify_deinit(rknn_gestures_classify_context_t *p_mv4);

#ifdef __cplusplus
}
#endif

#endif  //__RKNN_MOBILENET_V4_H__