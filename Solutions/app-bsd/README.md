# app-bsd

盲区检测(BSD, Blind Spot Detection)示例:从板端 MIPI 摄像头取流,在屏幕上以 2×2 宫格显示 4 路画面。

## 功能说明

- 4 路 MIPI 摄像头通道(`SrcCfg_tab` 中 `srcType="AHD"`、`loaction` 为通道号 1~4)各自一个采集线程,从 `/dev/video*` 取 NV12 帧
- 每帧交给 `analyzer` 模块,按屏幕分辨率绘制 2×2 宫格布局
- 通过 `display_pro` 的 DRM 接口直接把画布送到 primary 图层全屏显示(单缓冲区一次导入、长期驻留,避免每帧重复导入)
- 算法部分当前是空实现:`analyzer/algoProcess.cpp` 中 `bsd_init()` 被注释,`algorithm_process()` 只返回空结果,所以**画面只有视频、没有检测框**。需要时取消注释并放置 `bsd_person.model` 即可接入

## 编译前准备

### 交叉编译环境(编译容器 easyeai-compiler-2404)

- 编译环境需已导出 `SYSROOT=/mnt`(板端根文件系统的 NFS4 挂载点),`build.sh` 依赖它定位 sysroot 与部署路径
- 交叉工具链 `aarch64-linux-gnu-gcc`(Ubuntu 24.04, glibc 2.39),板端 Debian 13 (glibc 2.43)

### 板端依赖

```bash
apt install libdrm-dev libopencv-dev
```

- `libdrm-dev`:`display` 模块走 DRM/KMS
- `libopencv-dev`:`opencv_core` / `opencv_imgproc` / `opencv_imgcodecs`(本工程直接用 OpenCV 画宫格与画框)
- 若缺 OpenCV 头文件,编译报 `opencv2/opencv.hpp: No such file or directory`;缺 `.so` 则链接报 `cannot find -lopencv_core`。与 `alsa`、`gstreamer` 一样,板端常只装运行时库,需显式装 dev 包

### 预编译库

依赖 `easyeai-api` 下 `display`、`log_manager_pro`、`algorithm/bsd` 的 `debian13/` 预编译库,已随仓库提供。若缺失,到 SDK 源码 `/opt/Toolkit-C-SDK/Toolkit-C-SDK_3572/api_src/` 对应 `media_api`、`common_api`、`algorithm_api` 目录下用 `./build.sh <模块名>` 重编并拷入。

## 编译

```bash
./build.sh          # 编译,产物输出到 Release/app-bsd
./build.sh clear    # 清理 build/ 与 Release/app-bsd
./build.sh cpres    # 编译并把 Release/ 全部文件拷到板端
```

`build.sh` 会把产物拷到 `$SYSROOT/userdata/Solu/app-bsd/`。由于 `/mnt/userdata/` 对编译容器内 developer 用户无写权限(NFS root squash),该步骤会 Permission denied,**不影响编译产物**,请按下面"部署"手动拷贝。

## 部署与运行

### 部署

```bash
cp Release/app-bsd /mnt/userdata/Solu/app-bsd/     # 在板端执行可用 root,或从板端自身文件系统拷贝
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
3. 摄像头设备节点存在(默认 `/dev/video23`、`video31` 由 `mipicamera_init` 按索引映射,索引来自 `loaction` 字段)
4. `/userdata/logs/` 目录存在,程序会初始化日志 `/userdata/logs/app_bsd.ini`

### 常用调整

| 需求 | 修改位置 |
|---|---|
| 通道数 / 通道号 | `src/main.cpp` 的 `SrcCfg_tab[]`(`.loaction` 填通道号) |
| 分辨率 | `src/capturer/decChannel.cpp` 的 `CAMERA_WIDTH` / `CAMERA_HEIGHT`(默认 1920×1080) |
| 宫格布局 | `src/analyzer/analyzer.cpp` 中 `cols` / `rows`(默认 2×2) |
| 启用 BSD 检测 | `src/analyzer/algoProcess.cpp` 取消 `bsd_init(&gBSDCtx, "bsd_person.model")` 注释,并把 `bsd_person.model` 放到运行目录 |

`Ctrl+C` 退出(`g_exitRequested` 标志),会依次释放各通道与算法上下文。
