# vcic_stream — 车载摄像头视频流传输

基于 VCIC（ISO 17215）的双板协同实时视频传输方案：Server 端采集/编码并推流，Client 端接收/解码并送显。

数据面已改为 **GStreamer + Rockchip MPP**，不再直接调用 RK_MPI / Rockit：

- **Server（摄像头模式）**：V4L2 `/dev/videoN` → NV12 → `appsrc` → `mpph264enc`/`mpph265enc` → `h264parse`/`h265parse`（`config-interval=1`）→ AnnexB（`alignment=au`）→ `appsink` → VCIC 推流
- **Server（文件模式）**：读取 H.264/H.265 AnnexB 文件，解析成 Access Unit 后按指定帧率循环回调，无需摄像头
- **Client**：VCIC 分片重组 → `appsrc` → `h264parse`/`h265parse` → `mppvideodec` → NV12 → `appsink` → 拷入 4 个 DMA-BUF 环形缓冲 → `display_primary_commit()` zero-copy 送显

## 1. 使用方法

### 1.1 Server 端（摄像头 / 推流端，默认模式）

```bash
sudo ./vcic_stream_server [ifname] [local_ip] [width] [height] [framerate] [fmt] [camera]
# 默认值：eth0 / 任意 IP / 1920x1080 / 30fps / h264 / camera 62
# 示例：
sudo ./vcic_stream_server eth0 192.168.1.20 1920 1080 30 h264 62
```

- `camera` 可写索引或设备路径（如 `62`、`/dev/video62`），代码取字符串末尾数字作为索引；默认 **62**
- `fmt` 支持 `h264`、`h265`
- 摄像头帧由 `mipicamera_*`（V4L2）采集，固定输出 NV12 后送入 `mpph264enc`/`mpph265enc`

### 1.2 Server 端（文件模式）

```bash
sudo ./vcic_stream_server --file <h264_file> [ifname] [local_ip] [framerate] [fmt]
# 示例：
sudo ./vcic_stream_server --file miaobiao.h264 eth0 192.168.1.20 30 h264
```

- 文件必须是 H.264/H.265 AnnexB 裸码流（`.h264` / `.h265` / `.264` / `.265`）
- 文件读完自动从头循环，按 `framerate` 控制推送节奏
- 该模式无摄像头采集与 GStreamer 编码，但 Client 端仍需 MPP 解码插件

### 1.3 Server 终端命令

启动后进入命令循环，**推流不会自动开始**，需输入 `start`：

| 命令 | 说明 |
|------|------|
| `start` | 开始推流（启动摄像头采集编码或文件读取线程） |
| `stop` | 停止推流并释放数据源 |
| `status` | 打印当前模式、分辨率/帧率、帧数/分片数/字节数 |
| `quit` / `q` | 退出程序 |

推流过程中 Server 每 5 秒主动发送一次 `VCIC_EVENT_CAM_STATUS` 事件。

### 1.4 Client 端（接收 / 显示端）

```bash
sudo ./vcic_stream_client <server_ip> [ifname] [local_ip] [fmt]
# 示例：
sudo ./vcic_stream_client 192.168.1.20 eth0 192.168.1.30 h264
```

- 默认 `ifname=eth0`、`fmt=h264`；`fmt` 必须与 Server 一致
- 客户端本地端口固定为 **30492**（避免与 Server 的 30491 冲突）
- 启动流程：初始化 `display_primary` → 启动 GStreamer 解码器 → 创建 VCIC Client → 订阅 `CAM_STATUS`/`FRAME_LOST` → 立即 `start_stream`
- 收到完整帧后送入解码器，解码出的 NV12 DMA-BUF 直接提交 primary 图层；主循环每 2 秒打印一次分片/帧/显示计数

### 1.5 编译

```bash
cd Solutions/vcic_stream
./build.sh
```

产物为 `Release/vcic_stream_server`、`Release/vcic_stream_client`；`$SYSROOT` 非空时会用 `sudo` 拷贝到 `$SYSROOT/userdata/Solu/vcic_stream/`。

## 2. 运行前置条件

### 2.1 网络配置

两块板子通过网线连接到同一局域网（同一网段），或直接网线对接。Server 与 Client 的 `stream_id`、组播 MAC、`fmt` 必须一致。

### 2.2 权限

AVTP 使用裸以太网 socket（Layer 2），两端都需要 root 或 `CAP_NET_RAW`：

```bash
sudo setcap cap_net_raw+ep ./vcic_stream_server
sudo setcap cap_net_raw+ep ./vcic_stream_client
```

