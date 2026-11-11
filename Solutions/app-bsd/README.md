# app-bsd

盲区检测(BSD, Blind Spot Detection)示例:从板端 4 路摄像头取流,在屏幕上以 2×2 宫格全屏显示。

## 功能说明

- `src/main.cpp` 的 `SrcCfg_tab[]` 默认配置 4 路 AHD 通道(`srcType="AHD"`,`loaction` 为 `"1"`~`"4"`);`DecChannel` 把 `loaction` 转成摄像头索引,`mipicamera_init()` 打开对应的 `/dev/video<index>`,以 1920×1080 NV12 取原始帧。`loaction` 直接作为设备号,默认即 `/dev/video1`~`/dev/video4`
- 每路一个取流线程;帧经 `videoOutHandle()` 送到 `analyzer`,分析线程轮询各通道调用 `algorithm_process()`,显示线程负责合成 2×2 宫格
- 显示使用 `display_pro` 的底层 DRM/KMS 接口:启动时选择第一个可用连接器的 primary 图层,用 `disp_import_dmabuf()` 把整屏 XRGB8888 DMA 缓冲导入一次并常驻;之后每帧只更新内容并 `disp_commit()`,避免每帧重复导入导致图层被摘下
- 每个窗口的画面用 OpenCV 旋转 270°、缩放到窗口大小、BGR→BGRA 后贴到整屏画布
- 算法当前为空实现:`analyzer/algoProcess.cpp` 中 `bsd_init()`/`bsd_run()` 均被注释,`algorithm_process()` 返回空结果,因此**只有视频、没有检测框**;`CMakeLists.txt` 的 `sysLib_list` 当前也未链接 `BSD_LIBS`
- 日志使用 `log_manager_pro`,初始化 `/userdata/logs/app_bsd.ini`
- `main()` 中的 SIGINT 处理被 `#if 0` 关闭,`g_exitRequested` 不会置位;`Ctrl+C` 走系统默认动作直接终止进程,通道对象析构和 `analyzer_exit()` 不会执行

## 编译前准备

### 板端依赖

```bash
apt install libdrm-dev libopencv-dev
```

- `libdrm-dev`:`display` 模块走 DRM/KMS
- `libopencv-dev`:`opencv_core` / `opencv_imgproc` / `opencv_imgcodecs`(本工程直接用 OpenCV 做旋转、缩放与宫格合成)
- 若缺 OpenCV 头文件,编译报 `opencv2/opencv.hpp: No such file or directory`;缺 `.so` 则链接报 `cannot find -lopencv_core`。板端通常只装运行时库,需显式装 dev 包

### 预编译库

`easyeai-api/env.cmake` 从 `${CMAKE_SYSROOT}/etc/os-release` 计算 `CMAKE_BOARDSYS`(Debian 13 → `debian13`,Ubuntu 26.04 → `ubuntu26.04`),各 `api.cmake` 按该目录取对应平台的库:

- `media/display`:直接按 `${CMAKE_BOARDSYS}` 查找,仓库同时提供 `debian13/` 与 `ubuntu26.04/`
- `common/log_manager_pro`:优先对应平台目录,缺失时回退 `debian13/`
- `common/base64`、`common/system_opt`、`media/rga`:直接编译源码
- `algorithm/bsd`:仓库仅有 `debian13/libbsd.a`;当前未参与链接。启用检测时需使用目标平台对应的 `libbsd.a`,并把 `${BSD_LIBS}` 加入 `sysLib_list`

## 编译

```bash
./build.sh          # 编译,产物输出到 Release/app-bsd
./build.sh clear    # 清理 build/ 与 Release/app-bsd
./build.sh cpres    # 编译并把 Release/ 全部文件拷到板端
```

`build.sh` 会尝试把产物拷到 `$SYSROOT/userdata/Solu/app-bsd/`;若该路径在当前环境中不可写,拷贝步骤会失败,**不影响编译产物**,请按下面"部署"手动拷贝。

## 部署与运行

### 部署

```bash
ssh root@<板端IP> "mkdir -p /userdata/Solu/app-bsd"
scp Release/app-bsd root@<板端IP>:/userdata/Solu/app-bsd/
chmod +x /userdata/Solu/app-bsd/app-bsd
```

### 运行(板端,需 root)

```bash
cd /userdata/Solu/app-bsd
sudo ./app-bsd
```

无命令行参数。通道数由 `src/main.cpp` 的 `SrcCfg_tab[]` 决定(默认 4 路,后 4 路已注释)。

运行前置条件:

1. **需要 root**:DRM/KMS 送显要取得 DRM master
2. **桌面环境不能占用 DRM**:若板端运行 GNOME/gdm 等桌面,先停掉(如 `systemctl stop gdm`),否则提交成功但画面不可见
3. 摄像头设备节点存在:默认打开 `/dev/video1`~`/dev/video4`,编号来自 `SrcCfg_tab[]` 的 `loaction`
4. `/userdata/logs/` 目录存在,程序会初始化日志 `/userdata/logs/app_bsd.ini`
5. 可选:运行目录下放 `noSignal.jpg`,通道对象尚未创建时用于占位;缺失时该窗口保持黑色

### 常用调整

| 需求 | 修改位置 |
|---|---|
| 通道数 / 摄像头节点 | `src/main.cpp` 的 `SrcCfg_tab[]`(`.loaction` 填 `/dev/videoN` 中的 N) |
| 分辨率 | `src/capturer/decChannel.cpp` 的 `CAMERA_WIDTH` / `CAMERA_HEIGHT`(默认 1920×1080) |
| 宫格布局 | `src/analyzer/analyzer.cpp` 中 `cols` / `rows`(默认 2×2) |
| 启用 BSD 检测 | 取消 `src/analyzer/algoProcess.cpp` 中 `bsd_init()`/`bsd_run()` 注释,在 `CMakeLists.txt` 的 `sysLib_list` 加入 `${BSD_LIBS}`,并把 `bsd_person.model` 放到运行目录 |
| 退出方式 | 当前无优雅退出;按 `Ctrl+C` 由系统终止进程 |
