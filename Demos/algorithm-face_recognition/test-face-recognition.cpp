
#include <opencv2/opencv.hpp>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include "face_detect.h"
#include "face_alignment.h"
#include "face_recognition.h"
#include "benchmark.h"

using namespace cv;

int main(int argc, char **argv) {
    face_detect_ctx detect_ctx;
    face_recognition_ctx recognition_ctx;

    std::vector<det> result1, result2;
    int ret;
    struct timeval start;
    struct timeval end;
    float time_use = 0;

    bool enable_benchmark = false;  // 默认关闭，只有指定 --benchmark 时才为 true
    if (argc < 3 || argc > 4) {
        printf("./face_recognition_demo xxx.jpg xxx.jpg [--benchmark]\n");
        return -1;
    }
    if (argc == 4) {
        if (std::string(argv[3]) == "--benchmark") {
            enable_benchmark = true;
        } else {
            printf("unknown option: %s\n", argv[3]);
            printf("./face_recognition_demo xxx.jpg xxx.jpg [--benchmark]\n");
            return -1;
        }
    }

    cv::Mat src_1, src_2;
    src_1 = cv::imread(argv[1], 1);
    src_2 = cv::imread(argv[2], 1);

    /* 人脸检测初始化 */
    printf("face detect init!\n");
    ret = face_detect_init(&detect_ctx, "face_detect.model");
    if (ret < 0) {
        printf("face_detect_init fail! ret=%d\n", ret);
        return -1;
    }

    /* 人脸识别初始化 */
    printf("face recognition init!\n");
    ret = face_recognition_init(&recognition_ctx, "face_recognition.model");
    if (ret < 0) {
        printf("face_recognition fail! ret=%d\n", ret);
        return -1;
    }

    /* 人脸检测执行 */
    face_detect_run(&detect_ctx, src_1, result1);
    face_detect_run(&detect_ctx, src_2, result2);

    Point2f points1[5], points2[5];

    for (int j = 0; j < (int)result1[0].landmarks.size(); ++j) {
        points1[j].x = (int)result1[0].landmarks[j].x;
        points1[j].y = (int)result1[0].landmarks[j].y;
    }

    for (int j = 0; j < (int)result2[0].landmarks.size(); ++j) {
        points2[j].x = (int)result2[0].landmarks[j].x;
        points2[j].y = (int)result2[0].landmarks[j].y;
    }

    Mat face_algin_1, face_algin_2;
    face_algin_1 = face_alignment(src_1, points1);
    face_algin_2 = face_alignment(src_2, points2);

    /* 人脸识别执行 */
    float feature_1[512], feature_2[512];
    float similarity;

    gettimeofday(&start, NULL);

    face_recognition_run(&recognition_ctx, &face_algin_1, &feature_1);
    face_recognition_run(&recognition_ctx, &face_algin_2, &feature_2);
    similarity = face_recognition_comparison(feature_1, feature_2, 512);

    gettimeofday(&end, NULL);
    time_use = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);  // 微秒
    printf("time_use is %f\n", time_use / 1000);

    printf("similarity:%f\n", similarity);

    /* 人脸检测释放 */
    face_detect_release(&detect_ctx);
    /*
     * ============================================================
     * 以下为【可选的 benchmark 段】，仅在命令行指定 --benchmark 时执行，
     * 不是主程序的一部分：不带 --benchmark 时整段会被跳过，
     *
     * 该段的作用是将模型推理重复执行 run_times 次，
     * 进度条显示执行进度，最后统计耗时分布
     * (avg / median / min / max)，用于衡量稳态推理性能。
     * ============================================================
     */
    if (enable_benchmark) {
        printf("benchmark start:\n");
        const int run_times = 100;
        std::vector<double> latencies;
        latencies.reserve(run_times);

        for (int i = 0; i < run_times; ++i) {
            double t0 = now_ms();
            face_recognition_run(&recognition_ctx, &face_algin_1, &feature_1);
            face_recognition_run(&recognition_ctx, &face_algin_2, &feature_2);
            similarity = face_recognition_comparison(feature_1, feature_2, 512);
            double t1 = now_ms();
            latencies.push_back(t1 - t0);

            print_progress_bar(i + 1, run_times);
        }
        print_latency_stats(latencies);
    }

    /* 人脸识别释放 */
    face_recognition_release(&recognition_ctx);

    return 0;
}
