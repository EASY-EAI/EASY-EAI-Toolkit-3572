#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include "rga_wrapper.h"

typedef unsigned long long __u64;
typedef unsigned int __u32;

struct dma_heap_allocation_data {
	__u64 len;
	__u32 fd;
	__u32 fd_flags;
	__u64 heap_flags;
};

#define DMA_HEAP_IOC_MAGIC	'H'
#define DMA_HEAP_IOCTL_ALLOC	_IOWR(DMA_HEAP_IOC_MAGIC, 0x0, struct dma_heap_allocation_data)

#define DMA_BUF_SYNC_READ      (1 << 0)
#define DMA_BUF_SYNC_WRITE     (2 << 0)
#define DMA_BUF_SYNC_RW        (DMA_BUF_SYNC_READ | DMA_BUF_SYNC_WRITE)
#define DMA_BUF_SYNC_START     (0 << 2)
#define DMA_BUF_SYNC_END       (1 << 2)

struct dma_buf_sync {
	__u64 flags;
};

#define DMA_BUF_BASE		'b'
#define DMA_BUF_IOCTL_SYNC	_IOW(DMA_BUF_BASE, 0, struct dma_buf_sync)

static int dma_buf_alloc_from_heap(const char *path, size_t size, int *fd_out, void **va_out)
{
	int ret;
	int dma_heap_fd;
	struct dma_heap_allocation_data buf_data;
	void *mmap_va;
	int prot;

	dma_heap_fd = open(path, O_RDWR);
	if (dma_heap_fd < 0)
		return -1;

	memset(&buf_data, 0, sizeof(buf_data));
	buf_data.len = size;
	buf_data.fd_flags = O_CLOEXEC | O_RDWR;
	ret = ioctl(dma_heap_fd, DMA_HEAP_IOCTL_ALLOC, &buf_data);
	close(dma_heap_fd);
	if (ret < 0)
		return -1;

	if (fcntl(buf_data.fd, F_GETFL) & O_RDWR)
		prot = PROT_READ | PROT_WRITE;
	else
		prot = PROT_READ;

	mmap_va = mmap(NULL, size, prot, MAP_SHARED, buf_data.fd, 0);
	if (mmap_va == MAP_FAILED) {
		close(buf_data.fd);
		return -1;
	}

	*fd_out = buf_data.fd;
	*va_out = mmap_va;
	return 0;
}
int alloc_dmabuf(size_t dma_size, int *dma_fd, void **pBuf_Map)
{
	static const char *const paths[] = {
		"/dev/dma_heap/system-uncached",
		"/dev/dma_heap/cma-uncached",
		"/dev/dma_heap/system",
		"/dev/rk_dma_heap/rk-dma-heap-cma",
	};

	for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
		if (dma_buf_alloc_from_heap(paths[i], dma_size, dma_fd, pBuf_Map) == 0) {
			printf("dma_buf_alloc_from [%s] success\n", paths[i]);
			return 0;
		}
	}
	return -1;
}

// 缓存同步，将CPU缓存中的数据同步到设备，每通过mmap修改后，都需要这样同步进去
int dma_sync_cpu_to_device(int fd)
{
	struct dma_buf_sync sync;
	memset(&sync, 0, sizeof(sync));
	sync.flags = DMA_BUF_SYNC_END | DMA_BUF_SYNC_RW;
	return ioctl(fd, DMA_BUF_IOCTL_SYNC, &sync);
}

RgaSURF_FORMAT rgaFmt(char *strFmt)
{
    if(0 == strcmp(strFmt, "NV12")){
        return RK_FORMAT_YCbCr_420_SP;
    }else if(0 == strcmp(strFmt, "NV21")){
        return RK_FORMAT_YCrCb_420_SP;
    }else if(0 == strcmp(strFmt, "BGR")){
        return RK_FORMAT_BGR_888;
    }else if(0 == strcmp(strFmt, "RGB")){
        return RK_FORMAT_RGB_888;
    }else{
        return RK_FORMAT_UNKNOWN;
    }
}

int srcImg_ConvertTo_dstImg(Image *pDst, Image *pSrc)
{
	rga_info_t src, dst;
	int ret = -1;

	if (!pSrc || !pDst) {
		printf("%s: NULL PTR!\n", __func__);
		return -1;
	}

	//图像参数转换
	memset(&src, 0, sizeof(rga_info_t));
	src.fd = pSrc->fd;
	src.virAddr = pSrc->pBuf;
	src.mmuFlag = 1;
	src.rotation =  pSrc->rotation;
	rga_set_rect(&src.rect, 0, 0, pSrc->width, pSrc->height, pSrc->hor_stride, pSrc->ver_stride, pSrc->fmt);

	memset(&dst, 0, sizeof(rga_info_t));
	dst.fd = pDst->fd;
	dst.virAddr = pDst->pBuf;
	dst.mmuFlag = 1;
	dst.rotation =  pDst->rotation;
	rga_set_rect(&dst.rect, 0, 0, pDst->width, pDst->height, pDst->hor_stride, pDst->ver_stride, pDst->fmt);
	if (c_RkRgaBlit(&src, &dst, NULL)) {
		printf("%s: rga fail\n", __func__);
		ret = -1;
	} else {
		ret = 0;
	}

	return ret;
}

