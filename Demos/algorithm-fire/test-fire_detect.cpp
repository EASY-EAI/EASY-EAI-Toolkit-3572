#include <stdio.h>
#include <sys/time.h>

#include <opencv2/opencv.hpp>

#include "fire_detect.h"
#include "benchmark.h"

using namespace cv;
using namespace std;

static Scalar colorArray[10] = {
    Scalar(255, 0, 0, 255),
    Scalar(0, 255, 0, 255),
    Scalar(0, 0, 139, 255),
    Scalar(0, 100, 0, 255),
    Scalar(139, 139, 0, 255),
    Scalar(209, 206, 0, 255),
    Scalar(0, 127, 255, 255),
    Scalar(139, 61, 72, 255),
    Scalar(0, 255, 0, 255),
    Scalar(255, 0, 0, 255),
};

int plot_one_box(Mat src, int x1, int x2, int y1, int y2, char *label, char colour) {
    int tl = round(0.002 * (src.rows + src.cols) / 2) + 1;
    rectangle(src, cv::Point(x1, y1), cv::Point(x2, y2), colorArray[(unsigned char)colour], 3);

    int tf = max(tl - 1, 1);

    int base_line = 0;
    cv::Size t_size = getTextSize(label, FONT_HERSHEY_SIMPLEX, (float)tl / 3, tf, &base_line);
    int x3 = x1 + t_size.width;
    int y3 = y1 - t_size.height - 3;

    rectangle(src, cv::Point(x1, y1), cv::Point(x3, y3), colorArray[(unsigned char)colour], -1);
    putText(
        src, label, cv::Point(x1, y1 - 2), FONT_HERSHEY_SIMPLEX, (float)tl / 3, cv::Scalar(255, 255, 255, 255), tf, 8);
    return 0;
}

int main(int argc, char **argv) {
    bool enable_benchmark = false;

    if (argc < 3 || argc > 4) {
        printf("%s <model_path> <image_path> [--benchmark]\n", argv[0]);
        return -1;
    }
    if (argc == 4) {
        if (std::string(argv[3]) == "--benchmark") {
            enable_benchmark = true;
        } else {
            printf("unknown option: %s\n", argv[3]);
            printf("%s <model_path> <image_path> [--benchmark]\n", argv[0]);
            return -1;
        }
    }

    const char *model_path = argv[1];
    const char *image_path = argv[2];

    /* 参数初始化 */
    detect_result_group_t detect_result_group;

    /* 算法模型初始化 */
    fire_detect_ctx ctx;
    fire_detect_init(&ctx, model_path);

    /* 算法运行 */
    cv::Mat src;
    src = cv::imread(image_path, 1);

    struct timeval start;
    struct timeval end;
    float time_use = 0;

    gettimeofday(&start, NULL);

    fire_detect_run(&ctx, src, &detect_result_group);

    gettimeofday(&end, NULL);
    time_use = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);  // 微秒
    printf("time_use is %f\n", time_use / 1000);

    /* 算法结果在图像中画出并保存 */
    // Draw Objects
    char text[256];
    for (int i = 0; i < detect_result_group.count; i++) {
        detect_result_t *det_result = &(detect_result_group.results[i]);
        if (det_result->prop < 0.4) {
            continue;
        }

        sprintf(text, "%s %.1f%%", det_result->name, det_result->prop * 100);
        printf("%s @ (%d %d %d %d) %f\n", det_result->name, det_result->box.left, det_result->box.top,
            det_result->box.right, det_result->box.bottom, det_result->prop);
        int x1 = det_result->box.left;
        int y1 = det_result->box.top;
        int x2 = det_result->box.right;
        int y2 = det_result->box.bottom;
        /*
        rectangle(src, cv::Point(x1, y1), cv::Point(x2, y2), cv::Scalar(255, 0, 0, 255), 3);
        putText(src, text, cv::Point(x1, y1 + 12), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0));
        */
        plot_one_box(src, x1, x2, y1, y2, text, i % 10);
    }

    cv::imwrite("result.jpg", src);

    /*
     * ============================================================
     * 以下为【可选的 benchmark 段】，仅在命令行指定 --benchmark 时执行，
     * 不是主程序的一部分：不带 --benchmark 时整段会被跳过，
     *
     * 该段的作用是将模型推理部分重复执行 run_times 次，
     * 进度条显示执行进度，最后统计耗时分布
     * (avg / median / min / max)，用于衡量稳态推理性能。
     * ============================================================
     */
    if (enable_benchmark) {
        printf("Fire detect benchmark start:\n");
        const int run_times = 100;
        std::vector<double> latencies;
        latencies.reserve(run_times);

        for (int i = 0; i < run_times; i++) {
            double t0 = now_ms();
            fire_detect_run(&ctx, src, &detect_result_group);
            double t1 = now_ms();
            latencies.push_back(t1 - t0);

            print_progress_bar(i + 1, run_times);
        }

        print_latency_stats(latencies);
    }

    /* 算法模型空间释放 */
    fire_detect_release(&ctx);

    return 0;
}
