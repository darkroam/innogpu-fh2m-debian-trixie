/*************************************************************************/ /*!
@File			innodpu_drm_gem.h
@Title
@Copyright		Copyright (c) Innosilicon Technology Ltd. All Rights Reserved
@Description
@License		Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/
#ifndef __INNODPU_DRM_GEM_H
#define __INNODPU_DRM_GEM_H

#include <linux/dma-buf.h>
#include "inno_drm_version.h"

#if (DRM_VERSION >= KERNEL_VERSION(5, 5, 0))
#include <drm/drm_device.h>
#include <drm/drm_file.h>
#endif
#if (DRM_VERSION >= KERNEL_VERSION(5, 5, 0))
#include <drm/drm_device.h>
#include <drm/drm_file.h>
#else
#include <drm/drmP.h>
#endif

#if (DRM_VERSION >= KERNEL_VERSION(3, 18, 0))
#include <drm/drm_gem.h>
#endif

#include "innodpu_common.h"

#include "pvr_dma_resv.h"
#include "hal_interface.h"
#include "kernel_compatibility.h"

#define VGA_MAX_WIDTH 1920
#define VGA_MAX_HEIGHT 1200

#define DBM_GEM_NEED_CONTINUOUS INNO_BIT(26)
#define DBM_GEM_GTT             INNO_BIT(27)
#define DBM_GEM_INVISIBLE       INNO_BIT(28)
#define DBM_GEM_NO_CLEAR        INNO_BIT(29)

#define GEM_USED_VMALLOC
#define GEM_ZERO_SIZE			(0x100000)
#define DRM_VRAM_FMT "%pK:visiable-%d dev_paddr %#llx"
#define DRM_VRAM_ARG(m) \
	(m), (m)->mem_manager->visible, &((m)->dev_paddr)

extern struct vm_operations_struct inno_gem_vm_ops;

struct gem_vm_list {
	struct list_head list;
	unsigned long vm_vaddr;
	unsigned long offset;
	unsigned long dev_paddr;
	void * vaddr;
	int size;
	bool cpu_write;
};

typedef struct innodpu_gem_object_t {
	char *name;

	/* these file use for ditinguish gem type */
	uint32_t flags;
	innodpu_mem_class class;

	struct drm_mm_node mm_node;
	struct list_head mem_node;
	struct list_head free_node;

	/* these field use for s3/s4  */
	void *suspend_data;
	struct list_head pm_node;

	/* these filed used for dmabuf export*/
	struct mutex vm_lock;
	struct sg_table *sgt;
	struct dma_buf *tmp_dbuf;
	struct list_head vm_head;
	bool dma_map_export_host_addr;

	/* master manager */
	innodpu_mem_manager *mem_manager;

	/* vram addr info */
	struct phys_mem_resource *pmr;
	struct gtt_item *gtt;
	phys_addr_t cpu_paddr;
	dma_addr_t dev_paddr;

	unsigned long size_origin;
	unsigned long size_align;

	bool is_fbdev_obj;
	bool cpu_prep;
	bool cpu_prep_write;
	bool is_duplicate;
	int dfd;
	spinlock_t lock;

#if (DRM_VERSION < KERNEL_VERSION(5, 2, 0))
	struct dma_resv _resv;
#endif
	struct dma_resv *resv;

	struct drm_gem_object base;
} innodpu_gem_object;

#define to_innodpu_obj(obj) container_of(obj, innodpu_gem_object, base)

void innodpu_drm_fix_vma_flags(struct vm_area_struct *vma,
	pgprot_t vm_page_prot, unsigned long vm_flags);

bool innodpu_gem_check_memory_shared(struct drm_device *drm_dev,
	struct innodpu_drm_private *dpu_priv, uint32_t flag);
int innodpu_multiuser_add(struct drm_device *drm_dev,
	struct innodpu_drm_private *dev_priv, void *data, struct drm_file *drm_file);
