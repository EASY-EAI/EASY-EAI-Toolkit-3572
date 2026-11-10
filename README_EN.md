<br/>
<br/>


[中文](README.md)

<br />
<br />

If you first come into contact with this project. You can do:
[reference the Quick Start Guide](https://www.easy-eai.com/document_details/18/505)

If you have extensive experience in embedded C development. You can do:
[reference the EASYEAI development Example](https://www.easy-eai.com/document_details/18/539)
[reference the API development manual](https://www.easy-eai.com/document_details/18/625)


How to use：
[1] - Clone this Git Storage to Local
[2] - Enter the corresponding (Demo/Solution) directory of this repository: ./build.sh
[3] - The relevant (Demo/Solution) will be generated under Release/ in the specific directory

Directory description:
---
* `easyeai-api` - source codes and prebuilt libraries of each component api, prebuilt libraries are grouped by board system version (ubuntu22.04 / ubuntu24.04 / ubuntu26.04 / debian13)
* `Demos` - api demo programs, used to verify api functions
* `Solutions` - complete solutions, such as app-bsd, detect-Person, rtspIPCamera, rtspMulitPlayer, speechRecognitionLive, vcic_stream
* `UiSolutions` - Qt solutions, such as QDemo, QMultiDisplay

important update log:
---
> 2026-11-10 :
> * Release EASY-EAI-Toolkit-3572-0.0.0
> * add easyeai-api
>     * algorithm_api
>         * [new] bsd
>         * [new] car_detect
>         * [new] face_detect
>         * [new] face_landmark98
>         * [new] face_recognition
>         * [new] fire_detect
>         * [new] geometry
>         * [new] gestures
>         * [new] helmet_detect
>         * [new] lane_detect
>         * [new] lpr
>         * [new] ocr
>         * [new] person_detect
>         * [new] person_pose
>         * [new] qrdecode
>         * [new] speech_recognition
>     * common_api
>         * [new] base64
>         * [new] data_queue
>         * [new] font_engine
>         * [new] ini_wrapper
>         * [new] ipc
>         * [new] json
>         * [new] log_manager
>         * [new] log_manager_pro
>         * [new] qrcode
>         * [new] system_opt
>     * media_api
>         * [new] camera
>         * [new] display
>         * [new] gpu
>         * [new] gst_opt
>         * [new] rga
>     * netProtocol_api
>         * [new] http
>         * [new] modbus
>         * [new] mqtt
>         * [new] vcic
> * add Demo
>     * [new] algorithm-bsd
>     * [new] algorithm-car
>     * [new] algorithm-face_detect
>     * [new] algorithm-face_landmark98
>     * [new] algorithm-face_recognition
>     * [new] algorithm-fire
>     * [new] algorithm-gestures
>     * [new] algorithm-helmet
>     * [new] algorithm-lane_detect
>     * [new] algorithm-lpr
>     * [new] algorithm-ocr
>     * [new] algorithm-person
>     * [new] algorithm-person_pose
>     * [new] algorithm-qrdecode
>     * [new] algorithm-speech_recognition
>     * [new] common-base64
>     * [new] common-font_engine
>     * [new] common-ini
>     * [new] common-qrcode
>     * [new] common-system_opt
>     * [new] media-display
>     * [new] media-rga
>     * [new] netProtocol-http
>     * [new] netProtocol-modbus
>     * [new] netProtocol-mqtt
>     * [new] netProtocol-vcic
> * add Solution
>     * [new] app-bsd
>     * [new] detect-Person
>     * [new] rtspIPCamera
>     * [new] rtspMulitPlayer
>     * [new] speechRecognitionLive
>     * [new] vcic_stream
> * add UiSolution
>     * [new] QDemo
>     * [new] QMultiDisplay
>
> 2026-11-10 :
> * create this project
