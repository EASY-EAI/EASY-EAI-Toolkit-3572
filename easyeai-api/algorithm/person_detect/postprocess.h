#ifndef PERSON_DETECT_POSTPROCESS_H
#define PERSON_DETECT_POSTPROCESS_H

#include <stdint.h>
#include <vector>

#define OBJ_NAME_MAX_SIZE 16               // 类别名字符串的最大长度（含结束符 '\0'），实际可用 15 字节
#define OBJ_NUMB_MAX_SIZE 64               // 单帧图像最多输出的目标数量
#define OBJ_CLASS_NUM 1                    // 模型支持的类别数（人员检测固定为 1 类）
#define NMS_THRESH 0.45                    // 默认的 NMS IoU 阈值
#define BOX_THRESH 0.25                    // 默认的目标置信度阈值
#define PROP_BOX_SIZE (5 + OBJ_CLASS_NUM)  // 每个候选框的输出通道数：4 个框偏移 + 1 个目标置信度 + 类别数

/**
 * @brief 目标框结构体
 * @details 以原图坐标系表示目标框，存放左上角与右下角的坐标（不是宽高）。
 */
typedef struct _BOX_RECT {
    int left;    // 左上角 x 坐标
    int right;   // 右下角 x 坐标
    int top;     // 左上角 y 坐标
    int bottom;  // 右下角 y 坐标
} BOX_RECT;

/**
 * @brief 单个目标检测结果结构体
 */
typedef struct __detect_result_t {
    char name[OBJ_NAME_MAX_SIZE];  // 类别名，最长 OBJ_NAME_MAX_SIZE-1 个字节，以 '\0' 结尾
    int class_index;               // 类别索引，从 0 开始
    BOX_RECT box;                  // 目标框，原图坐标系
    float prop;                    // 目标置信度，范围 0~1
} detect_result_t;

/**
 * @brief 一帧图像的目标检测结果集合
 * @details 有效元素为 results 中下标 [0, count) 的部分。
 */
typedef struct _detect_result_group_t {
    int id;                                      // 保留字段，当前版本未赋值（保持为 0）
    int count;                                   // 有效结果数量，范围 0 ~ OBJ_NUMB_MAX_SIZE
    detect_result_t results[OBJ_NUMB_MAX_SIZE];  // 目标检测结果数组
} detect_result_group_t;

#endif  // PERSON_DETECT_POSTPROCESS_H
