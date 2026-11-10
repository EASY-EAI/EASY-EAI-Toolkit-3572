# detect-Person

人员检测示例:从 CSI 摄像头实时取流,用 RKNN 人员检测模型推理,把检测框与置信度叠加到画面后全屏显示。

## 功能说明

- 打开一路 MIPI/CSI 摄像头(默认 1920×1080),`mipicamera_getframe` 循环取帧
- 独立推理线程(`detect_thread_entry`)调用 `person_detect` 模块,主线程负责取流与显示,两者通过互斥锁共享图像
- 主线程绘制检测框(`plot_one_box`,置信度 ≥ 0.4 才画)与 `名字 置信度%` 标签
- 画面经 `display` 模块的 `display_primary_commit` 送到 primary 图层全屏显示

## 编译前准备

### 交叉编译环境(编译容器 easyeai-compiler-2404)

- 编译环境需已导出 `SYSROOT=/mnt`(板端根文件系统的 NFS4 挂载点),`build.sh` 依赖它定位 sysroot 与部署路径
- 交叉工具链 `aarch64-linux-gnu-gcc`(Ubuntu 24.04, glibc 2.39),板端 Debian 13 (glibc 2.43)

### 板端依赖

```bash
apt install libdrm-dev libopencv-dev freetype
```

- `libdrm-dev`:`display` 模块走 DRM/KMS
- `libopencv-dev`:`opencv_core` / `opencv_imgproc` / `opencv_imgcodecs`
- `freetype`:`font_engine` 模块的 pkg-config 检查(`freetype2`)必需,pkg-config 找不到时报 `None of the required 'freetype2' found`;缺头文件报 `ft2build.h: No such file or directory`

### 预编译库

依赖 `easyeai-api` 下 `display`、`log_manager_pro`、`font_engine`、`algorithm/person_detect` 的 `debian13/` 预编译库,已随仓库提供。缺失时到 SDK 源码 `/opt/Toolkit-C-SDK/Toolkit-C-SDK_3572/api_src/` 对应目录 `./build.sh <模块名>` 重编后拷入。

## 编译

```bash
./build.sh          # 编译,产物输出到 Release/detect-Person
./build.sh clear    # 清理 build/ 与 Release/detect-Person
./build.sh cpres    # 编译并把 Release/ 全部文件拷到板端
```

`build.sh` 会尝试拷贝到 `$SYSROOT/userdata/Solu/detect-Person/`;`/mnt/userdata/` 对编译容器内 developer 用户无写权限(NFS root squash),该步骤 Permission denied **不影响编译产物**,请手动部署。

## 部署与运行

### 部署

```bash
mkdir -p /userdata/Solu/detect-Person
cp Release/detect-Person /userdata/Solu/detect-Person/
# 模型必须与可执行文件在同一目录(代码用相对路径 "person_detect.model")
cp <模型路径>/person_detect.model /userdata/Solu/detect-Person/
chmod +x /userdata/Solu/detect-Person/detect-Person
```

### 运行(板端,需 root)

```bash
cd /userdata/Solu/detect-Person
sudo ./detect-Person <23/31>
```

参数:

| 参数 | 含义 |
|---|---|
| `23` | csi0,对应 `/dev/video23` |
| `31` | csi1,对应 `/dev/video31` |

参数缺失时会打印 usage 并直接返回。

运行前置条件:

1. **需要 root**:DRM/KMS 送显需取得 DRM master
2. **桌面环境不能占用 DRM**:先停桌面(如 `systemctl stop gdm`),否则画面不可见
3. **模型放在工作目录**:代码中写死相对路径 `person_detect.model`,必须在运行目录下;模型缺失时日志报 `person_detect_init failed, model: person_detect.model`
4. 日志初始化需要 `/userdata/logs/`,配置文件 `/userdata/logs/detect_person.ini`

### 常用调整

| 需求 | 修改位置 |
|---|---|
| 分辨率 | `src/main.cpp` 的 `CAMERA_WIDTH` / `CAMERA_HEIGHT`(默认 1920×1080) |
| 置信度门槛 | `src/main.cpp` 主循环中 `if (det_result->prop < 0.4)` |
| 跳过起始帧数 | `src/main.cpp` 的 `skip = 10` |
| 帧间隔 | 主循环末尾 `usleep(15*1000)` |