Client 还需要 DRM master 才能送显，通常直接 `sudo` 运行；若板端有桌面，先 `systemctl stop gdm` 释放 DRM。

### 2.3 GStreamer / MPP 插件

运行时依赖 Rockchip GStreamer 插件：

```bash
gst-inspect-1.0 mpph264enc    # Server 摄像头模式
gst-inspect-1.0 mpph265enc    # fmt=h265 时
gst-inspect-1.0 mppvideodec   # Client 解码
gst-inspect-1.0 appsrc
gst-inspect-1.0 appsink
```

Server 摄像头模式还需 `h264parse`/`h265parse`；文件模式不需要编码器，但 Client 仍需 `mppvideodec`。

### 2.4 摄像头设备节点（仅摄像头模式）

```bash
ls /dev/video*                                # 确认设备节点
v4l2-ctl -d /dev/video62 --list-formats-ext   # 查看支持的分辨率和格式
```

默认使用 `/dev/video62`，可通过命令行第 7 个参数覆盖。已移除对 `librockit.so` 与 `rkaiq_3A.service` 的依赖。

### 2.5 日志目录

程序使用 `log_manager_pro`，需要 `/userdata/logs/` 存在：

- Server：`/userdata/logs/vcic_stream_server.ini`
- Client：`/userdata/logs/vcic_stream_client.ini`

## 3. VCIC 闭源库接口介绍

VCIC（Vehicle Camera Interface Consortium）是基于 ISO 17215 标准的车载摄像头接口通信库，以闭源静态库形式提供（`libvcic.a`），头文件为 `vcic.h`。

### 3.1 Server 端 API（摄像头侧）

| 接口 | 说明 |
|------|------|
| `VCIC_server_create(cfg)` | 创建服务实例，配置网络接口、服务 ID、组播 MAC、编解码格式等 |
| `VCIC_server_destroy(h)` | 销毁服务实例，释放资源 |
| `VCIC_server_register_feed(h, pObj, feed_cb)` | 注册视频帧数据源回调，域控请求取流时循环调用获取视频帧 |
| `VCIC_server_send_event(h, eventId, data, len)` | 向已订阅的域控推送事件通知（帧丢失 / 摄像头状态） |

### 3.2 Client 端 API（域控侧）

| 接口 | 说明 |
|------|------|
| `VCIC_client_create(cfg)` | 创建客户端实例，配置网络接口、Server IP、组播 MAC 等 |
| `VCIC_client_destroy(h)` | 销毁客户端实例，释放资源 |
| `VCIC_client_start_stream(h, pObj, frame_cb)` | 向摄像头发送取流请求，成功后通过回调接收视频帧 |
| `VCIC_client_stop_stream(h)` | 停止取流 |
| `VCIC_client_set_param(h, param_id, data, len)` | 设置摄像头参数（分辨率 / 帧率 / 码率 / 翻转 / 编码格式 / 画质） |
| `VCIC_client_get_param(h, param_id, resp, resp_len)` | 查询摄像头参数 |
| `VCIC_client_subscribe_event(h, pObj, eventId, event_cb)` | 订阅事件通知（帧丢失 / 摄像头状态变化） |
| `VCIC_client_unsubscribe_event(h, eventId)` | 取消订阅事件 |

### 3.3 回调函数类型

| 回调类型 | 侧 | 说明 |
|----------|------|------|
| `VCIC_Feed_CB` | Server | 数据源回调，提供视频帧数据（指针 + 长度 + 是否末帧） |
| `VCIC_Frame_CB` | Client | 视频帧回调，接收视频帧（数据 + 长度 + AVTP 时间戳 + 编码格式） |
| `VCIC_Event_CB` | Client | 事件回调，接收事件通知（事件 ID + 数据） |

### 3.4 配置结构体

**VCIC_ServerConfig_t**（摄像头侧）：

| 字段 | 说明 |
|------|------|
| `ifname` | 网络接口，如 `"eth0"` |
| `local_ip` | 本机 IP，空串则自动选择 |
| `service_id` | 服务 ID，通常 `VCIC_SERVICE_ID`（0x0101） |
| `instance_id` | 实例 ID，通常 `VCIC_INSTANCE_ID`（0x0001） |
| `app_port` | 应用端口，通常 `VCIC_APP_PORT`（30491） |
| `stream_id[8]` | AVTP 数据流 ID |
| `dst_mac[6]` | 组播目标 MAC 地址 |
| `codec` | 编解码格式：`VCIC_CODEC_H264` / `VCIC_CODEC_H265` |

**VCIC_ClientConfig_t**（域控侧）：

