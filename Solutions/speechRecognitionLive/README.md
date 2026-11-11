# speechRecognitionLive

基于 RKNN 的实时语音识别(Whisper)演示程序:通过 ALSA 采集板端麦克风 PCM,维护 5 秒滑动窗口、每秒触发一次推理,识别结果打印到终端。

## 功能说明

- 采集设备固定为 `hw:0,0`,16kHz / S16_LE / 2 声道(硬件声卡物理 2 声道,单声道会在首次 `snd_pcm_readi` 返回 EIO);程序只取左声道
- 启动采集前通过 ALSA control 接口设置 `hw:0` 的 mixer 控件(`ACodec_LP ADC Switch`、`PGA Gain`、`Headset/Main Mic Switch`、`SAI2 SDI0 Loopback Switch` 等),用于建立麦克风到 ADC 的路由
- 采集线程维护 5 秒环形缓冲;每累计 1 秒采样点且缓冲已填满,就通知主线程做一次快照
- 主线程把最近的 5 秒 PCM 拷贝到 Whisper 的 30 秒输入缓冲开头(其余补零),放入深度 2 的推理队列;推理跟不上时丢弃新帧,不积压
- 推理线程调用 `speech_recognition_run()`,打印识别文本、单次耗时和 RTF
- `Ctrl+C`/`SIGTERM` 通过 `sigaction`(不带 `SA_RESTART`)中断阻塞的 `snd_pcm_readi()`,采集与推理线程可正常退出

## 编译前准备

### 1. 板端/编译环境依赖

```bash
apt install libasound2-dev
```

- `libasound2-dev` 提供 `alsa/asoundlib.h` 头文件、`libasound.so` 链接符号与 `alsa.pc`,编译期必需;未安装时编译报 `fatal error: alsa/asoundlib.h: No such file or directory`
- `speech_recognition/api.cmake` 用 `pkg_check_modules(FFTW3F REQUIRED fftw3f)` 与 `pkg_check_modules(SNDFILE REQUIRED sndfile)` 检查 `fftw3f`、`sndfile`。交叉编译时 pkg-config 必须能找到 sysroot 内的 `.pc`(设置 `PKG_CONFIG_SYSROOT_DIR=$SYSROOT`、`PKG_CONFIG_PATH=$SYSROOT/usr/lib/aarch64-linux-gnu/pkgconfig`),否则 configure 直接报 `Package 'fftw3f' not found`

### 2. Toolkit 预编译库

依赖 `easyeai-api/algorithm/speech_recognition` 的静态库(`libspeech_recognition.a` 等)。`speech_recognition/api.cmake` 直接按 `${CMAKE_BOARDSYS}` 取库目录(Debian 13 → `debian13/`,Ubuntu 26.04 → `ubuntu26.04/`),但仓库当前只提供 `debian13/libspeech_recognition.a`,**没有 ubuntu26.04 目录也没有回退逻辑**:

- Debian 13 板:可直接使用现有库
- Ubuntu 26.04 板:需要 `ubuntu26.04/` 目录下对应版本的 `libspeech_recognition.a`(当前仓库未提供),或为构建配置增加 debian13 回退

## 编译

```bash
./build.sh          # cmake -DCAPTURE_DEBUG=ON && make -j24,产物输出到 Release/
./build.sh clear    # 清理 build/ 与 Release/
```

`build.sh` 在 `$SYSROOT` 非空时会尝试把产物拷到 `$SYSROOT/userdata/Solu/speechRecognitionLive/`。若该路径在当前环境中不可写,拷贝步骤会失败,**不影响编译产物**,按下面"部署"手动操作即可。

可调编译参数:`CAPTURE_DEBUG=ON` 时每秒打印一次左右声道峰值,用于排查麦克风采集问题(默认由 build.sh 开启)。

## 部署与运行

### 部署

```bash
ssh root@<板端IP> "mkdir -p /userdata/Solu/speechRecognitionLive"
scp Release/speechRecognitionLive root@<板端IP>:/userdata/Solu/speechRecognitionLive/
```

### 运行(板端)

```bash
cd /userdata/Solu/speechRecognitionLive
./speechRecognitionLive <encoder> <decoder> <filter> <vocab> <task>
```

必须提供全部 5 个参数,个数不符时只打印 usage 并退出。

参数说明:

| 参数 | 含义 | 板端参考路径 |
|---|---|---|
| `encoder` | Whisper encoder RKNN 模型 | `/userdata/Demo/algorithm-speech_recognition/speech_encoder.model` |
| `decoder` | Whisper decoder RKNN 模型 | `/userdata/Demo/algorithm-speech_recognition/speech_decoder.model` |
| `filter` | mel 滤波器系数表 | `/userdata/Demo/algorithm-speech_recognition/filters.txt` |
| `vocab` | 词表文件(EN.txt / CN.txt) | `/userdata/Demo/algorithm-speech_recognition/CN.txt` |
| `task` | 任务类型:`cn`(中文)或 `en`(英文),其他值直接报错 | — |

示例(中文识别):

```bash
./speechRecognitionLive \
    /userdata/Demo/algorithm-speech_recognition/speech_encoder.model \
    /userdata/Demo/algorithm-speech_recognition/speech_decoder.model \
    /userdata/Demo/algorithm-speech_recognition/filters.txt \
    /userdata/Demo/algorithm-speech_recognition/CN.txt cn
```

### 采集说明

- 程序启动时会先 `drop` 一次 `hw:0,0`,清理上次 `kill -9` 遗留的 ALSA 状态,再重新打开设备
- 若无音频输出,可用 `CAPTURE_DEBUG` 的峰值打印确认采集是否正常,并用 `alsamixer` 检查录音增益与通道路由
- 退出方式:`Ctrl+C` 正常停止采集与推理;若进程卡死,可再按一次或 `kill -9`,下次启动会自动清理 ALSA 状态
