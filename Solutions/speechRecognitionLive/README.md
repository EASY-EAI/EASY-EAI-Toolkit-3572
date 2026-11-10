# speechRecognitionLive

基于 RKNN 的实时语音识别(Whisper)演示程序:通过 ALSA 采集板端麦克风 PCM,维护 5 秒滑动窗口、每秒触发一次推理,识别结果打印到终端。

## 编译前准备

### 1. 交叉编译环境(编译容器 easyeai-compiler-2404)

- 编译环境需已导出 `SYSROOT=/mnt`(板端根文件系统的 NFS4 挂载点),`build.sh` 依赖它定位 sysroot 与部署路径
- 交叉工具链:`aarch64-linux-gnu-gcc`(Ubuntu 24.04, glibc 2.39);板端为 Debian 13 (trixie, glibc 2.43),链接使用 `-fuse-ld=gold`,无版本校验问题。

### 2. 板端依赖(在目标板上执行,root@192.168.88.146)

```bash
apt install libasound2-dev
```

- `libasound2-dev` 提供 `alsa/asoundlib.h` 头文件、`libasound.so` 链接符号与 `alsa.pc`,编译期必需;未安装时编译报 `fatal error: alsa/asoundlib.h: No such file or directory`。
- `fftw3f`、`sndfile` 板端已有(pkg-config 已确认),无需额外安装。

### 3. Toolkit 预编译库

依赖 `easyeai-api/algorithm/speech_recognition` 的 `debian13/` 预编译库(`libspeech_recognition.a` 等),已随仓库提供。若缺失,到 SDK 源码 `/opt/Toolkit-C-SDK/Toolkit-C-SDK_3572/api_src/algorithm_api/` 下执行 `./build.sh speech_recognition` 重编并拷入 `easyeai-api/algorithm/speech_recognition/debian13/`。

## 编译

```bash
./build.sh          # 编译,产物输出到 Release/speechRecognitionLive
./build.sh clear    # 清理 build/ 与 Release/
```

build.sh 内部执行 `cmake .. -DCAPTURE_DEBUG=ON && make`,并尝试把产物拷贝到 `$SYSROOT/userdata/Solu/speechRecognitionLive/`。注意:`/mnt/userdata/` 对编译容器内的 developer 用户无写权限(NFS root squash),该拷贝步骤会 Permission denied,**不影响编译产物**,按下面"部署"手动操作即可。

可调编译参数:`CAPTURE_DEBUG=ON` 时打印采集峰值,用于排查麦克风采集问题(默认由 build.sh 开启)。

## 部署与运行

### 部署

```bash
# 在编译容器上(假设板端 /userdata 已可通过 NFS 访问;否则用 scp/U 盘)
ssh root@192.168.88.146 "mkdir -p /userdata/Solu/speechRecognitionLive"
cp ../Release/speechRecognitionLive /mnt/userdata/Solu/speechRecognitionLive/  # 若权限允许
# 或在板端直接从 NFS 路径拷贝
```

### 运行(板端)

```bash
cd /userdata/Solu/speechRecognitionLive
./speechRecognitionLive <encoder> <decoder> <filter> <vocab> <task>
```

参数说明:

| 参数 | 含义 | 板端参考路径 |
|---|---|---|
| `encoder` | Whisper encoder RKNN 模型 | `/userdata/Demo/algorithm-speech_recognition/speech_encoder.model` |
| `decoder` | Whisper decoder RKNN 模型 | `/userdata/Demo/algorithm-speech_recognition/speech_decoder.model` |
| `filter` | mel 滤波器系数表 | `/userdata/Demo/algorithm-speech_recognition/filters.txt` |
| `vocab` | 词表文件(EN.txt / CN.txt) | `/userdata/Demo/algorithm-speech_recognition/CN.txt` |
| `task` | 任务类型:`cn`(中文)或 `en`(英文) | — |

示例(中文识别):

```bash
./speechRecognitionLive \
    /userdata/Demo/algorithm-speech_recognition/speech_encoder.model \
    /userdata/Demo/algorithm-speech_recognition/speech_decoder.model \
    /userdata/Demo/algorithm-speech_recognition/filters.txt \
    /userdata/Demo/algorithm-speech_recognition/CN.txt cn
```

### 采集说明

- 采集设备固定为 `hw:0,0`,16kHz / 16-bit / 立体声(硬件声卡物理 2 声道,单声道无法打开),程序取左声道。
- `Ctrl+C` 退出:信号处理会用 `EINTR` 中断阻塞的 `snd_pcm_readi`,采集线程正常退出,不会卡死。
- 若无音频输出,可用 `CAPTURE_DEBUG` 的峰值打印确认采集是否正常,并用 `alsamixer` 检查录音增益与通道路由(程序内部通过 ALSA ctl 接口设置了部分 `hw:0` 控件)。
