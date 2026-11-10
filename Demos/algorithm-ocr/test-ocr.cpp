
#include <opencv2/opencv.hpp>
#include <stdio.h>
#include <sys/time.h>
#include "benchmark.h"
#include "ocr.h"

using namespace cv;
using namespace std;

#define INDENT "    "
#define THRESHOLD 0.3        // pixel score threshold
#define BOX_THRESHOLD 0.9    // box score threshold
#define USE_DILATION false   // whether to do dilation, true or false
#define DB_UNCLIP_RATIO 1.5  // unclip ratio for poly type

int main(int argc, char **argv) {
    bool enable_benchmark = false;
    if (argc < 2 || argc > 3) {
        printf("%s <image_path> [--benchmark]\n", argv[0]);
        return -1;
    }

    if (argc == 3) {
        if (strcmp(argv[2], "--benchmark") == 0) {
            enable_benchmark = true;
        } else {
            printf("unknown option: %s\n", argv[2]);
            printf("%s <image_path> [--benchmark]\n", argv[0]);
            return -1;
        }
    }

    /* 参数初始化 */
    const char *img_path = argv[1];
    Mat input_image, rgb_img;
    input_image = imread(img_path);
    if (input_image.empty()) {
        cout << "Error: Could not load image" << endl;
        return -1;
    }

    cv::cvtColor(input_image, rgb_img, COLOR_BGR2RGB);

    rknn_app_context_t ocr_det_ctx, ocr_rec_ctx;
    memset(&ocr_det_ctx, 0, sizeof(rknn_app_context_t));
    memset(&ocr_rec_ctx, 0, sizeof(rknn_app_context_t));

    /* OCR算法检测模型&识别模型初始化 */
    ocr_det_init(&ocr_det_ctx, "ocr-det.model");
    ocr_rec_init(&ocr_rec_ctx, "ocr-rec.model");

    struct timeval start;
    struct timeval end;
    float time_use = 0;

    /* OCR算法检测模型运行 */
    ocr_det_result results;
    ocr_det_postprocess_params params;
    params.threshold = THRESHOLD;
    params.box_threshold = BOX_THRESHOLD;
    params.use_dilate = USE_DILATION;
    params.db_score_mode = (char *)"slow";
    params.db_box_type = (char *)"poly";
    params.db_unclip_ratio = DB_UNCLIP_RATIO;

    gettimeofday(&start, NULL);
    int ret;
    ret = ocr_det_run(&ocr_det_ctx, rgb_img, &params, &results);
    if (ret != 0) {
        printf("inference_ppocr_det_model fail! ret=%d\n", ret);
    }
    gettimeofday(&end, NULL);
    time_use = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);  // 微秒
    printf("time_use is %f\n", time_use / 1000);

    /* 截取文字信息和画框 */
    printf("DRAWING OBJECT\n");
    for (int i = 0; i < results.count; i++) {
        printf("[%d]: [(%d, %d), (%d, %d), (%d, %d), (%d, %d)] %f\n", i, results.box[i].left_top.x,
            results.box[i].left_top.y, results.box[i].right_top.x, results.box[i].right_top.y,
            results.box[i].right_bottom.x, results.box[i].right_bottom.y, results.box[i].left_bottom.x,
            results.box[i].left_bottom.y, results.box[i].score);

        line(input_image, Point(results.box[i].left_top.x, results.box[i].left_top.y),
            Point(results.box[i].right_top.x, results.box[i].right_top.y), Scalar(0, 255, 0), 1, LINE_AA);
        line(input_image, Point(results.box[i].right_top.x, results.box[i].right_top.y),
            Point(results.box[i].right_bottom.x, results.box[i].right_bottom.y), Scalar(0, 255, 0), 1, LINE_AA);
        line(input_image, Point(results.box[i].right_bottom.x, results.box[i].right_bottom.y),
            Point(results.box[i].left_bottom.x, results.box[i].left_bottom.y), Scalar(0, 255, 0), 1, LINE_AA);
        line(input_image, Point(results.box[i].left_bottom.x, results.box[i].left_bottom.y),
            Point(results.box[i].left_top.x, results.box[i].left_top.y), Scalar(0, 255, 0), 1, LINE_AA);

        cv::Mat rgb_crop_image = GetRotateCropImage(rgb_img, results.box[i]);

        /* OCR算法识别模型运行 */
        ocr_rec_result rec_results;
        ocr_rec_run(&ocr_rec_ctx, rgb_crop_image, &rec_results);

        // print text result
        printf("regconize result: %s, score=%f\n", rec_results.str, rec_results.score);
    }
    cv::imwrite("result.jpg", input_image);

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
        printf("Det benchmark start:\n");
        const int run_times = 100;
        std::vector<double> latencies;
        latencies.reserve(run_times);

        for (int i = 0; i < run_times; ++i) {
            double t0 = now_ms();
            ocr_det_run(&ocr_det_ctx, rgb_img, &params, &results);
            double t1 = now_ms();
            latencies.push_back(t1 - t0);

            print_progress_bar(i + 1, run_times);
        }
        print_latency_stats(latencies);

        printf("Rec benchmark start:\n");
        latencies.clear();
        latencies.reserve(run_times);
        cv::Mat rgb_crop_image = GetRotateCropImage(rgb_img, results.box[0]);
        ocr_rec_result rec_results;

        for (int i = 0; i < run_times; ++i) {
            double t0 = now_ms();
            ocr_rec_run(&ocr_rec_ctx, rgb_crop_image, &rec_results);
            double t1 = now_ms();
            latencies.push_back(t1 - t0);

            print_progress_bar(i + 1, run_times);
        }
        print_latency_stats(latencies);
    }
    ocr_det_release(&ocr_det_ctx);
    ocr_rec_release(&ocr_rec_ctx);
    return 0;
}
