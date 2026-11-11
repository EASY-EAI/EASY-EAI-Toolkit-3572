# rtspIPCamera

RTSP 摄像头推流示例:把板端两路 V4L2 摄像头(NV12)经 MPP 硬件编码为 H.264,再以 RTSP 服务对外推流,可用 VLC 等客户端拉流观看。

## 功能说明

- 为 `SrcCfg_tab` 中每路源创建一个 GStreamer RTSP media factory,管线为:
  `appsrc name=videosrc ! mpph264enc ! rtph264pay name=pay0 pt=96`
- `appsrc` 的 `need-data` 回调里调用 `mipicamera_getframe` 取 NV12 帧并 `push-buffer`,`mpph264enc` 走 Rockchip MPP 硬件编码
- 摄像头节点由 `loaction` 末尾两位数字解析成索引(`/dev/video62` → 62),`appSrc_Init()` 中调用 `mipicamera_set_format(..., RK_FORMAT_YCbCr_420_SP)` 固定按 NV12 采集
- `gst_rtsp_media_factory_set_shared(factory, TRUE)` 允许多个客户端共享同一路流;每路挂载路径为 `/test<index>`(index 从 0 开始),端口固定 **8554**
- 两路默认源:`/dev/video62`、`/dev/video71`,均为 1920×1080@30fps NV12,编码 H.264

## 编译前准备

### 板端依赖(编译期必需)

```bash
apt install libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev libgstrtspserver-1.0-dev
```

- `gst_opt/api.cmake` 用 `pkg_search_module(... REQUIRED)` 检查 `gstreamer-1.0`、`gstreamer-video-1.0`、`gstreamer-rtsp-server-1.0` 三个模块
- 板端常见情况是**只有 GStreamer 运行时库、没有 `.pc` 与头文件**,此时 configure 阶段报 `None of the required 'gstreamer-1.0' found`,这是板端缺 dev 包,不是 SYSROOT 问题
- 运行时还需 Rockchip GStreamer 插件:`mpph264enc`(MPP 硬编)、`rtph264pay`。用 `gst-inspect-1.0 mpph264enc` 确认存在

### 预编译库

仅依赖 `easyeai-api` 的 `media/gst_opt` 与 `common/system_opt`。`gst_opt/api.cmake` 直接按 `${CMAKE_BOARDSYS}` 找库(Debian 13 → `media/gst_opt/debian13/`,Ubuntu 26.04 → `media/gst_opt/ubuntu26.04/`,两个目录均已提供,无跨平台回退);`system_opt` 直接编译源码。

## 编译

```bash
./build.sh          # 编译,产物输出到 Release/rtspIPCamera
./build.sh clear    # 清理 build/ 与 Release/
```

`build.sh` 会尝试把产物拷到 `$SYSROOT/userdata/Solu/rtspIPCamera/`;若该路径在当前环境中不可写,拷贝步骤会失败,**不影响编译产物**,请手动部署。

## 部署与运行

### 部署

```bash
mkdir -p /userdata/Solu/rtspIPCamera
cp Release/rtspIPCamera /userdata/Solu/rtspIPCamera/
chmod +x /userdata/Solu/rtspIPCamera/rtspIPCamera
```

### 运行(板端)

```bash
cd /userdata/Solu/rtspIPCamera
./rtspIPCamera
```

无必需参数。**注意:`-p/--port` 虽然被 GOption 解析,但 `g_object_set(server, "service", ...)` 实际用的是编译期常量 `DEFAULT_RTSP_PORT`(8554),命令行 `-p` 目前不生效**;要改端口请直接改 `src/main.cpp` 的 `DEFAULT_RTSP_PORT` 后重新编译。程序没有信号处理,`Ctrl+C` 直接终止。

启动成功会打印每路挂载地址,例如:

```
stream ready at rtsp://127.0.0.1:8554/test0
stream ready at rtsp://127.0.0.1:8554/test1
```

### 拉流验证

在 PC 或同网段设备上:

```bash
# VLC
vlc rtsp://<板端IP>:8554/test0
# 或 ffplay
ffplay -rtsp_transport tcp rtsp://<板端IP>:8554/test0
```

运行前置条件:

1. 摄像头设备节点存在(`/dev/video62`、`/dev/video71`,按板端实际 CSI 编号调整);`loaction` 需以两位数字结尾,否则索引解析为 0
2. 板端与客户端网络互通,防火墙放行 8554/TCP
3. 需要 `mpph264enc` 插件;若日志报 `no element "mpph264enc"`,说明 Rockchip GStreamer 插件未安装

### 常用调整

| 需求 | 修改位置 |
|---|---|
| 摄像头节点 / 分辨率 / 帧率 / 码流类型 | `src/main.cpp` 的 `SrcCfg_tab[]`(`.loaction` 为 `/dev/videoXX`,`.videoDesc` 里是 `NV12,1920,1080,30,...,"h264"`) |
| 监听端口默认值 | `src/main.cpp` 的 `DEFAULT_RTSP_PORT` |
| 挂载路径 | `src/main.cpp` 的 `ATTACH_URL`(默认 `/test`) |
| 增加/减少路数 | 在 `SrcCfg_tab[]` 中增删条目,代码会按数组长度自动建流 |
| 改为 H.265 | 把 `.videoDesc` 的 `videoDstType` 改为 `"h265"`,管线会自动生成 `mpph265enc` / `rtph265pay` |
