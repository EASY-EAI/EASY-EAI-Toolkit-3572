/**
 *
 * Copyright 2021 by Guangzhou Easy EAI Technologny Co.,Ltd.
 * website: www.easy-eai.com
 *
 * Author: CHM <chenhaiman@easy-eai.com>
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * License file for more details.
 * 
 */

#ifndef CAMERA_H
#define CAMERA_H

#ifdef __cplusplus
extern "C" {
#endif
 
#include <linux/videodev2.h>
#include <rga/RgaApi.h>

#define	USB_OTG		1
#define	USB2_0		3
#define	USB_DIRECT	0

#ifndef MIPI_SRCFRAME_MAX_PLANES
#define MIPI_SRCFRAME_MAX_PLANES 1
#endif

/* usb camera */
int usbcamera_init(int vIndex, int width, int height, int rot);
void usbcamera_exit(int vIndex);
int usbcamera_getframe(int vIndex, char *pbuf);
void usbcamera_preset_fps(int fps);
void usbcamera_set_format(int vIndex, int format);

/* support format:
RK_FORMAT_RGBA_8888
RK_FORMAT_RGBX_8888
RK_FORMAT_RGB_888
RK_FORMAT_BGRA_8888
RK_FORMAT_RGB_565
RK_FORMAT_RGBA_5551
RK_FORMAT_RGBA_4444
RK_FORMAT_BGR_888

RK_FORMAT_YCbCr_422_SP
RK_FORMAT_YCbCr_422_P
RK_FORMAT_YCbCr_420_SP
RK_FORMAT_YCbCr_420_P

RK_FORMAT_YCrCb_422_SP
RK_FORMAT_YCrCb_422_P
RK_FORMAT_YCrCb_420_SP
RK_FORMAT_YCrCb_420_P

RK_FORMAT_BPP1
RK_FORMAT_BPP2
RK_FORMAT_BPP4
RK_FORMAT_BPP8

RK_FORMAT_Y4
RK_FORMAT_YCbCr_400

RK_FORMAT_BGRX_8888

RK_FORMAT_YVYU_422
RK_FORMAT_YVYU_420
RK_FORMAT_VYUY_422
RK_FORMAT_VYUY_420
RK_FORMAT_YUYV_422
RK_FORMAT_YUYV_420
RK_FORMAT_UYVY_422
RK_FORMAT_UYVY_420

RK_FORMAT_YCbCr_420_SP_10B
RK_FORMAT_YCrCb_420_SP_10B
RK_FORMAT_YCbCr_422_10b_SP
RK_FORMAT_YCrCb_422_10b_SP
*/

/* mipi camera */
int mipicamera_init(int camIndex, int width, int height, int rot);
void mipicamera_exit(int camIndex);
int mipicamera_getframe(int camIndex, char *pbuf);
void mipicamera_set_format(int camIndex, int format);


typedef struct {
    uint8_t *pMapBuff;
    int width;
    int height;
    int horStride;
    int verStride;
    int dataSize;
    int format;
    struct v4l2_plane planes[MIPI_SRCFRAME_MAX_PLANES];
    struct v4l2_buffer readbuffer;
}SrcFrame_t;
int mipicamera_getSrcframe(int camIndex, SrcFrame_t *pFrame);
int mipicamera_putSrcframe(int camIndex, SrcFrame_t *pFrame);

#ifdef __cplusplus
}
#endif
#endif
