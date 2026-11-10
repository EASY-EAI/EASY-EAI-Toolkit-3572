#include "benchmark.h"

#include <stdio.h>
#include <sys/time.h>
#include <algorithm>

double now_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

void print_progress_bar(int current, int total, int bar_width) {
    if (total <= 0) return;

    // 一行最多 256 字节，每个方块占 3 字节，留出 "]\r[nn/nn" 的空间
    const int max_width = 60;
    if (bar_width < 1) bar_width = 1;
    if (bar_width > max_width) bar_width = max_width;

    if (current > total) current = total;

    int filled = current * bar_width / total;
    char line[256];
    int pos = snprintf(line, sizeof(line), "\r[");
    for (int j = 0; j < bar_width; j++)
        pos += snprintf(line + pos, sizeof(line) - pos, "%s", j < filled ? "\xe2\x96\x88" : "-");
    snprintf(line + pos, sizeof(line) - pos, "] %d/%d", current, total);

    fputs(line, stdout);
    fflush(stdout);

    if (current >= total) fputc('\n', stdout);
}

void print_latency_stats(const std::vector<double> &latencies) {
    if (latencies.empty()) {
        printf("no latency data\n");
        return;
    }

    std::vector<double> sorted(latencies);
    std::sort(sorted.begin(), sorted.end());

    double sum = 0;
    for (size_t i = 0; i < latencies.size(); i++) sum += latencies[i];

    size_t n = sorted.size();
    double mean = sum / (double)n;
    double median = (n % 2) ? sorted[n / 2] : (sorted[n / 2 - 1] + sorted[n / 2]) / 2.0;

    printf("latency(ms) n=%d  avg %.3f  median %.3f  min %.3f  max %.3f\n", (int)n, mean, median, sorted.front(),
        sorted.back());
}
