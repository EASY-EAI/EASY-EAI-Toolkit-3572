# detect-Person

人员检测示例:从 CSI 摄像头实时取流,用 RKNN 人员检测模型推理,把检测框与置信度叠加到画面后全屏显示。

## 功能说明

- 启动时必须带一个摄像头索引参数(`23`/`31`),`mipicamera_init()` 打开 `/dev/video<index>`,默认 1920×1080
- `mipicamera_init()` 的默认输出格式为 `RK_FORMAT_BGR_888`,`mipicamera_getframe()` 内部用 RGA 把 sensor 的 NV12 转成 BGR,因此主循环直接把缓冲区包装成 `CV_8UC3` 的 `Mat` 使用
- 跳过前 10 帧后,主线程负责取流与显示,独立推理线程(`detect_thread_entry`)负责 `person_detect_run()`,两者通过 `img_lock` 互斥锁共享图像
- 推理线程启动时加载相对路径 `person_detect.model`;初始化失败只打印 `person_detect_init failed` 并退出该线程,主线程仍继续预览画面(无检测框)
- 主线程绘制置信度 ≥ 0.4 的检测框(`plot_one_box`)与 `名字 置信度%` 标签
- 送显前用 RGA 做 BGR→BGRX8888 转换并旋转 270°,再通过 `display_primary_commit()` 送到 primary 图层全屏显示(缩放由显示层完成)
- 日志使用 `log_manager_pro`,初始化 `/userdata/logs/detect_person.ini`

## 编译前准备

### 板端依赖

```bash
apt install libdrm-dev libopencv-dev libfreetype-dev
```

- `libdrm-dev`:`display` 模块走 DRM/KMS
- `libopencv-dev`:`opencv_core` / `opencv_imgproc` / `opencv_imgcodecs`(画框、写文字)
- `libfreetype-dev`:`font_engine` 模块的 `find_package(PkgConfig)` + `pkg_search_module(FREETYPE2 REQUIRED freetype2)` 检查必需;找不到时报 `None of the required 'freetype2' found`,缺头文件报 `ft2build.h: No such file or directory`

### 预编译库

`easyeai-api/env.cmake` 从 `${CMAKE_SYSROOT}/etc/os-release` 计算 `CMAKE_BOARDSYS`(Debian 13 → `debian13`,Ubuntu 26.04 → `ubuntu26.04`),各 `api.cmake` 按该目录取对应平台的库:

- `media/display`:直接按 `${CMAKE_BOARDSYS}` 查找,仓库同时提供 `debian13/` 与 `ubuntu26.04/`
- `common/log_manager_pro`:优先对应平台目录,缺失时回退 `debian13/`
- `algorithm/person_detect`:优先对应平台目录,不存在时回退 `debian13/libperson_detect.a`(仓库当前只有 `debian13/`,因此两个平台都可链接)
- `common/font_engine`、`common/base64`、`common/system_opt`、`media/rga`:直接编译源码

## 编译

```bash
./build.sh          # 编译,产物输出到 Release/detect-Person
./build.sh clear    # 清理 build/ 与 Release/detect-Person
./build.sh cpres    # 编译并把 Release/ 全部文件拷到板端
```

`build.sh` 会尝试把产物拷到 `$SYSROOT/userdata/Solu/detect-Person/`;若该路径在当前环境中不可写,拷贝步骤会失败,**不影响编译产物**,请手动部署。

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
3. **模型放在工作目录**:代码中写死相对路径 `person_detect.model`,必须在运行目录下;模型缺失时推理线程退出,画面仍会显示但没有检测框
4. 日志初始化需要 `/userdata/logs/`,配置文件 `/userdata/logs/detect_person.ini`
5. 退出方式:`Ctrl+C` 由系统终止进程,未注册信号处理与资源释放流程

### 常用调整

| 需求 | 修改位置 |
|---|---|
| 分辨率 | `src/main.cpp` 的 `CAMERA_WIDTH` / `CAMERA_HEIGHT`(默认 1920×1080) |
| 置信度门槛 | `src/main.cpp` 主循环中 `if (det_result->prop < 0.4)` |
| 跳过起始帧数 | `src/main.cpp` 的 `skip = 10` |
| 帧间隔 | 主循环末尾 `usleep(15*1000)` |
| 送显旋转方向 | `src/main.cpp` 的 `commitFrameToDisplay()` 中 `RGA_ROT_270` |
