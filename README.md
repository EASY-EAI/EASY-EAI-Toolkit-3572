<br/>
<br/>


[English](README_EN.md)

<br />
<br />

如果您初次接触本项目，您可以：
[查阅入门(快速上手)指南](https://www.easy-eai.com/document_details/18/505)

如果您具备丰富的嵌入式C语言开发经验，您可以：
[查阅EASYEAI开发实例](https://www.easy-eai.com/document_details/18/539)
[查阅API开发手册](https://www.easy-eai.com/document_details/18/625)


使用方法：
[1] - 克隆本GitHub仓库
[2] - 进入具体的(demo/solution)目录执行：./build.sh
[3] - 则会在具体的(demo/solution)目录的Release/下生成相关的可执行程序

目录说明：
---
* `easyeai-api` - 各组件 api 源码与预编译库，预编译库按板端系统版本分目录(ubuntu22.04 / ubuntu24.04 / ubuntu26.04 / debian13)
* `Demos` - 各 api 的单例程序，用于验证 api 功能
* `Solutions` - 完整方案，如 app-bsd、detect-Person、rtspIPCamera、rtspMulitPlayer、speechRecognitionLive、vcic_stream
* `UiSolutions` - Qt 方案，如 QDemo、QMultiDisplay

重要更新：
---
> 2026-11-10 :
> * 发布 EASY-EAI-Toolkit-3572-0.0.0
> * 新增 easyeai-api
>     * 算法组件api
>         * [新增] bsd
>         * [新增] car_detect
>         * [新增] face_detect
>         * [新增] face_landmark98
>         * [新增] face_recognition
>         * [新增] fire_detect
>         * [新增] geometry
>         * [新增] gestures
>         * [新增] helmet_detect
>         * [新增] lane_detect
>         * [新增] lpr
>         * [新增] ocr
>         * [新增] person_detect
>         * [新增] person_pose
>         * [新增] qrdecode
>         * [新增] speech_recognition
>     * 通用组件api
>         * [新增] base64
>         * [新增] data_queue
>         * [新增] font_engine
>         * [新增] ini_wrapper
>         * [新增] ipc
>         * [新增] json
>         * [新增] log_manager
>         * [新增] log_manager_pro
>         * [新增] qrcode
>         * [新增] system_opt
>     * 多媒体组件api
>         * [新增] camera
>         * [新增] display
>         * [新增] gpu
>         * [新增] gst_opt
>         * [新增] rga
>     * 网络协议组件api
>         * [新增] http
>         * [新增] modbus
>         * [新增] mqtt
>         * [新增] vcic
> * 新增【Api单例】
>     * [新增] algorithm-bsd
>     * [新增] algorithm-car
>     * [新增] algorithm-face_detect
>     * [新增] algorithm-face_landmark98
>     * [新增] algorithm-face_recognition
>     * [新增] algorithm-fire
>     * [新增] algorithm-gestures
>     * [新增] algorithm-helmet
>     * [新增] algorithm-lane_detect
>     * [新增] algorithm-lpr
>     * [新增] algorithm-ocr
>     * [新增] algorithm-person
>     * [新增] algorithm-person_pose
>     * [新增] algorithm-qrdecode
>     * [新增] algorithm-speech_recognition
>     * [新增] common-base64
>     * [新增] common-font_engine
>     * [新增] common-ini
>     * [新增] common-qrcode
>     * [新增] common-system_opt
>     * [新增] media-display
>     * [新增] media-rga
>     * [新增] netProtocol-http
>     * [新增] netProtocol-modbus
>     * [新增] netProtocol-mqtt
>     * [新增] netProtocol-vcic
> * 新增【开发实例】
>     * [新增] app-bsd
>     * [新增] detect-Person
>     * [新增] rtspIPCamera
>     * [新增] rtspMulitPlayer
>     * [新增] speechRecognitionLive
>     * [新增] vcic_stream
> * 新增【Ui开发实例】
>     * [新增] QDemo
>     * [新增] QMultiDisplay
>
> 2026-11-10 :
> * 创建本项目
