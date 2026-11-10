# rtspMulitPlayer

多路 RTSP 拉流、硬件解码并轮播显示到屏幕的示例程序（RK3572 / Ubuntu 26.04）。

## 功能说明

- GStreamer 拉取 RTSP 流：`rtspsrc` → `rtph264depay` / `rtph265depay` → `h264parse` / `h265parse` → `mppvideodec`（MPP 硬解）→ `appsink`
- 解码后的 NV12 帧经 RGA 转为 BGR，按通道缓存到内存
- 显示线程每 10 秒切换一路通道，将当前帧经 RGA 做 `BGR → BGRX8888` 转换和 270° 旋转后，交给 `display` 模块的 primary 图层全屏显示（缩放由显示层完成）
- 通道尚未收到任何码流时显示 `noSignal.jpg`
- 目标检测算法当前未启用（`algoProcess.cpp` 中的模型调用已注释），只保留结果绘制接口

## 运行前置条件（板端）

1. 板子系统为 Ubuntu 26.04 (aarch64)，且已安装运行时依赖：
   - GStreamer 及 Rockchip 插件（`rtspsrc`、`rtph264depay`/`rtph265depay`、`h264parse`/`h265parse`、`mppvideodec`、`appsink`）
   - librga（RGA 硬件加速）
   - OpenCV（`opencv_core`、`opencv_imgproc`、`opencv_imgcodecs`）
2. **需要 root 权限运行**：显示模块通过 DRM/KMS 送显，需要取得 DRM master。
3. **桌面环境不能占用 DRM**：GNOME/gdm 会持有 DRM master 并关闭输出，此时程序提交成功但画面不可见。运行前请停止桌面：

   ```bash
   sudo systemctl stop gdm
   ```

   运行结束后恢复桌面：

   ```bash
   sudo systemctl start gdm
   ```

4. RTSP 摄像头与本机网络互通，且账号密码、地址正确。
5. 程序的工作目录下必须有 `noSignal.jpg`（`Release/` 目录中已提供）。

## 编译前置条件（主机）

- 已配置 aarch64 交叉工具链，默认读取 `~/configs/cross.cmake`（工具链 + sysroot）
- sysroot 中已包含目标系统的 OpenCV、GStreamer、libdrm 等开发文件
- `easyeai-api` 下已提供 ubuntu26.04 的预编译静态库（`display`、`gst_opt`、`rga`、`log_manager_pro` 等），无需单独编译 SDK

## 编译

在 `Solutions/rtspMulitPlayer` 目录执行：

```bash
export SYSROOT=/path/to/rootfs     # 例如 /home/jango/mnt
./build.sh
```

`build.sh` 会：

1. 清空并重新生成 `build/`
2. `cmake .. && make -j24`
3. 将可执行文件拷贝到 `Release/`
4. 将 `Release/<程序>` 拷贝到 `$SYSROOT/userdata/Solu/rtspMulitPlayer/`

只编译、不部署到 sysroot 时，可直接使用：

```bash
rm -rf build && mkdir build && cd build
cmake .. && make -j4
```

清理编译产物：

```bash
./build.sh clear
```

## 部署到板子

方式一：编译时已通过 `SYSROOT` 拷贝到 rootfs，直接烧写/同步即可。

方式二：手动拷贝 `Release/` 下的全部文件（可执行文件和 `noSignal.jpg`）：

```bash
scp Release/rtspMulitPlayer Release/noSignal.jpg <user>@<board>:/userdata/Solu/rtspMulitPlayer/
```

## 运行

在板子终端直接执行（非 adb）：

```bash
sudo systemctl stop gdm            # 释放 DRM master
cd /userdata/Solu/rtspMulitPlayer
sudo ./rtspMulitPlayer
```

按 `Ctrl+C` 结束程序；结束后可执行 `sudo systemctl start gdm` 恢复桌面。

## 修改视频源

视频源在 [src/main.cpp](src/main.cpp) 的 `SrcCfg_tab[]` 中配置，每项对应一路通道：

```c
static const SrcCfg_t SrcCfg_tab[] = {
    {
        .srcType      = "rtsp",
        .loaction     = "rtsp://admin:a12345678@192.168.5.66",
        .videoEncType = "h264",
        .audioEncType = "null",
    },
    {
        .srcType      = "rtsp",
        .loaction     = "rtsp://admin:a12345678@192.168.5.68",
        .videoEncType = "h264",
        .audioEncType = "null",
    }
};
```

- 通道数由数组元素个数决定，显示线程按数组顺序每 10 秒轮播
- `videoEncType` 支持 `h264`、`h265`
- 音频当前未启用（`audioEncType = "null"`）

修改后需重新编译并部署。

## 常见问题

| 现象 | 原因 / 处理 |
| --- | --- |
| 程序运行但屏幕黑屏 | 桌面环境仍占用 DRM master，先 `sudo systemctl stop gdm` |
| 终端提示 `Could not open resource for reading and writing` / `Failed to connect` | RTSP 地址、账号密码或网络不通，检查 `SrcCfg_tab` 和摄像头网络 |
| 显示的是 `noSignal.jpg` | 对应通道没有取到码流，或摄像头未推送视频 |
| 程序启动即退出：`display_primary_init failed` | 没有可用连接器（屏幕未连接/未使能）或 primary 图层不可用 |
| 画面方向不对 | 显示前固定做 270° 旋转；竖屏/横屏需求不同时可调整 `analyzer.cpp` 中的 `RGA_ROT_270` |
| 未检测到人员/目标 | 目标检测当前未启用，属预期行为 |