| 字段 | 说明 |
|------|------|
| `ifname` | 网络接口 |
| `local_ip` | 本机 IP |
| `service_id` | 服务 ID |
| `instance_id` | 实例 ID |
| `local_port` | 本地端口（本示例固定 30492） |
| `server_ip` | 摄像头 SoC 的 IP 地址 |
| `server_port` | 摄像头 SoC 应用端口（本示例为 30491） |
| `stream_id[8]` | 数据流 ID（需与摄像头侧一致） |
| `multicast_mac[6]` | 组播 MAC（需与摄像头侧一致） |

### 3.5 事件与参数 ID

**事件 ID**：

| 宏 | 值 | 说明 |
|------|------|------|
| `VCIC_EVENT_FRAME_LOST` | 0x8001 | 帧丢失事件（Client 收到后清空重组缓冲） |
| `VCIC_EVENT_CAM_STATUS` | 0x8002 | 摄像头状态变化事件（Server 推流时每 5 秒发送） |

**参数 ID**（`VCIC_client_set_param` / `VCIC_client_get_param` 使用）：

| 宏 | 值 | 数据格式 |
|------|------|------|
| `VCIC_PARAM_RESOLUTION` | 0x0001 | `uint16_t width, uint16_t height` |
| `VCIC_PARAM_FRAMERATE` | 0x0002 | `uint8_t fps` |
| `VCIC_PARAM_BITRATE` | 0x0003 | `uint32_t bps` |
| `VCIC_PARAM_FLIP` | 0x0004 | `uint8_t`: 0=none, 1=H, 2=V, 3=HV |
| `VCIC_PARAM_CODEC` | 0x0005 | `uint8_t`: 1=H.264, 2=H.265 |
| `VCIC_PARAM_QUALITY` | 0x0006 | `uint8_t`: 1~10 |

## 4. 默认网络参数

Server 与 Client 代码中写死了相同的流标识，修改时两端必须同步：

| 常量 | 默认值 |
|------|--------|
| `STREAM_ID` | `00 1B 21 FF FE 00 00 01` |
| `MCAST_MAC` | `91:E0:F0:00:0E:80` |
| Server `app_port` | 30491 |
| Client `local_port` | 30492 |

## 5. 编译前准备

### 5.1 板端依赖（编译期）

编译期需要 GStreamer 开发包（提供 `.pc` 与头文件），运行时需要 Rockchip MPP 插件包。不同板端镜像的包名可能不同，可用 `gst-inspect-1.0 mppvideodec` 确认插件是否就绪。

`CMakeLists.txt` 通过 pkg-config 检查以下模块，缺任何一个都会在 configure 阶段失败：

- `gstreamer-1.0`
- `gstreamer-app-1.0`
- `gstreamer-video-1.0`

交叉编译时 pkg-config 需指向 sysroot（`PKG_CONFIG_SYSROOT_DIR` / `PKG_CONFIG_PATH`），否则可能找到主机的 x86 `.pc`。

### 5.2 预编译库

`easyeai-api/env.cmake` 从 `${CMAKE_SYSROOT}/etc/os-release` 计算 `CMAKE_BOARDSYS`(Debian 13 → `debian13`，Ubuntu 26.04 → `ubuntu26.04`):

- `netProtocol/vcic`:直接按 `${CMAKE_BOARDSYS}` 查找，仓库同时提供 `debian13/` 与 `ubuntu26.04/`，无跨平台回退
- `media/display`:直接按 `${CMAKE_BOARDSYS}` 查找，仓库同时提供 `debian13/` 与 `ubuntu26.04/`，无跨平台回退
- `common/log_manager_pro`:优先对应平台目录，缺失时回退 `debian13/`
- `common/system_opt`、`media/rga`:直接编译源码

## 6. 常见问题

| 现象 | 原因 / 处理 |
|------|-------------|
| `VCIC_server_create failed` | 网卡名 / IP 不正确，或缺少 `CAP_NET_RAW` |
| Client 一直无画面 | Server 未输入 `start`；或两端 `stream_id`/组播 MAC/`fmt` 不一致 |
| `no element "mppvideodec"` / `"mpph264enc"` | 缺少 Rockchip GStreamer 插件或 dev 包 |
| `display_primary_init failed` | 无可用显示器/连接器；或未取得 DRM master |
| 提交成功但屏幕黑屏 | 桌面环境占用 DRM，先 `systemctl stop gdm` |
| `gst_parse_launch failed` | GStreamer 插件缺失或管线 caps 不匹配（分辨率/格式） |
| 首次 `Ctrl+C` 未退出 | 程序进入清理流程；再按一次会强制 `_exit` |
