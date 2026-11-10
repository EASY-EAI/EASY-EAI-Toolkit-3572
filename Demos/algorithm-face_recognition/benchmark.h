#ifndef BENCHMARK_H
#define BENCHMARK_H

#include <vector>

// 当前时间，单位毫秒
double now_ms();

// 打印/刷新进度条，用 \r 覆盖同一行；current == total 时自动换行
void print_progress_bar(int current, int total, int bar_width = 30);

// 打印耗时统计：均值 / 中值 / 最小值 / 最大值
void print_latency_stats(const std::vector<double> &latencies);

#endif // BENCHMARK_H
