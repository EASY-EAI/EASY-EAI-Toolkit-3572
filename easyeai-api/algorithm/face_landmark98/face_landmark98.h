#ifndef __RKNN_FACE_LANDMARK_H__
#define __RKNN_FACE_LANDMARK_H__

#include "opencv2/opencv.hpp"
#include "rknn3_api.h"

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
} rknn_face_landmark_context_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  face_landmark98_init
 *
 * @param[i/o]		p_face_landmark			face_landmark模型上下文
 * @param[in]		p_model_path			face_landmark rknn模型地址
 * @return									模型初始化结果，0成功，负值失败
 */
int face_landmark98_init(rknn_face_landmark_context_t *ctx, const char *p_model_path);

/**
 * @brief  face_landmark98_run
 *
 * @param[in]		p_face_landmark			face_landmark模型上下文
 * @param[in]		image					待检测图片
 * @return									人脸关键点
 */
std::vector<cv::Point> face_landmark98_run(rknn_face_landmark_context_t *ctx, cv::Mat image);

/**
 * @brief face_landmark_release
 *
 * @param[i/o]		p_face_landmark			face_landmark模型上下文
 * @return									模型释放结果
 */
int face_landmark98_release(rknn_face_landmark_context_t *ctx);

#ifdef __cplusplus
}
#endif

#endif  //__RKNN_FACE_LANDMARK_H__