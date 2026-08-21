/*
 * @File
 * @Title       PDP DRM definitions shared between kernel and user space.
 * @Codingstyle LinuxKernel
 * @Copyright   Copyright (c) Imagination Technologies Ltd. All Rights Reserved
 * @License     Strictly Confidential.
 */

#if !defined(__PDP_DRM_H__)
#define __PDP_DRM_H__

#if defined(__KERNEL__)
#include <drm/drm.h>
#else
#include <drm.h>
#endif

struct drm_pdp_gem_create {
	__u64 size;	/* in */
	__u32 flags;	/* in */
	__u32 handle;	/* out */
};

struct drm_pdp_gem_mmap {
	__u32 handle;	/* in */
	__u32 pad;
	__u64 offset;	/* out */
};

struct drm_dpu_vram_count {
	__u64 visiable_vram_size;
	__u64 visiable_vram_usage;
	__u64 invisiable_vram_size;
	__u64 invisiable_vram_usage;
	__u64 flags;
};

struct drm_dpu_gem_inv_get {
	__u32 handle;	/* in */
	__u32 isInv;	/* out */
};

#define PDP_GEM_CPU_PREP_READ	(1 << 0)
#define PDP_GEM_CPU_PREP_WRITE	(1 << 1)
#define PDP_GEM_CPU_PREP_NOWAIT	(1 << 2)

struct drm_pdp_gem_cpu_prep {
	__u32 handle;	/* in */
	__u32 flags;	/* in */
};

struct drm_pdp_gem_cpu_fini {
	__u32 handle;	/* in */
	__u32 pad;
};

struct drm_pdp_gem_obj_fd {
	__u64 obj;	/* in */
	__u32 dfd;	/* out */
};

struct drm_pdp_base_fd {
	unsigned int plane_id; // which display, input param
	unsigned long frame_id;

	unsigned int width;
	unsigned int height;
	size_t frame_size;
	unsigned int format;

	int obj_idr; // used for DRM_IOCTL_GEM_OPEN
	unsigned int fd; // dma_buf fd; output param

	int cursor_obj_idr;
	unsigned int cursor_x;
	unsigned int cursor_y;
	unsigned int cursor_width;
	unsigned int cursor_height;
	unsigned int cursor_format;// only support ARGB
};

struct drm_pdp_chip_info {
	__u64 chip_type;
	bool support_inv;
	bool support_shmem;
};


typedef enum DRM_ADDR_TYPE_E {
    DRM_DEV_PADDR = 0,
    DRM_DEV_VADDR,
} DRM_ADDR_TYPE;

struct drm_pdp_gem_addr
{
    uint32_t handle;
    DRM_ADDR_TYPE addr_type;
    uint64_t addr;
};

/*
 * filed gtt_support and smmu_support:
 * every bit mean module support specific feat
 * 	BIT0 PCIE
 *  BIT1 AXI
 *  BIT2 GPU
 *  BIT3 DPU
 *  BIT4 VPU
 *  BIT5 APU
*/
struct drm_hw_info {
	uint64_t chip_type;
	uint32_t gtt_support;
	uint32_t smmu_support;
	uint32_t dpu_bus_align;
	uint64_t reserved[32];
};

struct drm_sw_info {
	uint32_t version;

	uint64_t gem_flags;
	uint64_t reserved[32];
};

struct drm_common_info {
	struct drm_hw_info hwinfo;
	struct drm_sw_info swinfo;
};

/*
 * DRM command numbers, relative to DRM_COMMAND_BASE.
 * These defines must be prefixed with "DRM_".
 */
#define DRM_PDP_GEM_CREATE		0x20
#define DRM_PDP_GEM_MMAP		0x21
#define DRM_PDP_GEM_CPU_PREP	0x22
#define DRM_PDP_GEM_CPU_FINI	0x23
#define DRM_PDP_GEM_OBJ_FD		0x24
#define DRM_PDP_GEM_FREE_FD		0x25
#define DRM_PDP_GEM_ADDR 		0x26
#define DRM_PDP_GEM_BASE_FD		0X27
#define DRM_PDP_GEM_GET			0x28
#define DRM_PDP_GEM_INV_GET		0x29
#define DRM_PDP_CHIP_INFO		0x30
#define DRM_INNO_COMMON_INFO 	0x2f


/* These defines must be prefixed with "DRM_IOCTL_". */
#define DRM_IOCTL_PDP_GEM_CREATE \
	DRM_IOWR(DRM_COMMAND_BASE + DRM_PDP_GEM_CREATE, \
		 struct drm_pdp_gem_create)

#define DRM_IOCTL_PDP_GEM_MMAP\
	DRM_IOWR(DRM_COMMAND_BASE + DRM_PDP_GEM_MMAP, \
		 struct drm_pdp_gem_mmap)

#define DRM_IOCTL_PDP_GEM_CPU_PREP \
	DRM_IOW(DRM_COMMAND_BASE + DRM_PDP_GEM_CPU_PREP, \
		struct drm_pdp_gem_cpu_prep)

#define DRM_IOCTL_PDP_GEM_CPU_FINI \
	DRM_IOW(DRM_COMMAND_BASE + DRM_PDP_GEM_CPU_FINI, \
		struct drm_pdp_gem_cpu_fini)

#define DRM_IOCTL_PDP_GEM_OBJ_FD \
	DRM_IOWR(DRM_COMMAND_BASE + DRM_PDP_GEM_OBJ_FD, \
		struct drm_pdp_gem_obj_fd)

#define DRM_IOCTL_PDP_GEM_FREE_FD \
	DRM_IOW(DRM_COMMAND_BASE + DRM_PDP_GEM_FREE_FD, \
		struct drm_pdp_gem_obj_fd)

#define DRM_IOCTL_PDP_GEM_BASE_FD	 \
	DRM_IOWR(DRM_COMMAND_BASE + DRM_PDP_GEM_BASE_FD, \
		struct drm_pdp_base_fd)

#define DRM_IOCTL_PDP_GEM_GET	 \
	DRM_IOWR(DRM_COMMAND_BASE + DRM_PDP_GEM_GET, \
		struct drm_dpu_vram_count)

#define DRM_IOCTL_PDP_GEM_INV_GET	 \
	DRM_IOWR(DRM_COMMAND_BASE + DRM_PDP_GEM_INV_GET, \
		struct drm_dpu_gem_inv_get)

#define DRM_IOCTL_PDP_CHIP_INFO	 \
	DRM_IOWR(DRM_COMMAND_BASE + DRM_PDP_CHIP_INFO, \
		struct drm_pdp_chip_info)

#define DRM_IOCTL_PDP_GEM_ADDR \
	DRM_IOWR(DRM_COMMAND_BASE + DRM_PDP_GEM_ADDR, \
		struct drm_pdp_gem_addr)

#define DRM_IOCTL_INNO_COMMON_INFO \
	DRM_IOWR(DRM_COMMAND_BASE + DRM_INNO_COMMON_INFO, \
		struct drm_common_info)


#endif /* defined(__PDP_DRM_H__) */
