
#include <opencv2/opencv.hpp>
#include <stdio.h>
#include <sys/time.h>
#include <string>
#include <vector>
#include "face_detect.h"
#include "benchmark.h"

using namespace cv;

int main(int argc, char **argv) {
    bool enable_benchmark = false;  // 默认关闭，只有指定 --benchmark 时才为 true

    // 用法: ./test-face-detect <image> [--benchmark]
    if (argc < 2 || argc > 3) {
        printf("./test-face-detect xxx [--benchmark]\n");
        return -1;
    }
    if (argc == 3) {
        if (std::string(argv[2]) == "--benchmark") {
            enable_benchmark = true;
        } else {
            printf("unknown option: %s\n", argv[2]);
            printf("./test-face-detect xxx [--benchmark]\n");
            return -1;
        }
    }

    struct timeval start;
    struct timeval end;
    float time_use = 0;

    face_detect_ctx ctx;
    std::vector<det> result;

    // 1. 读图
    Mat image;
    image = cv::imread(argv[1], 1);

    // 2. 初始化模型
    face_detect_init(&ctx, "face_detect.model");

    // 3. 推理一次并打印本次耗时（含首次初始化开销，不代表稳态性能）
    gettimeofday(&start, NULL);
    face_detect_run(&ctx, image, result);

    gettimeofday(&end, NULL);
    time_use = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);  // 微秒
    printf("time_use is %f\n", time_use / 1000);

    printf("face num:%d\n", (int)result.size());

    // 4. 把检测框与人脸关键点画到图上
    for (int i = 0; i < (int)result.size(); i++) {
        int x = (int)(result[i].box.x);
        int y = (int)(result[i].box.y);
        int w = (int)(result[i].box.width);
        int h = (int)(result[i].box.height);
        rectangle(image, Rect(x, y, w, h), Scalar(0, 255, 0), 2, 8, 0);

        for (int j = 0; j < (int)result[i].landmarks.size(); ++j) {
            cv::circle(image, cv::Point((int)result[i].landmarks[j].x, (int)result[i].landmarks[j].y), 2,
                cv::Scalar(225, 0, 225), 2, 8);
        }
    }

    // 5. 保存可视化结果
    imwrite("result.jpg", image);

    /*
     * ============================================================
     * 以下为【可选的 benchmark 段】，仅在命令行指定 --benchmark 时执行，
     * 不是主程序的一部分：不带 --benchmark 时整段会被跳过，
     * 程序做完「读图 -> 推理一次 -> 画框 -> 存图」就退出。
     *
     * 该段的作用是将模型推理重复执行 run_times 次，
     * 进度条显示执行进度，最后统计耗时分布
     * (avg / median / min / max)，用于衡量稳态推理性能。
     * ============================================================
     */
    if (enable_benchmark) {
        printf("Start benchmark: \n");
        const int run_times = 100;
        std::vector<double> latencies;
        latencies.reserve(run_times);

        for (int i = 0; i < run_times; i++) {
            // 只对 face_detect_run 计时，进度条打印的开销不计入
            double t0 = now_ms();
            face_detect_run(&ctx, image, result);
            double t1 = now_ms();
            latencies.push_back(t1 - t0);

            print_progress_bar(i + 1, run_times);
        }

        print_latency_stats(latencies);
    }

    face_detect_release(&ctx);

    return 0;
}