int innodpu_multiuser_set(struct drm_device *drm_dev,
	struct innodpu_drm_private *dev_priv, void *data, struct drm_file *drm_file);
int innodpu_multiuser_del(struct drm_device *drm_dev,
	struct innodpu_drm_private *dev_priv, void *data, struct drm_file *drm_file);
void innodpu_xorg_monitor_switch_user(struct drm_device *drm_dev,
	innodpu_shared_mem *pshared_mem);

int innodpu_gem_dbmflags_to_position(struct drm_device *drm_dev,
	struct innodpu_drm_private *dpu_priv, uint32_t flags);
int innodpu_get_dmbflags_to_class(uint32_t flags, innodpu_mem_positon pos);
struct drm_gem_object *innodpu_gem_object_create_priv(struct drm_device *drm_dev,
	innodpu_mem_manager *mem_manager, size_t size, u32 flags);
int innodpu_gem_object_create(struct drm_file *drm_file, struct drm_device *drm_dev,
	innodpu_mem_manager *mem_manager, void *data, bool is_priv);
void innodpu_gem_object_free(struct drm_gem_object *gem_obj);

innodpu_mem_manager *innodpu_mem_manager_init(struct drm_device *drm_dev,
	bool visible, innodpu_mem_positon pos, innodpu_shared_mem *shared_mem_info);
void innodpu_mem_manager_fini(struct drm_device *drm_dev, innodpu_mem_manager *mem_manager);

int innodpu_gem_share_pre_alloc(struct drm_device *drm_dev, innodpu_shared_mem *pshared_mem,
	unsigned long mem_size);
void innodpu_gem_share_pre_free(struct drm_device *drm_dev, innodpu_shared_mem *pshared_mem);

innodpu_pdp_vga_gem *innodpu_pdp_vga_mem_init(struct drm_device *drm_dev);
void innodpu_pdp_vga_mem_fini(struct drm_device *drm_dev, innodpu_pdp_vga_gem *pdp_vga_gem);
bool innodpu_pdp_vga_buffer_set(innodpu_pdp_vga_gem *pdp_vga_gem,
	inno_dev* dev, unsigned int target_width, unsigned int target_height);

innodpu_zero_gem *innodpu_zero_mem_init(struct drm_device *drm_dev, uint64_t size);
void innodpu_zero_mem_fini(struct drm_device *drm_dev, innodpu_zero_gem *zero_mem);

int innodpu_gem_vram_count(int id, void *ptr, void *data);
void innodpu_sysmem_thaw_free(innodpu_mem_manager *mem_manager);
void innodpu_gem_suspend(innodpu_mem_manager *mem_manager);
void innodpu_gem_backup(innodpu_mem_manager *mem_manager);
void innodpu_gem_recover(innodpu_mem_manager *mem_manager);
void innodpu_gem_zero_vram_recover(struct drm_device *drm_dev, innodpu_zero_gem *zero_gem);

int inno_gem_object_cpu_prep_ioctl(struct drm_device *dev, void *data, struct drm_file *file);
int inno_gem_object_cpu_fini_ioctl(struct drm_device *dev, void *data, struct drm_file *file);
int inno_gem_object_dump_vram_ioctl(struct drm_device *dev, void *data, struct drm_file *file);
int inno_gem_object_inv_get_ioctl(struct drm_device *dev, void *data, struct drm_file *file);

struct dma_resv *innodpu_gem_get_resv(struct drm_gem_object *obj);
unsigned long fh2m_innodpu_gem_get_dev_paddr(struct drm_gem_object * obj);
unsigned long innodpu_gem_get_dev_vaddr(struct drm_gem_object *obj);
unsigned long fh2m_innodpu_gem_get_cpu_paddr(struct drm_gem_object *obj);
unsigned long fh2m_innodpu_gem_get_size_origin(struct drm_gem_object *obj);
const struct dma_buf_ops* fh2m_innodpu_gem_get_dma_buf_ops(void);
#endif //__INNODPU_DRM_GEM_H
