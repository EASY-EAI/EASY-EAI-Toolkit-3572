# rtspIPCamera

RTSP 摄像头推流示例:把板端两路 MIPI 摄像头(NV12)经 MPP 硬件编码为 H.264,再以 RTSP 服务对外推流,可用 VLC 等客户端拉流观看。

## 功能说明

- 为 `SrcCfg_tab` 中每路源创建一个 GStreamer RTSP media factory,管线为:
  `appsrc name=videosrc ! mpph264enc ! rtph264pay name=pay0 pt=96`
- `appsrc` 的 `need-data` 回调里调用 `mipicamera_getframe` 取 NV12 帧并 `push-buffer`,`mpph264enc` 走 Rockchip MPP 硬件编码
- 挂载路径为 `/test<index>`(index 从 0 开始),默认端口 **8554**
- 两路默认源:`/dev/video62`、`/dev/video71`,均为 1920×1080@30fps NV12,编码 H.264

## 编译前准备

### 交叉编译环境(编译容器 easyeai-compiler-2404)

- 编译环境需已导出 `SYSROOT=/mnt`(板端根文件系统 NFS4 挂载点),`build.sh` 依赖它定位 sysroot 与部署路径
- 交叉工具链 `aarch64-linux-gnu-gcc`(Ubuntu 24.04, glibc 2.39),板端 Debian 13 (glibc 2.43)

### 板端依赖(编译期必需)

```bash
apt install libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev libgstrtspserver-1.0-dev
```

- `gst_opt/api.cmake` 用 `pkg_search_module(... REQUIRED)` 检查 `gstreamer-1.0`、`gstreamer-video-1.0`、`gstreamer-rtsp-server-1.0` 三个模块
- 板端常见情况是**只有 GStreamer 运行时库、没有 `.pc` 与头文件**,此时 configure 阶段报 `None of the required 'gstreamer-1.0' found`,这是板端缺 dev 包,不是 SYSROOT 问题
- 运行时还需 Rockchip GStreamer 插件:`mpph264enc`(MPP 硬编)、`rtph264pay`。用 `gst-inspect-1.0 mpph264enc` 确认存在

### 预编译库

仅依赖 `easyeai-api` 的 `media/gst_opt` 与 `common/system_opt`。`gst_opt` 的 `debian13/` 预编译库已随仓库提供;缺失时到 SDK 源码 `/opt/Toolkit-C-SDK/Toolkit-C-SDK_3572/api_src/media_api/` 下 `./build.sh gst_opt` 重编并拷入 `easyeai-api/media/gst_opt/debian13/`。

## 编译

```bash
./build.sh          # 编译,产物输出到 Release/rtspIPCamera
./build.sh clear    # 清理 build/ 与 Release/
```

`build.sh` 会尝试拷贝到 `$SYSROOT/userdata/Solu/rtspIPCamera/`;`/mnt/userdata/` 对编译容器内 developer 用户无写权限(NFS root squash),该步骤会 Permission denied,**不影响编译产物**,请手动部署。

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

无必需参数。**注意:`-p/--port` 虽然被 GOption 解析,但 `g_object_set(server, "service", ...)` 实际用的是编译期常量 `DEFAULT_RTSP_PORT`(8554),命令行 `-p` 目前不生效**;要改端口请直接改 `src/main.cpp` 的 `DEFAULT_RTSP_PORT` 后重新编译。

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

1. 摄像头设备节点存在(`/dev/video62`、`/dev/video71`,按板端实际 CSI 编号调整)
2. 板端与客户端网络互通,防火墙放行 8554/TCP
3. 需要 `mpph264enc` 插件;若日志报 `no element "mpph264enc"`,说明 Rockchip GStreamer 插件未安装

### 常用调整

| 需求 | 修改位置 |
|---|---|
| 摄像头节点 / 分辨率 / 帧率 / 码流类型 | `src/main.cpp` 的 `SrcCfg_tab[]`(`.loaction` 为 `/dev/videoXX`,`.videoDesc` 里是 `NV12,1920,1080,30,...,"h264"`) |
| 监听端口默认值 | `src/main.cpp` 的 `DEFAULT_RTSP_PORT` |
| 挂载路径 | `src/main.cpp` 的 `ATTACH_URL`(默认 `/test`) |
| 增加/减少路数 | 在 `SrcCfg_tab[]` 中增删条目,代码会按数组长度自动建流 |
