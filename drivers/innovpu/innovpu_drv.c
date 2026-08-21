/*************************************************************************/ /*!
@File			innovpu_drv.c
@Title			innovpu driver
@Copyright		Copyright (c) Innosilicon Technology Ltd. All Rights Reserved
@Description	innovpu driver
@License        Dual MIT/GPLv2

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

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/wait.h>
#include <linux/list.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/version.h>
#include <linux/kthread.h>
#include <linux/pci.h>
#include <linux/kobject.h>
#include <linux/debugfs.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <linux/version.h>
#include <linux/sched.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/sched/signal.h>
#endif
#include <linux/firmware.h>

#include "kernel_compatibility.h"
#include "syscommon.h"
#include "osfunc_common.h"
#include "innogpu.h"
#include "inno_misc.h"
#include "innovpu.h"
#include "innovpu_drv.h"
#include "innovpu_dmabuf.h"
#include "innovpu_common.h"
#include "inno_timer.h"
#include "osfunc.h"

#define MAX_INTERRUPT_QUEUE (16 * 32)
static atomic64_t g_vpu_node_num;
static vpu_drv_info * g_vpu_drv_info;
uint64_t g_vpu_debug_level = 0; // set debug level

#ifdef USE_REFACTOR_LOGIC
	const char *g_vpu_version = "2.0.0";
#else
	const char *g_vpu_version = "1.0.0";
#endif

static u32 vpu_free_buffers(vpu_drv_ctxs *vpu_drv_ctx, void *filp)
{
	vpudrv_buffer_pool_t *pool, *n = NULL;
	vpudrv_buffer_t vb;
	vpu_buf_obj_t obj;
	int found = 0;

	vpu_dbg(vpu_drv_ctx->drv_context.vpudev, " vpu_free_buffers, filp:%p\n", filp);
	while (1) {
		memset(&vb, 0, sizeof(vpudrv_buffer_t));
		memset(&obj, 0, sizeof(vpudrv_buffer_t));
		fh2m_inno_spin_lock(vpu_drv_ctx->drv_context.vpu_lock);
		list_for_each_entry_safe(pool, n, &vpu_drv_ctx->drv_context.vbp_head, list) {
			if (pool->filp == filp) {
				vb = pool->buf.vb;
				obj = pool->buf.obj;
				found = 1;
				list_del(&pool->list);
				kfree(pool);
				break;
			}
		}
		fh2m_inno_spin_unlock(vpu_drv_ctx->drv_context.vpu_lock);

		if (found == 1) {
			found = 0;
			if (obj.buf_type == BUF_TYPE_VPU_IMPORT ||
				obj.buf_type == BUF_TYPE_VPU_EXPORT) {
				vpu_destroy_dmabuf(&(vpu_drv_ctx->drv_context), &obj);
			} else if (vb.base) {
				vpu_free_dma_buffer(&vb, &(vpu_drv_ctx->drv_context));
			}
		} else {
			break;
		}
	}
	return 0;
}

vpu_drv_ctxs * vpu_get_drv_ctx(struct file *filp)
{
	return (vpu_drv_ctxs *)filp->private_data;
}

#ifdef USE_REFACTOR_LOGIC
static vpu_buf_obj_t* vpu_get_vb_from_fd(struct file *filp, int fd) {
	vpu_drv_ctxs *vpu_drv_ctx = vpu_get_drv_ctx(filp);
	vpudrv_buffer_pool_t *vbp, *n;
	vpu_buf_obj_t *vb = NULL;

	fh2m_inno_spin_lock(vpu_drv_ctx->drv_context.vpu_lock);
	list_for_each_entry_safe(vbp, n, &vpu_drv_ctx->drv_context.vbp_head, list) {
		if ((vbp->buf.vb.fd == fd) && (vbp->filp == filp)) {
			vb = &vbp->buf.obj;
			break;
		}
	}
	fh2m_inno_spin_unlock(vpu_drv_ctx->drv_context.vpu_lock);

	return vb;
}
#endif

static int vpu_get_instance_id(struct file *filp) //, vpu_drv_ctxs tmp
{
	/* TODO: instance id <-> filp map */
	vpu_drv_ctxs *vpu_drv_ctx = vpu_get_drv_ctx(filp);
	vpudrv_instanace_list_t *vil, *n;
	int inst_idx = -1;

	//Step1. Search instance id whether in list
	fh2m_inno_spin_lock(vpu_drv_ctx->drv_context.vpu_lock);
	list_for_each_entry_safe(vil, n, &vpu_drv_ctx->drv_context.inst_list_head, list)
	{
		if (vil->filp == filp) {
			inst_idx = vil->inst_idx;
			break;
		}
	}
	fh2m_inno_spin_unlock(vpu_drv_ctx->drv_context.vpu_lock);

	return inst_idx;
}

static int vpu_open(struct inode *inode, struct file *filp)
{
	vpu_drv_ctxs *vpu_drv_ctx;

	vpu_drv_ctx = container_of((struct cdev *)inode->i_cdev, vpu_drv_ctxs, vpucdev);

	fh2m_inno_spin_lock(vpu_drv_ctx->drv_context.vpu_lock);
	vpu_drv_ctx->drv_context.vpu_context.open_count++;
	fh2m_inno_spin_unlock(vpu_drv_ctx->drv_context.vpu_lock);

	filp->private_data = vpu_drv_ctx;

	return 0;
}

/*static u32 vpu_ioctl(struct inode *inode, struct file *filp, u_int cmd, u_long arg) // for kernel 2.6.9*/
static long vpu_ioctl(struct file *filp, u_int cmd, u_long arg)
{
	int ret = 0;

	vpu_drv_ctxs *vpu_drv_ctx = vpu_get_drv_ctx(filp);
	if (!vpu_drv_ctx) {
		vpu_prerr("%s vpu_drv_ctx is NULL\n", __func__);
		return -EINVAL;
	}

	switch (cmd) {
		case VDI_IOCTL_DMA_MAP_HOSTMEM:
			{
				vpudrv_buffer_t vb;
				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "[+]VDI_IOCTL_DMA_MAP_HOSTMEM\n");
				ret = fh2m_inno_copy_from_user(&vb, (const vpudrv_buffer_t* __user)arg, sizeof(vpudrv_buffer_t));
				if (ret) {
					return -EFAULT;
				}
				if ((ret = down_interruptible(vpu_drv_ctx->drv_context.vpu_sem)) == 0) {
					ret = vpu_dma_map_hostmem(&(vpu_drv_ctx->drv_context), &vb);
					up(vpu_drv_ctx->drv_context.vpu_sem);
					if (ret) {
						break;
					}
				}
				ret = fh2m_inno_copy_to_user((void* __user)arg, &vb,sizeof(vpudrv_buffer_t));
				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "[-]VDI_IOCTL_DMA_MAP_HOSTMEM\n");
			}
			break;
		case VDI_IOCTL_DMA_UNMAP_HOSTMEM:
			{
				vpudrv_buffer_t vb;
				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "[+]VDI_IOCTL_DMA_UNMAP_HOSTMEM\n");
				ret = fh2m_inno_copy_from_user(&vb, (const vpudrv_buffer_t* __user)arg, sizeof(vpudrv_buffer_t));
				if (ret) {
					return -EFAULT;
				}
				if ((ret = down_interruptible(vpu_drv_ctx->drv_context.vpu_sem)) == 0) {
					vpu_dma_unmap_hostmem(&(vpu_drv_ctx->drv_context), &vb);
					up(vpu_drv_ctx->drv_context.vpu_sem);
				}
				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "[-]VDI_IOCTL_DMA_UNMAP_HOSTMEM\n");
			}
			break;
		case VDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY:
			{
				vpudrv_buffer_pool_t *vbp;

				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "[+]VDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY\n");

				if ((ret = down_interruptible(vpu_drv_ctx->drv_context.vpu_sem)) == 0) {
					vbp = kzalloc(sizeof(*vbp), GFP_KERNEL);
					if (!vbp) {
						up(vpu_drv_ctx->drv_context.vpu_sem);
						return -ENOMEM;
					}

					ret = fh2m_inno_copy_from_user(&(vbp->buf.vb), (vpudrv_buffer_t *)arg, sizeof(vpudrv_buffer_t));
					if (ret) {
						kfree(vbp);
						up(vpu_drv_ctx->drv_context.vpu_sem);
						return -EFAULT;
					}

					ret = vpu_alloc_dma_buffer(&(vbp->buf.vb), &(vpu_drv_ctx->drv_context));
					if (ret != 0) {
						ret = -ENOMEM;
						kfree(vbp);
						up(vpu_drv_ctx->drv_context.vpu_sem);
						break;
					}

					ret = copy_to_user((void __user *)arg, &(vbp->buf.vb), sizeof(vpudrv_buffer_t));
					if (ret) {
						vpu_free_dma_buffer(&(vbp->buf.vb), &(vpu_drv_ctx->drv_context));
						kfree(vbp);
						ret = -EFAULT;
						up(vpu_drv_ctx->drv_context.vpu_sem);
						break;
					}

					vbp->filp = filp;
					fh2m_inno_spin_lock(vpu_drv_ctx->drv_context.vpu_lock);
					vbp->buf.obj.buf_type = BUF_TYPE_VPU_MALLOC;
					list_add(&vbp->list, &vpu_drv_ctx->drv_context.vbp_head);
					fh2m_inno_spin_unlock(vpu_drv_ctx->drv_context.vpu_lock);

					up(vpu_drv_ctx->drv_context.vpu_sem);
				}
				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "[-]VDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY\n");
			}
			break;
		case VDI_IOCTL_FREE_PHYSICALMEMORY:
			{
				vpudrv_buffer_pool_t *vbp = NULL, *n = NULL;
				vpudrv_buffer_t vb;
				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "[+]VDI_IOCTL_FREE_PHYSICALMEMORY\n");

				if ((ret = down_interruptible(vpu_drv_ctx->drv_context.vpu_sem)) == 0) {
					ret = fh2m_inno_copy_from_user(&vb, (vpudrv_buffer_t *)arg, sizeof(vpudrv_buffer_t));
					if (ret) {
						up(vpu_drv_ctx->drv_context.vpu_sem);
						return -EACCES;
					}

					if (vb.base) {
						vpu_free_dma_buffer(&vb, &(vpu_drv_ctx->drv_context));
					}

					fh2m_inno_spin_lock(vpu_drv_ctx->drv_context.vpu_lock);
					list_for_each_entry_safe(vbp, n, &vpu_drv_ctx->drv_context.vbp_head, list)
					{
						if (vbp->buf.vb.base == vb.base) {
							list_del(&vbp->list);
							kfree(vbp);
							break;
						}
					}
					fh2m_inno_spin_unlock(vpu_drv_ctx->drv_context.vpu_lock);

					up(vpu_drv_ctx->drv_context.vpu_sem);
				}
				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "[-]VDI_IOCTL_FREE_PHYSICALMEMORY\n");

			}
			break;
		case VDI_IOCTL_GET_RESERVED_VIDEO_MEMORY_INFO:
			break;
		case VDI_IOCTL_DMABUF_EXPORT:
			{
				vpudrv_buffer_pool_t *vbp;
				int fd;

				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "[+]VDI_IOCTL_DMABUF_EXPORT\n");
				vbp = kzalloc(sizeof(*vbp), GFP_KERNEL);
				if (!vbp) {
					return -ENOMEM;
				}

				ret = fh2m_inno_copy_from_user(&(vbp->buf.vb), (vpudrv_buffer_t *)arg, sizeof(vpudrv_buffer_t));
				if (ret) {
					kfree(vbp);
					return -EFAULT;
				}

				if ((ret = down_interruptible(vpu_drv_ctx->drv_context.vpu_sem)) == 0) {
					fd = vpu_export_dmabuf(&(vpu_drv_ctx->drv_context), &(vbp->buf.obj));
					if (fd < 0) {
						ret = -ENOMEM;
						kfree(vbp);
						up(vpu_drv_ctx->drv_context.vpu_sem);
						break;
					}
					up(vpu_drv_ctx->drv_context.vpu_sem);
				} else {
					kfree(vbp);
					return -EFAULT;
				}

				ret = copy_to_user((void __user *)arg, &(vbp->buf.vb), sizeof(vpudrv_buffer_t));
				if (ret) {
					ret = -EFAULT;
				}

				vbp->filp = filp;
				fh2m_inno_spin_lock(vpu_drv_ctx->drv_context.vpu_lock);
				vbp->buf.obj.buf_type = BUF_TYPE_VPU_EXPORT;
				list_add(&vbp->list, &vpu_drv_ctx->drv_context.vbp_head);
				fh2m_inno_spin_unlock(vpu_drv_ctx->drv_context.vpu_lock);

				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "[-]VDI_IOCTL_DMABUF_EXPORT\n");
			}
			break;
		case VDI_IOCTL_DMABUF_IMPORT:
			{
				vpudrv_buffer_pool_t *vbp;
				vpudrv_buffer_t *vb;

				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "[+]VDI_IOCTL_DMABUF_IMPORT\n");
				vbp = kzalloc(sizeof(*vbp), GFP_KERNEL);
				if (!vbp) {
					return -ENOMEM;
				}

				ret = fh2m_inno_copy_from_user(&(vbp->buf.vb), (vpudrv_buffer_t *)arg, sizeof(vpudrv_buffer_t));
				if (ret) {
					fh2m_inno_kfree(vbp);
					return -EFAULT;
				}
				vb = vpu_import_dmabuf(&(vpu_drv_ctx->drv_context), &(vbp->buf.obj));
				if (IS_ERR_OR_NULL(vb)) {
					ret = PTR_ERR(vb);
					fh2m_inno_kfree(vbp);
					break;
				}

				ret = copy_to_user((void __user *)arg, vb, sizeof(vpudrv_buffer_t));
				if (ret) {
					fh2m_inno_kfree(vbp);
					ret = -EFAULT;
					vpu_destroy_dmabuf(&(vpu_drv_ctx->drv_context), &(vbp->buf.obj));
					break;
				}

				vbp->filp = filp;
				vbp->buf.obj.buf_type = BUF_TYPE_VPU_IMPORT;
				fh2m_inno_atomic64_add(vbp->buf.vb.size, &(vpu_drv_ctx->drv_context.external_mem));
				fh2m_inno_spin_lock(vpu_drv_ctx->drv_context.vpu_lock);
				list_add(&vbp->list, &vpu_drv_ctx->drv_context.vbp_head);
				fh2m_inno_spin_unlock(vpu_drv_ctx->drv_context.vpu_lock);

				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "[-]VDI_IOCTL_DMABUF_IMPORT\n");
			}
			break;
		case VDI_IOCTL_DMABUF_DESTROY:
			{
				vpudrv_buffer_pool_t *vbp = NULL, *n = NULL;
				vpudrv_buffer_t vb;
				vpu_buf_obj_t obj;
				int found = 0;

				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "[+]VDI_IOCTL_DMABUF_DESTROY\n");
				ret = fh2m_inno_copy_from_user(&vb, (vpudrv_buffer_t *)arg, sizeof(vpudrv_buffer_t));
				if (ret < 0) {
					return -EFAULT;
				}

				fh2m_inno_spin_lock(vpu_drv_ctx->drv_context.vpu_lock);
				list_for_each_entry_safe(vbp, n, &vpu_drv_ctx->drv_context.vbp_head, list) {
					if ((vbp->buf.vb.fd == vb.fd) && (vbp->filp == filp) && (vbp->buf.vb.phys_addr == vb.phys_addr)) {
						found = 1;
						obj = vbp->buf.obj;
						list_del(&vbp->list);
						fh2m_inno_kfree(vbp);
						break;
					}
				}
				fh2m_inno_spin_unlock(vpu_drv_ctx->drv_context.vpu_lock);

				if (found == 1) {
					vpu_destroy_dmabuf(&(vpu_drv_ctx->drv_context), &obj);
				}
				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "[-]VDI_IOCTL_DMABUF_DESTROY\n");
			}
			break;
		case VDI_IOCTL_DMAFENCE_SIGNAL:
			{
#ifdef DMA_FENCE_SYNC_SUPPORT
				sync_event_t ev;
				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "[+]VDI_IOCTL_DMAFENCE_SIGNAL\n");

				ret = fh2m_inno_copy_from_user(&ev, (sync_event_t *)arg, sizeof(ev));
				if (ret < 0) {
					return -EFAULT;
				}
				ret = vpu_dma_fence_signal(ev.fd, ev.event, NULL);
				if (ret) {
					vpu_error(vpu_drv_ctx->drv_context.vpudev, "vpu hw reset failed\n");
					return -EFAULT;
				}
				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "[-]VDI_IOCTL_DMAFENCE_SIGNAL\n");
#endif
			}
			break;
		case VDI_IOCTL_DMAFENCE_CREATE:
			{

#ifdef DMA_FENCE_SYNC_SUPPORT
				sync_event_t ev;
				struct dma_fence *dma_buf_fence = NULL;
				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "[+]VDI_IOCTL_DMAFENCE_CREATE\n");

				ret = fh2m_inno_copy_from_user(&ev, (sync_event_t *)arg, sizeof(ev));
				if (ret < 0) {
					return -EFAULT;
				}
				ret = vpu_dma_fence_create(ev.fd, ev.event, NULL, &dma_buf_fence);
				if (ret != 0) {
					vpu_error(vpu_drv_ctx->drv_context.vpudev, "vpu_dma_fence_create failed\n");
					return -EFAULT;
				}
				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "[-]VDI_IOCTL_DMAFENCE_CREATE\n");
#endif
			}
			break;
		case VDI_IOCTL_WAIT_INTERRUPT:
			{
				vpudrv_intr_info_t info;
#ifdef SUPPORT_MULTI_INST_INTR
				u32 intr_inst_index;
				u32 intr_reason_in_q;
				u32 interrupt_flag_in_q;
#endif
				struct vpu_context_t *vpu_ctx = &vpu_drv_ctx->drv_context.vpu_context;

				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "[+]VDI_IOCTL_WAIT_INTERRUPT\n");

				ret = fh2m_inno_copy_from_user(&info, (vpudrv_intr_info_t *)arg, sizeof(vpudrv_intr_info_t));
				if (ret != 0)
				{
					return -EFAULT;
				}
#ifdef SUPPORT_MULTI_INST_INTR
				intr_inst_index = info.intr_inst_index;

				intr_reason_in_q = 0;
				interrupt_flag_in_q = fh2m_inno_kfifo_out_spinlocked(vpu_drv_ctx->drv_context.interrupt_pending_q[intr_inst_index],
					&intr_reason_in_q, sizeof(u32), vpu_drv_ctx->drv_context.kfifo_lock);
				if (interrupt_flag_in_q > 0)
				{
					vpu_ctx->interrupt_reason[intr_inst_index] = intr_reason_in_q;
					vpu_dbg(vpu_drv_ctx->drv_context.vpudev, " Interrupt Remain : intr_inst_index=%d, intr_reason_in_q=0x%x, interrupt_flag_in_q=%d\n",
						intr_inst_index, intr_reason_in_q, interrupt_flag_in_q);
					goto INTERRUPT_REMAIN_IN_QUEUE;
				}

				ret = inno_wait_event_interruptible_timeout((vpu_drv_ctx->drv_context.interrupt_wait_q[intr_inst_index]),
					vpu_drv_ctx->drv_context.interrupt_flag[intr_inst_index] != 0, fh2m_inno_msecs_to_jiffies(info.timeout));

#endif
				if (!ret) {
					//vpu_error(vpu_drv_ctx->drv_context.vpudev, "[-]VDI_IOCTL_WAIT_INTERRUPT timeout = %d \n", info.timeout);
					ret = -ETIME;
					break;
				}

				if (signal_pending(fh2m_inno_get_current_task())) {
					ret = -ERESTARTSYS;
					break;
				}

#ifdef SUPPORT_MULTI_INST_INTR
				intr_reason_in_q = 0;
				interrupt_flag_in_q = fh2m_inno_kfifo_out_spinlocked(vpu_drv_ctx->drv_context.interrupt_pending_q[intr_inst_index],
					&intr_reason_in_q, sizeof(u32), vpu_drv_ctx->drv_context.kfifo_lock);
				if (interrupt_flag_in_q > 0) {
					vpu_ctx->interrupt_reason[intr_inst_index] = intr_reason_in_q;
				}
				else {
					vpu_ctx->interrupt_reason[intr_inst_index] = 0;
				}
				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, " inst_index(%d), interrupt_flag(%d), reason(0x%08llx)\n", intr_inst_index,
					vpu_drv_ctx->drv_context.interrupt_flag[intr_inst_index], vpu_ctx->interrupt_reason[intr_inst_index]);

INTERRUPT_REMAIN_IN_QUEUE:
				info.intr_reason = vpu_ctx->interrupt_reason[intr_inst_index];
				vpu_drv_ctx->drv_context.interrupt_flag[intr_inst_index] = 0;
				vpu_ctx->interrupt_reason[intr_inst_index] = 0;
#endif
				ret = copy_to_user((void __user *)arg, &info, sizeof(vpudrv_intr_info_t));
				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "[-]VDI_IOCTL_WAIT_INTERRUPT\n");
				if (ret != 0)
				{
					return -EFAULT;
				}
			}
			break;
		case VDI_IOCTL_SET_CLOCK_GATE:
			{
				u32 clkgate;

				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "[+]VDI_IOCTL_SET_CLOCK_GATE\n");
				//if (get_user(clkgate, (u32 __user *) arg))
				if (fh2m_inno_copy_from_user(&clkgate,  (void *)arg, sizeof(u32)))
					return -EFAULT;
				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "[-]VDI_IOCTL_SET_CLOCK_GATE\n");

			}
			break;
		case VDI_IOCTL_GET_INSTANCE_POOL:
			{
				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "[+]VDI_IOCTL_GET_INSTANCE_POOL\n");
				if ((ret = down_interruptible(vpu_drv_ctx->drv_context.vpu_sem)) == 0) {

					if (vpu_drv_ctx->drv_context.instance_pool.base != 0) {

						ret = copy_to_user((void __user *)arg, &vpu_drv_ctx->drv_context.instance_pool, sizeof(vpudrv_buffer_t));
						if (ret != 0)
							ret = -EFAULT;
					} else {

						ret = fh2m_inno_copy_from_user(&vpu_drv_ctx->drv_context.instance_pool, (vpudrv_buffer_t *)arg, sizeof(vpudrv_buffer_t));
						if (ret == 0) {
							vpu_drv_ctx->drv_context.instance_pool.base = (u64)fh2m_inno_vmalloc(PAGE_ALIGN(vpu_drv_ctx->drv_context.instance_pool.size));
							vpu_drv_ctx->drv_context.instance_pool.phys_addr = vpu_drv_ctx->drv_context.instance_pool.base;
							if (vpu_drv_ctx->drv_context.instance_pool.base != 0) {
								/*clearing memory*/
								fh2m_inno_memset((void *)vpu_drv_ctx->drv_context.instance_pool.base, 0x0, PAGE_ALIGN(vpu_drv_ctx->drv_context.instance_pool.size));
								ret = copy_to_user((void __user *)arg, &vpu_drv_ctx->drv_context.instance_pool, sizeof(vpudrv_buffer_t));
								if (ret == 0) {
									/* success to get memory for instance pool */
									fh2m_inno_up(vpu_drv_ctx->drv_context.vpu_sem);
									break;
								}
							}
						}

						ret = -EFAULT;
					}
					fh2m_inno_up(vpu_drv_ctx->drv_context.vpu_sem);
				}

				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "[-]VDI_IOCTL_GET_INSTANCE_POOL\n");
			}
			break;
		case VDI_IOCTL_GET_COMMON_MEMORY:
			{
				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "[+]VDI_IOCTL_GET_COMMON_MEMORY\n");
				if (vpu_drv_ctx->drv_context.common_memory.base != 0) {
					ret = copy_to_user((void __user *)arg, &vpu_drv_ctx->drv_context.common_memory, sizeof(vpudrv_buffer_t));
					if (ret != 0) {
						ret = -EFAULT;
						break;
					}
				} else {
					ret = fh2m_inno_copy_from_user(&vpu_drv_ctx->drv_context.common_memory, (vpudrv_buffer_t *)arg, sizeof(vpudrv_buffer_t));
					if (ret != 0) {
						ret = -EFAULT;
						break;
					}
					ret = vpu_alloc_dma_buffer(&vpu_drv_ctx->drv_context.common_memory, &(vpu_drv_ctx->drv_context));
					if (ret != 0) {
						ret = -EFAULT;
						break;
					}
 					ret = copy_to_user((void __user *)arg, &vpu_drv_ctx->drv_context.common_memory, sizeof(vpudrv_buffer_t));
					if (ret != 0) {
						vpu_free_dma_buffer(&vpu_drv_ctx->drv_context.common_memory, &(vpu_drv_ctx->drv_context));
						ret = -EFAULT;
						break;
					}
				}
				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "[-]VDI_IOCTL_GET_COMMON_MEMORY\n");
			}
			break;
		case VDI_IOCTL_GET_PVRIC_MEMORY:
			{
				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "[+]VDI_IOCTL_GET_PVRIC_MEMORY\n");
				if (vpu_drv_ctx->drv_context.pvric_memory.base != 0) {
					ret = copy_to_user((void __user *)arg, &vpu_drv_ctx->drv_context.pvric_memory, sizeof(vpudrv_buffer_t));
					if (ret != 0) {
						ret = -EFAULT;
						break;
					}
				} else {
					ret = fh2m_inno_copy_from_user(&vpu_drv_ctx->drv_context.pvric_memory, (vpudrv_buffer_t *)arg, sizeof(vpudrv_buffer_t));
					if (ret != 0) {
						ret = -EFAULT;
						break;
					}
					ret = vpu_alloc_dma_buffer(&vpu_drv_ctx->drv_context.pvric_memory, &(vpu_drv_ctx->drv_context));
					if (ret != 0) {
						ret = -EFAULT;
						break;
					}
					ret = copy_to_user((void __user *)arg, &vpu_drv_ctx->drv_context.pvric_memory, sizeof(vpudrv_buffer_t));
					if (ret != 0) {
						vpu_free_dma_buffer(&vpu_drv_ctx->drv_context.pvric_memory, &(vpu_drv_ctx->drv_context));
						ret = -EFAULT;
						break;
					}
				}
				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "[-]VDI_IOCTL_GET_PVRIC_MEMORY\n");
			}
			break;
		case VDI_IOCTL_OPEN_INSTANCE:
			{
				vpudrv_inst_info_t inst_info;
				vpudrv_instanace_list_t *vil, *n = NULL;
				int codecIndex = -1;

				vil = fh2m_inno_kzalloc_kernel(sizeof(*vil));
				if (!vil) {
					return -ENOMEM;
				}
				if (fh2m_inno_copy_from_user(&inst_info, (vpudrv_inst_info_t *)arg, sizeof(vpudrv_inst_info_t))) {
					fh2m_inno_kfree(vil);
					return -EFAULT;
				}

				codecIndex = vpu_capture_inst_index(&(vpu_drv_ctx->drv_context), inst_info.core_idx);
				if (codecIndex < 0) {
					fh2m_inno_kfree(vil);
					return -EFAULT;
				}
				inst_info.inst_idx = codecIndex;

				vil->inst_idx = inst_info.inst_idx;
				vil->core_idx = inst_info.core_idx;
				vil->filp = (void*)filp;
				vil->status = VPU_INST_STATUS_IDLE;

				fh2m_inno_spin_lock(vpu_drv_ctx->drv_context.vpu_lock);
				list_add(&vil->list, &vpu_drv_ctx->drv_context.inst_list_head);

				inst_info.inst_open_count = 0; /* counting the current open instance number */
				list_for_each_entry_safe(vil, n, &vpu_drv_ctx->drv_context.inst_list_head, list)
				{
					if (vil->core_idx == inst_info.core_idx) {
						inst_info.inst_open_count++;
					}
				}
				vpu_drv_ctx->drv_context.vpu_open_ref_count += 1; /* flag just for that vpu is in opened or closed */
				fh2m_inno_spin_unlock(vpu_drv_ctx->drv_context.vpu_lock);

				ret = vpu_mutex_lock(inst_info.core_idx, inst_info.inst_idx, VPUDRV_MUTEX_VPU, &(vpu_drv_ctx->drv_context));
				if (ret != 0 && ret != 1) {
					return -EFAULT;
				}
#ifdef SUPPORT_MULTI_INST_INTR
				fh2m_inno_kfifo_reset(vpu_drv_ctx->drv_context.interrupt_pending_q[inst_info.inst_idx]);
#endif
				vpu_mutex_unlock(inst_info.core_idx, inst_info.inst_idx, VPUDRV_MUTEX_VPU, &(vpu_drv_ctx->drv_context));

				if (copy_to_user((void __user *)arg, &inst_info, sizeof(vpudrv_inst_info_t))) {
					return -EFAULT;
				}

				fh2m_inno_spin_lock(vpu_drv_ctx->drv_context.inst_lock_info_lock[inst_info.inst_idx]);
				INIT_LIST_HEAD(&vpu_drv_ctx->drv_context.inst_lock_info_head[inst_info.inst_idx]);
				fh2m_inno_spin_unlock(vpu_drv_ctx->drv_context.inst_lock_info_lock[inst_info.inst_idx]);

				fh2m_inno_spin_lock(vpu_drv_ctx->drv_context.cmd_message_lock[inst_info.inst_idx]);
				INIT_LIST_HEAD(&vpu_drv_ctx->drv_context.cmd_message_head[inst_info.inst_idx]);
				fh2m_inno_spin_unlock(vpu_drv_ctx->drv_context.cmd_message_lock[inst_info.inst_idx]);

				fh2m_inno_spin_lock(vpu_drv_ctx->drv_context.input_buffer_lock[inst_info.inst_idx]);
				INIT_LIST_HEAD(&vpu_drv_ctx->drv_context.input_buffer_head[inst_info.inst_idx]);
				fh2m_inno_spin_unlock(vpu_drv_ctx->drv_context.input_buffer_lock[inst_info.inst_idx]);

				fh2m_inno_spin_lock(vpu_drv_ctx->drv_context.source_buffer_lock[inst_info.inst_idx]);
				INIT_LIST_HEAD(&vpu_drv_ctx->drv_context.source_buffer_head[inst_info.inst_idx]);
				fh2m_inno_spin_unlock(vpu_drv_ctx->drv_context.source_buffer_lock[inst_info.inst_idx]);
				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, " VDI_IOCTL_OPEN_INSTANCE core_idx=%d, inst_idx=%d, vpu_open_ref_count=%d, inst_open_count=%d\n",
					(u32)inst_info.core_idx, (u32)inst_info.inst_idx, vpu_drv_ctx->drv_context.vpu_open_ref_count, inst_info.inst_open_count);
			}
			break;
		case VDI_IOCTL_CLOSE_INSTANCE:
			{
				vpudrv_inst_info_t inst_info;
				vpudrv_instanace_list_t *vil = NULL, *n = NULL;
				vpu_lock_info_list_t *vlil = NULL, *vliln = NULL;
				uint64_t vpu_load = 0;
				u32 found = 0;

				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "[+]VDI_IOCTL_CLOSE_INSTANCE\n");
				if (fh2m_inno_copy_from_user(&inst_info, (vpudrv_inst_info_t *)arg, sizeof(vpudrv_inst_info_t)))
					return -EFAULT;

				fh2m_inno_spin_lock(vpu_drv_ctx->drv_context.vpu_lock);
				list_for_each_entry_safe(vil, n, &vpu_drv_ctx->drv_context.inst_list_head, list)
				{
					if (vil->inst_idx == inst_info.inst_idx && vil->core_idx == inst_info.core_idx) {
						vpu_load = (uint64_t)vil->inst_param.width * vil->inst_param.height * vil->inst_param.frameRate;
						atomic64_sub(vpu_load, &(vpu_drv_ctx->drv_context.statistic_load));
						list_del(&vil->list);
						fh2m_inno_kfree(vil);
						found = 1;
						break;
					}
				}

				if (0 == found) {
					fh2m_inno_spin_unlock(vpu_drv_ctx->drv_context.vpu_lock);
					return -EINVAL;
				}

				inst_info.inst_open_count = 0; /* counting the current open instance number */
				vpu_drv_ctx->drv_context.vpu_open_ref_count -= 1; /* flag just for that vpu is in opened or closed */
				list_for_each_entry_safe(vil, n, &vpu_drv_ctx->drv_context.inst_list_head, list)
				{
					if (vil->core_idx == inst_info.core_idx) {
						inst_info.inst_open_count++;
					}
				}
				fh2m_inno_spin_unlock(vpu_drv_ctx->drv_context.vpu_lock);

				fh2m_inno_spin_lock(vpu_drv_ctx->drv_context.inst_lock_info_lock[inst_info.inst_idx]);
				list_for_each_entry_safe(vlil, vliln, &vpu_drv_ctx->drv_context.inst_lock_info_head[inst_info.inst_idx], list) {
					list_del(&vlil->list);
					fh2m_inno_kfree(vlil);
				}
				fh2m_inno_spin_unlock(vpu_drv_ctx->drv_context.inst_lock_info_lock[inst_info.inst_idx]);

				ret = vpu_mutex_lock(inst_info.core_idx, inst_info.inst_idx, VPUDRV_MUTEX_VPU, &(vpu_drv_ctx->drv_context));
				if (ret != 0 && ret != 1) {
					return -EFAULT;
				}
#ifdef SUPPORT_MULTI_INST_INTR
				fh2m_inno_kfifo_reset(vpu_drv_ctx->drv_context.interrupt_pending_q[inst_info.inst_idx]);
#endif
				vpu_mutex_unlock(inst_info.core_idx, inst_info.inst_idx, VPUDRV_MUTEX_VPU, &(vpu_drv_ctx->drv_context));

				if (vpu_release_inst_index(&(vpu_drv_ctx->drv_context), inst_info.core_idx, inst_info.inst_idx) < 0) {
					return -EFAULT;
				}

				if (copy_to_user((void __user *)arg, &inst_info, sizeof(vpudrv_inst_info_t)))
					return -EFAULT;

				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, " VDI_IOCTL_CLOSE_INSTANCE core_idx=%d, inst_idx=%d, vpu_open_ref_count=%d, inst_open_count=%d\n",
					(u32)inst_info.core_idx, (u32)inst_info.inst_idx, vpu_drv_ctx->drv_context.vpu_open_ref_count, inst_info.inst_open_count);
			}
			break;
		case VDI_IOCTL_GET_INSTANCE_NUM:
			{
				vpudrv_inst_info_t inst_info;
				vpudrv_instanace_list_t *vil = NULL, *n = NULL;
				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "[+]VDI_IOCTL_GET_INSTANCE_NUM\n");

				ret = fh2m_inno_copy_from_user(&inst_info, (vpudrv_inst_info_t *)arg, sizeof(vpudrv_inst_info_t));
				if (ret != 0)
					break;

				fh2m_inno_spin_lock(vpu_drv_ctx->drv_context.vpu_lock);
				inst_info.inst_open_count = 0;
				list_for_each_entry_safe(vil, n, &vpu_drv_ctx->drv_context.inst_list_head, list)
				{
					if (vil->core_idx == inst_info.core_idx) {
						inst_info.inst_open_count++;
					}
				}
				fh2m_inno_spin_unlock(vpu_drv_ctx->drv_context.vpu_lock);

				ret = copy_to_user((void __user *)arg, &inst_info, sizeof(vpudrv_inst_info_t));
				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, " VDI_IOCTL_GET_INSTANCE_NUM core_idx=%d, inst_idx=%d, open_count=%d\n",
					(u32)inst_info.core_idx, (u32)inst_info.inst_idx, inst_info.inst_open_count);
			}
			break;
		case VDI_IOCTL_RESET:
			{
				ret = vpu_hw_reset(&(vpu_drv_ctx->drv_context));
				if (ret) {
					vpu_error(vpu_drv_ctx->drv_context.vpudev, "vpu hw reset failed\n");
					return -EINVAL;
				}
			}
			break;
		case VDI_IOCTL_DMA_TRANSFER:
			{
				vpu_dma_info_t info;
				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "[+]VDI_IOCTL_DMA_TRANSFER \n");

				ret = fh2m_inno_copy_from_user(&info, (vpu_dma_info_t *)arg, sizeof(vpu_dma_info_t));
				if (ret) {
					return -EFAULT;
				}
				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, " direction %d src domain %d dst domain %d\n",
							info.direction, info.src_vb.domain, info.dest_vb.domain);
				ret = vpu_dma_transfer(&(vpu_drv_ctx->drv_context), info, filp);
				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "[-]VDI_IOCTL_DMA_TRANSFER\n");
			}
			break;
		case VDI_IOCTL_DMA_TRANSFER_FAST:
			{
				vpu_dma_info_t info;
				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "[+]VDI_IOCTL_DMA_TRANSFER_FAST \n");
				ret = copy_from_user(&info, (vpu_dma_info_t *)arg, sizeof(vpu_dma_info_t));
				if (ret) {
					return -EFAULT;
				}
				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, " direction %d src domain %d dst domain %d\n",
							info.direction, info.src_vb.domain, info.dest_vb.domain);
				ret = vpu_dma_transfer_fast(&(vpu_drv_ctx->drv_context), info);
				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "[-]VDI_IOCTL_DMA_TRANSFER_FAST\n");
			}
			break;
		case VDI_IOCTL_GET_REGISTER_INFO:
			{
				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "[+]VDI_IOCTL_GET_REGISTER_INFO\n");
				ret = copy_to_user((void __user *)arg, &vpu_drv_ctx->drv_context.vpu_register, sizeof(vpudrv_buffer_t));
				if (ret != 0) {
					ret = -EFAULT;
				}
				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "[-]VDI_IOCTL_GET_REGISTER_INFO vpu_register.phys_addr==0x%llx, vpu_register.virt_addr=0x%llx, vpu_register.size=%d\n",
					vpu_drv_ctx->drv_context.vpu_register.phys_addr , vpu_drv_ctx->drv_context.vpu_register.virt_addr, vpu_drv_ctx->drv_context.vpu_register.size);
			}
			break;
		case VDI_IOCTL_GET_FREE_MEM_SIZE:
			{
				u64 size;

				size = vpu_get_free_mem_size(&(vpu_drv_ctx->drv_context));
				if (copy_to_user((void __user *)arg, &size, sizeof(u64))) {
					return -EFAULT;
				}
			}
			break;
		case VDI_IOCTL_GET_CHIP_INFO:
			{
				if (copy_to_user((void __user *)arg, &vpu_drv_ctx->drv_context.chip_info, sizeof(vpudrv_chip_info_t))) {
					return -EFAULT;
				}
			}
			break;
		case VDI_IOCTL_LOCK_ENQUEUE:
			{
				vpu_lock_info_t lock_info;

				if (fh2m_inno_copy_from_user(&lock_info, (vpu_lock_info_t *)arg, sizeof(vpu_lock_info_t))) {
					return -EFAULT;
				}

				ret = vpu_lock_enqueue(&(vpu_drv_ctx->drv_context), &lock_info);
			}
			break;
		case VDI_IOCTL_LOCK_DEQUEUE:
			{
				u64 instIndex;
				if (fh2m_inno_copy_from_user(&instIndex, (u64 *)arg, sizeof(u64))) {
					return -EFAULT;
				}

				ret = vpu_lock_dequeue(&(vpu_drv_ctx->drv_context), instIndex);
			}
			break;
		case VDI_IOCTL_TRY_LOCK:
			{
				u64 instIndex;
				if (fh2m_inno_copy_from_user(&instIndex, (u64 *)arg, sizeof(u64))) {
					return -EFAULT;
				}

				ret = vpu_try_lock(&(vpu_drv_ctx->drv_context), instIndex);
			}
			break;
		case VDI_IOCTL_UNLOCK:
			{
				u64 instIndex;
				if (fh2m_inno_copy_from_user(&instIndex, (u64 *)arg, sizeof(u64))) {
					return -EFAULT;
				}

				ret = vpu_unlock(&(vpu_drv_ctx->drv_context), instIndex);
			}
			break;
		case VDI_IOCTL_CREATE_INSTANCE:
			{
				u64 instIndex;
				if (fh2m_inno_copy_from_user(&instIndex, (u64 *)arg, sizeof(u64))) {
					return -EFAULT;
				}

				ret = vpu_create_instance(&(vpu_drv_ctx->drv_context), instIndex);
			}
			break;
		case VDI_IOCTL_DESTROY_INSTANCE:
			{
				u64 instIndex;
				if (fh2m_inno_copy_from_user(&instIndex, (u64 *)arg, sizeof(u64))) {
					return -EFAULT;
				}

				ret = vpu_destroy_instance(&(vpu_drv_ctx->drv_context), instIndex);
			}
			break;
		case VDI_IOCTL_INCREASE_LOAD:
			{
				vpudrv_instanace_list_t *vil = NULL, *n = NULL;
				vpu_instance_param_t instParam;
				uint64_t vpu_load = 0;
				if (fh2m_inno_copy_from_user(&instParam, (vpu_instance_param_t *)arg, sizeof(vpu_instance_param_t))) {
					return -EFAULT;
				}

				fh2m_inno_spin_lock(vpu_drv_ctx->drv_context.vpu_lock);
				list_for_each_entry_safe(vil, n, &vpu_drv_ctx->drv_context.inst_list_head, list)
				{
					if (vil->filp == filp) {
						vil->inst_param.width = instParam.width;
						vil->inst_param.height = instParam.height;
						vil->inst_param.frameRate = instParam.frameRate;
						vpu_info(vpu_drv_ctx->drv_context.dev, "instance_id:%d width:%u height:%u frame_rate:%u codec_type:0x%x\n", vil->inst_idx,
							instParam.width, instParam.height, instParam.frameRate, instParam.codec_type);
						vpu_load = instParam.width * instParam.height * instParam.frameRate;
						fh2m_inno_atomic64_add(vpu_load, &(vpu_drv_ctx->drv_context.statistic_load));
						break;
					}
				}
				fh2m_inno_spin_unlock(vpu_drv_ctx->drv_context.vpu_lock);
			}
			break;
		case VDI_IOCTL_GET_VRAM_INFO:
			{
				struct role_target role;
				vpu_vram_status vpu_visible_stats;
				struct vram_stats visible_stats;
				if (copy_from_user(&vpu_visible_stats, (vpu_vram_status *)arg, sizeof(vpu_vram_status))) {
					return -EFAULT;
				}
				fh2m_inno_memset(&visible_stats, 0, sizeof(struct vram_stats));
				fh2m_inno_spin_lock(vpu_drv_ctx->drv_context.vpu_lock);
				role.vram_role = HAL_VRAM_ROLE_VPU;
				role.id = vpu_drv_ctx->drv_context.vpu_id;
				role.sub_id = 0;
				if (fh2m_hal_get_vram_stats(vpu_drv_ctx->drv_context.parent, &role, true, &visible_stats) != 0) {
					vpu_error(vpu_drv_ctx->drv_context.dev, "fh2m_hal_get_vram_stats failed\n");
					fh2m_inno_spin_unlock(vpu_drv_ctx->drv_context.vpu_lock);
					return -EINVAL;
				}
				vpu_visible_stats.free_size = visible_stats.free_size;
				vpu_visible_stats.total_size = visible_stats.total_size;
				fh2m_inno_spin_unlock(vpu_drv_ctx->drv_context.vpu_lock);
				if (copy_to_user((void __user *)arg, &vpu_visible_stats, sizeof(vpu_vram_status))) {
					return -EFAULT;
				}
			}
			break;
		case VDI_IOCTL_EXECUTE_SYNC_COMMAND:
			{
				vpu_sync_cmd_t sync_cmd_from_user, sync_cmd;
				uint32_t *buffer = NULL;
				int instance_id = -1;
				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "[+]VDI_IOCTL_EXECUTE_SYNC_COMMAND\n");
				if (copy_from_user(&sync_cmd_from_user, (vpu_sync_cmd_t *)arg, sizeof(vpu_sync_cmd_t))) {
					vpu_error(vpu_drv_ctx->drv_context.vpudev, "copy_from_user failed.\n");
					return -EFAULT;
				}

				instance_id = vpu_get_instance_id(filp);
				if (instance_id < 0) {
					vpu_error(vpu_drv_ctx->drv_context.vpudev,"invalid instance id=%d, need open instance first\n", instance_id);
					return -EFAULT;
				}

				sync_cmd = sync_cmd_from_user;

				if (vpu_is_creat_inst_cmd(sync_cmd.cmd_type)) {
					vpu_drv_ctx->drv_context.proc_num[instance_id] = 0;
					vpu_drv_ctx->drv_context.status[instance_id] = VPU_STATUS_SUCCESS;
				}

				TRACE_PRINTK("vpu%u idx %u proc %u type %d sync_s\n", vpu_drv_ctx->drv_context.vpu_id, instance_id,
					vpu_drv_ctx->drv_context.proc_num[instance_id], sync_cmd.cmd_type);
				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "sync_cmd buffer size %d, or reg_mirror buffer size %d.\n", sync_cmd.buffer_size, sync_cmd.reg_mirror_size);
				if (sync_cmd.buffer_size > 1000 || sync_cmd.reg_mirror_size > 0x1000) {
					vpu_error(vpu_drv_ctx->drv_context.vpudev, "Insane sync_cmd buffer size %d, or reg_mirror buffer size %d.\n", sync_cmd.buffer_size, sync_cmd.reg_mirror_size);
					return -EFAULT;
				}

				/* It's inconvenient to use umd buffer in kernel, duplicate it instead. */
				buffer = fh2m_inno_vmalloc(sync_cmd_from_user.buffer_size*4);
				if(!buffer) {
					vpu_error(vpu_drv_ctx->drv_context.vpudev, "buffer vmalloc failed size:%d.\n", sync_cmd_from_user.buffer_size);
					return -EFAULT;
				}

				if (copy_from_user(buffer, (uint32_t *)sync_cmd_from_user.buffer, sync_cmd_from_user.buffer_size*4)) {
					vpu_error(vpu_drv_ctx->drv_context.vpudev, "copy_from_user for cmd buffer failed.\n");
					fh2m_inno_vfree(buffer);
					return -EFAULT;
				}
				sync_cmd.buffer = (uint64_t)buffer;
				sync_cmd.reg_mirror = (uint64_t)fh2m_inno_vmalloc(sync_cmd.reg_mirror_size);
				if(!sync_cmd.reg_mirror) {
					vpu_error(vpu_drv_ctx->drv_context.vpudev, "reg_mirror vmalloc failed size:%d.\n", sync_cmd.reg_mirror_size);
					vfree(buffer);
					return -EFAULT;
				}

				vpu_exec_sync_cmd(&(vpu_drv_ctx->drv_context), &sync_cmd, instance_id, false);

				ret = copy_to_user((void __user *)sync_cmd_from_user.reg_mirror,(void *)sync_cmd.reg_mirror, sync_cmd.reg_mirror_size);
				TRACE_PRINTK("vpu%u idx %u proc %u type %d sync_e\n", vpu_drv_ctx->drv_context.vpu_id, instance_id,
					vpu_drv_ctx->drv_context.proc_num[instance_id], sync_cmd.cmd_type);
				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "vpu%u inst_idx %u proc %u cmd_type %d\n", vpu_drv_ctx->drv_context.vpu_id, instance_id,
					vpu_drv_ctx->drv_context.proc_num[instance_id], sync_cmd.cmd_type);
				fh2m_inno_vfree(buffer);
				fh2m_inno_vfree((void *)sync_cmd.reg_mirror);
				if (vpu_drv_ctx->drv_context.status[instance_id] != VPU_STATUS_SUCCESS) {
					return -EFAULT;
				}
				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "[-]VDI_IOCTL_EXECUTE_SYNC_COMMAND\n");
			}
			break;
		case VDI_IOCTL_SEND_ASYNC_COMMAND:
		#ifdef USE_REFACTOR_LOGIC
			{
				vpu_cmd_message_t *p_cmd_message = NULL;
				vpu_async_cmd_t async_cmd;
				int instance_id, i;
				uint32_t *buffer = NULL;

				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "[+]VDI_IOCTL_SEND_ASYNC_COMMAND\n");
				if (copy_from_user(&async_cmd, (vpu_async_cmd_t *)arg, sizeof(vpu_async_cmd_t))) {
					vpu_error(vpu_drv_ctx->drv_context.vpudev, "copy_from_user failed.\n");
					return -EFAULT;
				}

				if (!async_cmd.buffer || async_cmd.buffer_size == 0) {
					vpu_error(vpu_drv_ctx->drv_context.vpudev, "async_cmd.buffer is NULL.\n");
					return -EFAULT;
				}

				instance_id = vpu_get_instance_id(filp);
				if (instance_id < 0) {
					vpu_error(vpu_drv_ctx->drv_context.vpudev, "invalid instance id=%d, need open instance first\n", instance_id);
					return -EFAULT;
				}

				if (vpu_drv_ctx->drv_context.status[instance_id] != VPU_STATUS_SUCCESS) {
					return -EFAULT;
				}

				if(!vpu_drv_ctx->drv_context.vpu_fence_msg[instance_id].instance_fence_processed_q || !vpu_drv_ctx->drv_context.vpu_fence_msg[instance_id].instance_fence_processed_q_lock
				 || !vpu_drv_ctx->drv_context.vpu_fence_msg[instance_id].vpu_fence_thread_wq) {
					vpu_error(vpu_drv_ctx->drv_context.vpudev, "vpu_fence_msg is NULL instance id=%d\n", instance_id);
					return -EFAULT;
				}

				p_cmd_message = fh2m_inno_vzalloc(sizeof(vpu_cmd_message_t));
				if(!p_cmd_message) {
					vpu_error(vpu_drv_ctx->drv_context.vpudev, "p_cmd_message vzalloc failed\n");
					return -EFAULT;
				}

				buffer = fh2m_inno_vmalloc(async_cmd.buffer_size*4);
				if(!buffer) {
					vpu_error(vpu_drv_ctx->drv_context.vpudev, "buffer vmalloc failed\n");
					fh2m_inno_vfree(p_cmd_message);
					return -EFAULT;
				}
				if (copy_from_user(buffer, (void __user *)async_cmd.buffer, async_cmd.buffer_size*4)) {
					vpu_error(vpu_drv_ctx->drv_context.vpudev, "copy_from_user failed.\n");
					fh2m_inno_vfree(buffer);
					fh2m_inno_vfree(p_cmd_message);
					return -EFAULT;
				}

				if (vpu_is_decpic_or_encpic_cmd(async_cmd.cmd_type)) {
					vpu_drv_ctx->drv_context.proc_num[instance_id]++;
				}

				TRACE_PRINTK("vpu%u idx %u proc %u type %d async_s\n", vpu_drv_ctx->drv_context.vpu_id, instance_id,
					vpu_drv_ctx->drv_context.proc_num[instance_id], async_cmd.cmd_type);

				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "vpu_id%u inst_idx %u proc_num %u cmd_type %d\n", vpu_drv_ctx->drv_context.vpu_id, instance_id,
					vpu_drv_ctx->drv_context.proc_num[instance_id], async_cmd.cmd_type);
				p_cmd_message->is_sync_cmd = false;
				p_cmd_message->cmd_internal_t.async_cmd.buffer = buffer;
				p_cmd_message->cmd_internal_t.async_cmd.buffer_size = async_cmd.buffer_size;
				p_cmd_message->instance_id = instance_id;
				p_cmd_message->time_stamp = fh2m_inno_clockmonotonic_raw();
				p_cmd_message->cmd_internal_t.async_cmd.cmd_type = async_cmd.cmd_type;
				p_cmd_message->proc_num = vpu_drv_ctx->drv_context.proc_num[instance_id];

				for (i = 0; i < DMA_BUF_FD_COUNT_MAX; i++) {
					if (async_cmd.fd[i].buffer_fd < 0) break;
					if (async_cmd.fd[i].buffer_type == BUFFER_TYPE_OUTPUT || async_cmd.fd[i].buffer_type == BUFFER_TYPE_OUTPUT_NEED_DECODE_COPY ||
						async_cmd.fd[i].buffer_type ==BUFFER_TYPE_OUTPUT_NEED_DISPLAY_COPY) {  //output buffer index
						vpu_buf_obj_t* output_buffer_vb = vpu_get_vb_from_fd(filp, async_cmd.fd[i].buffer_fd);//obtain from list
						if (output_buffer_vb) {
							//due to buffer released causing kernel crash
							//if(output_buffer_vb->map_addr == 0 && output_buffer_vb->vb.domain != INNO_VPU_GEM_DOMAIN_VRAM) {
							//	output_buffer_vb->map_addr = (uint64_t)fh2m_inno_ioremap_nocache(output_buffer_vb->vb.phys_addr, output_buffer_vb->vb.size);
							//	if(output_buffer_vb->map_addr == 0) {
							//		vpu_error(vpu_drv_ctx->drv_context.vpudev, "fh2m_inno_ioremap_nocache failed phys_addr:0x%llx size:%d\n", output_buffer_vb->vb.phys_addr, output_buffer_vb->vb.size);
							//	}
							//}
							fh2m_inno_memcpy(&(p_cmd_message->cmd_internal_t.async_cmd.output_vb), output_buffer_vb, sizeof(vpu_buf_obj_t));
						} else {
							ret = vpu_dmafd_to_vb(vpu_drv_ctx, async_cmd.fd[i].buffer_fd, &(p_cmd_message->cmd_internal_t.async_cmd.output_vb.vb));
							if (ret) {
								vpu_error(vpu_drv_ctx->drv_context.vpudev, "vpu_dmafd_to_vb failed. output_fd:%d\n", async_cmd.fd[i].buffer_fd);
							}
						}
						p_cmd_message->cmd_internal_t.async_cmd.output_vb.buffer_type = async_cmd.fd[i].buffer_type;
					}
					p_cmd_message->cmd_internal_t.async_cmd.dma_buf_info[i].p_dma_buf = dma_buf_get(async_cmd.fd[i].buffer_fd);/* pair match with dma_buf_put */
					if (!async_cmd.fd[i].skip_fence_track) {
						if (p_cmd_message->cmd_internal_t.async_cmd.dma_buf_info[i].p_dma_buf) {
							vpu_dma_fence_create(-1, false, p_cmd_message->cmd_internal_t.async_cmd.dma_buf_info[i].p_dma_buf, &p_cmd_message->cmd_internal_t.async_cmd.dma_buf_info[i].p_dma_fence);
						} else {
							vpu_error(vpu_drv_ctx->drv_context.vpudev, "buffer_fd:%d i:%d is not dma_buf\n", async_cmd.fd[i].buffer_fd, i);
						}
					}
					p_cmd_message->cmd_internal_t.async_cmd.dma_buf_info[i].buffer_info = async_cmd.fd[i];
				}

				/*Create result buffer fence*/
				if (async_cmd.result_fd >= 0) {
					vpu_buf_obj_t* result_buffer_vb = NULL;
					p_cmd_message->cmd_internal_t.async_cmd.result_cmd_size = async_cmd.result_cmd_size;
					p_cmd_message->cmd_internal_t.async_cmd.p_result_buf = dma_buf_get(async_cmd.result_fd);/* pair match with dma_buf_put */
					if (p_cmd_message->cmd_internal_t.async_cmd.p_result_buf) {
						vpu_dma_fence_create(-1, false, p_cmd_message->cmd_internal_t.async_cmd.p_result_buf, &p_cmd_message->cmd_internal_t.async_cmd.p_result_buf_fence);
					} else {
						vpu_error(vpu_drv_ctx->drv_context.vpudev, "result_fd:%d is not dma_buf\n", async_cmd.result_fd);
					}
					result_buffer_vb = vpu_get_vb_from_fd(filp, async_cmd.result_fd);
					if (result_buffer_vb) {
						if(result_buffer_vb->map_addr == 0 && result_buffer_vb->vb.domain != INNO_VPU_GEM_DOMAIN_VRAM) { //reduce ioremap and iounmap
							result_buffer_vb->map_addr = (uint64_t)fh2m_inno_ioremap_nocache(result_buffer_vb->vb.phys_addr, result_buffer_vb->vb.size);
							if(result_buffer_vb->map_addr == 0) {
								vpu_error(vpu_drv_ctx->drv_context.vpudev, "fh2m_inno_ioremap_nocache failed phys_addr:0x%llx size:%d\n", result_buffer_vb->vb.phys_addr, result_buffer_vb->vb.size);
							}
						}
						fh2m_inno_memcpy(&(p_cmd_message->cmd_internal_t.async_cmd.result_vb), result_buffer_vb, sizeof(vpu_buf_obj_t));
					} else {
						ret = vpu_dmafd_to_vb(vpu_drv_ctx, async_cmd.result_fd, &(p_cmd_message->cmd_internal_t.async_cmd.result_vb.vb));
						if (ret) {
							vpu_error(vpu_drv_ctx->drv_context.vpudev, "vpu_dmafd_to_vb failed. result_fd:%d\n", async_cmd.result_fd);
						}
					}
				} else {
				   p_cmd_message->cmd_internal_t.async_cmd.p_result_buf = NULL;
				}

				if (async_cmd.time_cost_fd >= 0) {
					vpu_buf_obj_t* time_buffer_vb = NULL;
					p_cmd_message->cmd_internal_t.async_cmd.p_time_cost = dma_buf_get(async_cmd.time_cost_fd);/* pair match with dma_buf_put */
					if (!p_cmd_message->cmd_internal_t.async_cmd.p_time_cost) {
						vpu_error(vpu_drv_ctx->drv_context.vpudev, "result_fd:%d is not dma_buf\n", async_cmd.result_fd);
					}
					time_buffer_vb = vpu_get_vb_from_fd(filp, async_cmd.time_cost_fd);
					if (time_buffer_vb) {
						if(time_buffer_vb->map_addr == 0) { //reduce ioremap and iounmap
							time_buffer_vb->map_addr = (uint64_t)fh2m_inno_ioremap_nocache(time_buffer_vb->vb.phys_addr, time_buffer_vb->vb.size);
							if(time_buffer_vb->map_addr == 0) {
								vpu_error(vpu_drv_ctx->drv_context.vpudev, "fh2m_inno_ioremap_nocache failed phys_addr:0x%llx size:%d\n", time_buffer_vb->vb.phys_addr, time_buffer_vb->vb.size);
							}
						}
						fh2m_inno_memcpy(&(p_cmd_message->cmd_internal_t.async_cmd.time_cost_vb), time_buffer_vb, sizeof(vpu_buf_obj_t));
					}
				}

				TRACE_PRINTK("vpu%u idx %u proc %u type %d async_e\n", vpu_drv_ctx->drv_context.vpu_id, instance_id,
					vpu_drv_ctx->drv_context.proc_num[instance_id], async_cmd.cmd_type);

				fh2m_inno_kfifo_in_spinlocked(vpu_drv_ctx->drv_context.vpu_fence_msg[instance_id].instance_fence_processed_q, &p_cmd_message, sizeof(vpu_cmd_message_t *), vpu_drv_ctx->drv_context.vpu_fence_msg[instance_id].instance_fence_processed_q_lock);
				fh2m_inno_atomic64_add(1, &(vpu_drv_ctx->drv_context.vpu_fence_msg[instance_id].vpu_instance_fence_counter));
				fh2m_inno_wake_up_interruptible(vpu_drv_ctx->drv_context.vpu_fence_msg[instance_id].vpu_fence_thread_wq);

				vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "[-]VDI_IOCTL_SEND_ASYNC_COMMAND\n");
			}
		#endif
			break;
		case VDI_IOCTL_CV_OPEN:
			if (fh2m_hal_dev_disable_irq(vpu_drv_ctx->drv_context.parent, vpu_drv_ctx->drv_context.irq_handler_num)) {
				vpu_error(vpu_drv_ctx->drv_context.dev, "fh2m_hal_dev_disable_irq failed (ret=%d)\n", ret);
				return -EFAULT;
			}

			break;
		case VDI_IOCTL_CV_CLOSE:
			if (fh2m_hal_dev_enable_irq(vpu_drv_ctx->drv_context.parent, vpu_drv_ctx->drv_context.irq_handler_num)) {
				vpu_error(vpu_drv_ctx->drv_context.dev, "fh2m_hal_dev_enable_irq failed (ret=%d)\n", ret);
				return -EFAULT;
			}

			break;
		case VDI_IOCTL_GET_WORKLOAD: {
			vpu_workload_info_t work_load_info;
			vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "[+]VDI_IOCTL_GET_WORKLOAD\n");
			work_load_info.running_instance = vpu_drv_ctx->drv_context.vpu_open_ref_count;
			work_load_info.product_code = vpu_drv_ctx->drv_context.product_code;
			work_load_info.workload = fh2m_inno_atomic64_read(&(vpu_drv_ctx->drv_context.statistic_load));

			if (copy_to_user((vpu_workload_info_t *)arg, &work_load_info, sizeof(vpu_workload_info_t))) {
				return -EFAULT;
			}
			break;
		}
		case VDI_IOCTL_GET_MEMINFO: {
			struct vram_stats visible_stats = {0};
			struct vram_stats invisible_stats = {0};
			struct role_target role = {0};
			vpu_meminfo_t mem_info = {0};

			vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "[+]VDI_IOCTL_GET_MEMINFO\n");

			fh2m_inno_spin_lock(vpu_drv_ctx->drv_context.vpu_lock);
			role.vram_role = HAL_VRAM_ROLE_VPU;
			role.id = vpu_drv_ctx->drv_context.vpu_id;
			role.sub_id = 0;
			if (fh2m_hal_get_vram_stats(vpu_drv_ctx->drv_context.parent, &role, true, &visible_stats) != 0) {
				vpu_error(vpu_drv_ctx->drv_context.dev, "fh2m_hal_get_vram_stats failed\n");
				fh2m_inno_spin_unlock(vpu_drv_ctx->drv_context.vpu_lock);
				return -EINVAL;
			}

			if (vpu_drv_ctx->drv_context.chip_info.bar_size & INNO_GPU_BAR_SIZE_SMALL) {
				if (fh2m_hal_get_vram_stats(vpu_drv_ctx->drv_context.parent, &role, false, &invisible_stats) != 0) {
					vpu_error(vpu_drv_ctx->drv_context.dev, "fh2m_hal_get_vram_stats failed\n");
					fh2m_inno_spin_unlock(vpu_drv_ctx->drv_context.vpu_lock);
					return -EINVAL;
				}
				mem_info.invisiblemem_free= invisible_stats.free_size/1024/1024;
				mem_info.is_smallbar= true;
			}
			mem_info.visiblemem_free= visible_stats.free_size/1024/1024;
			fh2m_inno_spin_unlock(vpu_drv_ctx->drv_context.vpu_lock);

			if (copy_to_user((vpu_meminfo_t *)arg, &mem_info, sizeof(vpu_meminfo_t))) {
				return -EFAULT;
			}

			break;
		}
		case VDI_IOCTL_GET_BUSINFO: {
			unsigned int busnumer = 0;

			struct dev_rsrc *pdev_rsrc = fh2m_inno_rsrc_devres_find(vpu_drv_ctx->drv_context.parent);
			if (!pdev_rsrc) {
				vpu_prerr("%s: pdev_rsrc is NULL \n", __func__);
				return -EFAULT;
			}

			busnumer = ((struct pci_dev*)pdev_rsrc->pdev)->bus->number;
			if (copy_to_user((unsigned int *)arg, &busnumer, sizeof(unsigned int))) {
				return -EFAULT;
			}
			break;
		}

		case VDI_IOCTL_CONFIG_SOURCE_BUFFER: {
			vpu_source_buffer_info_t buffer_info;
			int instance_id;
			if (copy_from_user(&buffer_info, (vpu_source_buffer_info_t *)arg, sizeof(vpu_source_buffer_info_t))) {
				vpu_error(vpu_drv_ctx->drv_context.vpudev, "copy_from_user failed.\n");
				return -EFAULT;
			}
			instance_id = vpu_get_instance_id(filp);
			if (instance_id < 0) {
				vpu_error(vpu_drv_ctx->drv_context.vpudev, "invalid instance id=%d, need open instance first\n", instance_id);
				return -EFAULT;
			}
			vpu_add_source_buffer(&(vpu_drv_ctx->drv_context), &buffer_info, instance_id);
			break;
		}
		default:
			{
				vpu_error(vpu_drv_ctx->drv_context.vpudev, " No such IOCTL, cmd is %d\n", cmd);
			}
			break;
	}

	return ret;
}

static ssize_t vpu_read(struct file *filp, char __user *buf, size_t len, loff_t *ppos)
{
	return -1;
}

static ssize_t vpu_write(struct file *filp, const char __user *buf, size_t len, loff_t *ppos)
{

	vpu_drv_ctxs *vpu_drv_ctx = vpu_get_drv_ctx(filp);

	/* vpu_dbg(vpu_drv_ctx->drv_context.vpudev, " vpu_write len=%d\n", (u32)len); */
	if (!buf) {
		vpu_error(vpu_drv_ctx->drv_context.vpudev, " vpu_write buf = NULL error \n");
		return -EFAULT;
	}

	if (len == sizeof(vpu_bit_firmware_info_t))	{
		vpu_bit_firmware_info_t *bit_firmware_info;

		bit_firmware_info = fh2m_inno_kmalloc_kernel(sizeof(vpu_bit_firmware_info_t));
		if (!bit_firmware_info) {
			vpu_error(vpu_drv_ctx->drv_context.vpudev, " vpu_write  bit_firmware_info allocation error \n");
			return -EFAULT;
		}

		if (fh2m_inno_copy_from_user(bit_firmware_info, buf, len)) {
			vpu_error(vpu_drv_ctx->drv_context.vpudev, " vpu_write fh2m_inno_copy_from_user error for bit_firmware_info\n");
			kfree(bit_firmware_info);
			return -EFAULT;
		}

		if (bit_firmware_info->size == sizeof(vpu_bit_firmware_info_t)) {
			vpu_dbg(vpu_drv_ctx->drv_context.vpudev, " vpu_write set bit_firmware_info coreIdx=0x%x, reg_base_offset=0x%x size=0x%x, bit_code[0]=0x%x\n",
					bit_firmware_info->core_idx, (u32)bit_firmware_info->reg_base_offset, bit_firmware_info->size,
					bit_firmware_info->bit_code[0]);

			if (bit_firmware_info->core_idx > MAX_NUM_VPU_CORE) {
				vpu_error(vpu_drv_ctx->drv_context.vpudev, " vpu_write coreIdx[%d] is exceeded than MAX_NUM_VPU_CORE[%d]\n",
					bit_firmware_info->core_idx, MAX_NUM_VPU_CORE);
				return -ENODEV;
			}
			fh2m_inno_memcpy(&(vpu_drv_ctx->drv_context.bit_firmware_info[bit_firmware_info->core_idx]), bit_firmware_info, sizeof(vpu_bit_firmware_info_t));
			fh2m_inno_kfree(bit_firmware_info);
			return len;
		}
		fh2m_inno_kfree(bit_firmware_info);
	}

	return -1;
}

static int vpu_release(struct inode *inode, struct file *filp)
{
#ifdef SUPPORT_MULTI_INST_INTR
	u32 i;
#endif
	vpu_drv_ctxs *vpu_drv_ctx = vpu_get_drv_ctx(filp);
	vpu_dbg(vpu_drv_ctx->drv_context.vpudev, " vpu_release\n");

	vpu_free_lock(&(vpu_drv_ctx->drv_context), filp);
	/* found and free the not closed instance by user applications */
	vpu_free_instances(&(vpu_drv_ctx->drv_context), filp);
	/* found and free the not handled buffer by user applications */
	vpu_free_buffers(vpu_drv_ctx, filp);

	fh2m_inno_spin_lock(vpu_drv_ctx->drv_context.vpu_lock);
	vpu_drv_ctx->drv_context.vpu_context.open_count--;
	if (vpu_drv_ctx->drv_context.vpu_context.open_count == 0) {
#ifdef SUPPORT_MULTI_INST_INTR
		for (i = 0; i < VPU_MAX_NUM_INSTANCE; i++) {
			fh2m_inno_kfifo_reset(vpu_drv_ctx->drv_context.interrupt_pending_q[i]);
		}
#endif
		/*
		   Don't release instance_pool.base here to simply pm notifier.
		   This buffer will be released @ innovpu_remove.
		 */
	}
	fh2m_inno_spin_unlock(vpu_drv_ctx->drv_context.vpu_lock);
	vpu_dbg(vpu_drv_ctx->drv_context.vpudev, " vpu_release done\n");
	return 0;
}

static int vpu_fasync(int fd, struct file *filp, int mode)
{
	vpu_drv_ctxs *vpu_drv_ctx = vpu_get_drv_ctx(filp);
	struct vpu_context_t *dev = &(vpu_drv_ctx->drv_context.vpu_context);
	return fasync_helper(fd, filp, mode, (struct fasync_struct **)&dev->async_queue);
}

static int vpu_map_to_register(struct file *fp, struct vm_area_struct *vm)
{
	u64 pfn;
	vpu_drv_ctxs *vpu_drv_ctx = vpu_get_drv_ctx(fp);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 3, 0))
	vm->vm_flags |= VM_IO | VM_RESERVED;
#else
	vm_flags_set(vm, vm->vm_flags | VM_IO | VM_RESERVED);
#endif
	vm->vm_page_prot = pgprot_noncached(vm->vm_page_prot);
	pfn =  vpu_drv_ctx->drv_context.vpu_register.phys_addr >> PAGE_SHIFT;

	return remap_pfn_range(vm, vm->vm_start, pfn, vm->vm_end-vm->vm_start, vm->vm_page_prot) ? -EAGAIN : 0;
}

static int vpu_map_to_physical_memory(struct file *fp, struct vm_area_struct *vm)
{
	vpudrv_buffer_pool_t *vbp = NULL, *n = NULL;
	u64 pfn = vm->vm_pgoff;
	vpu_drv_ctxs *vpu_drv_ctx = vpu_get_drv_ctx(fp);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 3, 0))
	vm->vm_flags |= VM_IO | VM_RESERVED;
#else
	vm_flags_set(vm, vm->vm_flags | VM_IO | VM_RESERVED);
#endif
#if defined(CONFIG_ARM) || defined(CONFIG_ARM64)
	fh2m_inno_pgprot_writecombine(&vm->vm_page_prot, &vm->vm_page_prot);
#else
	fh2m_inno_pgprot_noncached(&vm->vm_page_prot, &vm->vm_page_prot);
#endif

  	//TODO Update after having a good optimization plan
	if ((vm->vm_pgoff & VPU_FOUR_BYTES_ANDOPERATION) == ((vpu_drv_ctx->drv_context.common_memory.phys_addr >> PAGE_SHIFT) & VPU_FOUR_BYTES_ANDOPERATION))
	{
		pfn = vpu_drv_ctx->drv_context.common_memory.phys_addr >> PAGE_SHIFT;
	} else {
		fh2m_inno_spin_lock(vpu_drv_ctx->drv_context.vpu_lock);
		list_for_each_entry_safe(vbp, n, &vpu_drv_ctx->drv_context.vbp_head, list)
		{
			if ((vm->vm_pgoff & VPU_FOUR_BYTES_ANDOPERATION) == ((vbp->buf.vb.phys_addr >> PAGE_SHIFT) & VPU_FOUR_BYTES_ANDOPERATION))
			{
				pfn = vbp->buf.vb.phys_addr >> PAGE_SHIFT;
				break;
			}
		}
		fh2m_inno_spin_unlock(vpu_drv_ctx->drv_context.vpu_lock);
	}
	return remap_pfn_range(vm, vm->vm_start, pfn, vm->vm_end-vm->vm_start, vm->vm_page_prot) ? -EAGAIN : 0;
}

static int vpu_map_to_instance_pool_memory(struct file *fp, struct vm_area_struct *vm)
{
	int ret;
	long long length = vm->vm_end - vm->vm_start;
	u64 start = vm->vm_start;
	vpu_drv_ctxs *vpu_drv_ctx = vpu_get_drv_ctx(fp);
	char *vmalloc_area_ptr = (char *)vpu_drv_ctx->drv_context.instance_pool.base;
	u64 pfn;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 3, 0))
	vm->vm_flags |= VM_RESERVED;
#else
	vm_flags_set(vm, vm->vm_flags | VM_RESERVED);
#endif

	/* loop over all pages, map it page individually */
	while (length > 0)
	{
		pfn = vmalloc_to_pfn(vmalloc_area_ptr);
		if ((ret = remap_pfn_range(vm, start, pfn, PAGE_SIZE, PAGE_SHARED)) < 0) {
			return ret;
		}
		start += PAGE_SIZE;
		vmalloc_area_ptr += PAGE_SIZE;
		length -= PAGE_SIZE;
	}

	return 0;
}

/*!
 * @brief memory map interface for vpu file operation
 * @return  0 on success or negative error code on error
 */
static int vpu_mmap(struct file *fp, struct vm_area_struct *vm)
{
	vpu_drv_ctxs *vpu_drv_ctx = vpu_get_drv_ctx(fp);

	if (vm->vm_pgoff == 0)
		return vpu_map_to_instance_pool_memory(fp, vm);

  	//TODO Update after having a good optimization plan
	if ((vm->vm_pgoff & VPU_FOUR_BYTES_ANDOPERATION) == ((vpu_drv_ctx->drv_context.vpu_register.phys_addr >> PAGE_SHIFT) & VPU_FOUR_BYTES_ANDOPERATION))
		return vpu_map_to_register(fp, vm);

	return vpu_map_to_physical_memory(fp, vm);
}

static struct file_operations vpu_fops = {
	.owner = THIS_MODULE,
	.open = vpu_open,
	.read = vpu_read,
	.write = vpu_write,
	/*.ioctl = vpu_ioctl, // for kernel 2.6.9 of*/
	.unlocked_ioctl = vpu_ioctl,
	.compat_ioctl = vpu_ioctl,
	.release = vpu_release,
	.fasync = vpu_fasync,
	.mmap = vpu_mmap,
};

static char *size_to_string(u64 size, char* str, int str_len)
{
	if (size > 1024*1024) {
		snprintf(str, str_len, "%5llu M", size >> 20);
	} else if (size > 1024) {
		snprintf(str, str_len, "%5llu K", size >> 10);
	} else {
		snprintf(str, str_len, "%5llu B", size);
	}

	return str;
}

static ssize_t info_fops_read_file(struct file *file, char __user *user_buf,
					 size_t count, loff_t *ppos)
{
	char *buf;
	ssize_t ret;
	vpu_drv_info * vpu_info = NULL;
	int vpu_num = 0;
	int  i = 0;
	int offset = 0;
	int running_instance = 0;
	int visible_mem = 0;
	int invisible_mem = 0;
	int external_mem = 0;
	int total_visible_mem = 0;
	int total_invisible_mem = 0;
	int total_external_mem = 0;
	int vpu_load = 0;
	char tmp_buf[128];

	buf = (char *)__get_free_page(GFP_KERNEL);
	if (!buf) {
		return -ENOMEM;
	}
	vpu_info = (vpu_drv_info *)g_vpu_drv_info;
	vpu_num = fh2m_inno_atomic64_read(&vpu_info->vpu_num);

	offset += sprintf(buf + offset, "vpu version: %s \t\t build time: %s\n", g_vpu_version, GIT_COMPILE_DATA);
	offset += sprintf(buf + offset, "git branch : %s \t commit id : %s\n\n", GIT_BRANCH, GIT_COMMIT_ID);

	for (i = 0; i < vpu_num; i++) {
		visible_mem = fh2m_inno_atomic64_read(&vpu_info->vpu_ctxs[i]->drv_context.visible_mem);
		invisible_mem = fh2m_inno_atomic64_read(&vpu_info->vpu_ctxs[i]->drv_context.invisible_mem);
		external_mem = fh2m_inno_atomic64_read(&vpu_info->vpu_ctxs[i]->drv_context.external_mem);
		vpu_load = fh2m_inno_atomic64_read(&(vpu_info->vpu_ctxs[i]->drv_context.statistic_load));
		offset += sprintf(buf + offset, "%s instance: %2d workload %3d%%",
			vpu_info->vpu_ctxs[i]->drv_context.vpuname,
			vpu_info->vpu_ctxs[i]->drv_context.vpu_open_ref_count,
			vpu_load / 100 + ((vpu_load % 100 > 0) ? 1 :0));
		offset += sprintf(buf + offset, " memory: %s",
			size_to_string(visible_mem + invisible_mem + external_mem, tmp_buf, sizeof(tmp_buf) -1));
		offset += sprintf(buf + offset, " visible: %s", size_to_string(visible_mem, tmp_buf, sizeof(tmp_buf) -1));
		offset += sprintf(buf + offset, " invisible: %s", size_to_string(invisible_mem, tmp_buf, sizeof(tmp_buf) -1));
		offset += sprintf(buf + offset, " external: %s\n", size_to_string(external_mem, tmp_buf, sizeof(tmp_buf) -1));
		running_instance += vpu_info->vpu_ctxs[i]->drv_context.vpu_open_ref_count;
		total_visible_mem += visible_mem;
		total_invisible_mem += invisible_mem;
		total_external_mem += external_mem;
	}

	offset += sprintf(buf + offset, "\ntotal instance: %3d", running_instance);
	offset += sprintf(buf + offset, " memory: %s",
		size_to_string(total_visible_mem + total_invisible_mem + total_external_mem, tmp_buf, sizeof(tmp_buf) -1));
	offset += sprintf(buf + offset, " visible: %s", size_to_string(total_visible_mem, tmp_buf, sizeof(tmp_buf) -1));
	offset += sprintf(buf + offset, " invisible: %s", size_to_string(total_invisible_mem, tmp_buf, sizeof(tmp_buf) -1));
	offset += sprintf(buf + offset, " external: %s\n", size_to_string(total_external_mem, tmp_buf, sizeof(tmp_buf) -1));

	ret = simple_read_from_buffer(user_buf, count, ppos, buf, strlen(buf));

	free_page((unsigned long)buf);

	return ret;
}

static ssize_t mem_fops_read_file(struct file *file, char __user *user_buf,
					 size_t count, loff_t *ppos)
{
	char *buf;
	ssize_t ret;
	vpu_drv_info * vpu_info = NULL;
	int vpu_num = 0;
	int  i = 0;
	int j = 0;
	int offset = 0;
	int running_instance = 0;
	int visible_mem = 0;
	int invisible_mem = 0;
	int external_mem = 0;
	int total_visible_mem = 0;
	int total_invisible_mem = 0;
	int total_external_mem = 0;
	int buf_size = 512 * 1024;
	char tmp_buf[128];
	int limit_size = buf_size - 4 * 1024;
	vpudrv_buffer_pool_t *vbp, *n;
	vpudrv_buffer_t *vb = NULL;

	char buf_type_string[BUF_TYPE_VPU_MAX][10] = {"malloc", "export", "import"};

	buf = fh2m_inno_vmalloc(buf_size);
	if (!buf) {
		return -ENOMEM;
	}

	vpu_info = (vpu_drv_info *)g_vpu_drv_info;
	vpu_num = fh2m_inno_atomic64_read(&vpu_info->vpu_num);


	offset += sprintf(buf + offset, "vpu version: %s \t\t build time: %s\n", g_vpu_version, GIT_COMPILE_DATA);
	offset += sprintf(buf + offset, "git branch : %s \t commit id : %s\n\n", GIT_BRANCH, GIT_COMMIT_ID);

	for (i = 0; i < vpu_num; i++) {
		j = 0;
		visible_mem = fh2m_inno_atomic64_read(&vpu_info->vpu_ctxs[i]->drv_context.visible_mem);
		invisible_mem = fh2m_inno_atomic64_read(&vpu_info->vpu_ctxs[i]->drv_context.invisible_mem);
		external_mem = fh2m_inno_atomic64_read(&vpu_info->vpu_ctxs[i]->drv_context.external_mem);
		offset += sprintf(buf + offset, "%s instance: %2d",
			vpu_info->vpu_ctxs[i]->drv_context.vpuname,
			vpu_info->vpu_ctxs[i]->drv_context.vpu_open_ref_count);
		offset += sprintf(buf + offset, " memory: %s",
			size_to_string(visible_mem + invisible_mem + external_mem, tmp_buf, sizeof(tmp_buf) -1));
		offset += sprintf(buf + offset, " visible: %s", size_to_string(visible_mem, tmp_buf, sizeof(tmp_buf) -1));
		offset += sprintf(buf + offset, " invisible: %s", size_to_string(invisible_mem, tmp_buf, sizeof(tmp_buf) -1));
		offset += sprintf(buf + offset, " external: %s\n", size_to_string(external_mem, tmp_buf, sizeof(tmp_buf) -1));

		running_instance += vpu_info->vpu_ctxs[i]->drv_context.vpu_open_ref_count;
		total_visible_mem += visible_mem;
		total_invisible_mem += invisible_mem;
		total_external_mem += external_mem;

		spin_lock(vpu_info->vpu_ctxs[i]->drv_context.vpu_lock);
		list_for_each_entry_safe(vbp, n, &vpu_info->vpu_ctxs[i]->drv_context.vbp_head, list) {
			j++;
			vb = &vbp->buf.vb;
			offset += sprintf(buf + offset, "%2d size: %8d dev: 0x%llx phy: 0x%llx fd: %3d type: %s %s\n",
				j, vb->size, vb->dev_addr, vb->phys_addr, vb->fd, buf_type_string[vbp->buf.obj.buf_type],
				vb->domain == INNO_VPU_GEM_DOMAIN_CPU ? "visible": "invisible");
			if (offset > limit_size) {
				break;
			}
		}
		spin_unlock(vpu_info->vpu_ctxs[i]->drv_context.vpu_lock);
		offset += sprintf(buf + offset, "\n");
	}

	offset += sprintf(buf + offset, "\ntotal instance: %3d", running_instance);
	offset += sprintf(buf + offset, " memory: %s",
		size_to_string(total_visible_mem + total_invisible_mem + total_external_mem, tmp_buf, sizeof(tmp_buf) -1));
	offset += sprintf(buf + offset, " visible: %s", size_to_string(total_visible_mem, tmp_buf, sizeof(tmp_buf) -1));
	offset += sprintf(buf + offset, " invisible: %s", size_to_string(total_invisible_mem, tmp_buf, sizeof(tmp_buf) -1));
	offset += sprintf(buf + offset, " external: %s\n", size_to_string(total_external_mem, tmp_buf, sizeof(tmp_buf) -1));

	ret = simple_read_from_buffer(user_buf, count, ppos, buf, strlen(buf));
	fh2m_inno_vfree(buf);

	return ret;
}
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,6,0)) || defined(CONFIG_DEBUG_FS)
static int debug_fops_read_file(void *data, u64 *val) {
	*val = g_vpu_debug_level;
	return 0;
}

static int debug_fops_write_file(void *data, u64 val) {
	g_vpu_debug_level = val;
	return 0;
}
#endif
static ssize_t vpu_read_file(struct file *file, char __user *user_buf,
					 size_t count, loff_t *ppos)
{
	ssize_t ret = 0;
	u64 visible_mem = 0;
	u64 invisible_mem = 0;
	u64 external_mem = 0;
	u64 vpu_load = 0;
	char *buf;
	char tmp_buf[32];
#if defined(CONFIG_DEBUG_FS)
	vpu_drv_ctxs *vpu_drv_ctx = (vpu_drv_ctxs *)file->private_data;
#elif defined(CONFIG_PROC_FS)
	vpu_drv_ctxs *vpu_drv_ctx = (vpu_drv_ctxs *)fh2m_inno_get_dfs_file(file_inode(file));
#endif
	struct vram_stats visible_stats;
	struct vram_stats invisible_stats;
	struct role_target role;

	if (!vpu_drv_ctx)
		return -EINVAL;

	fh2m_inno_memset(tmp_buf, 0, 32);
	fh2m_inno_memset(&visible_stats, 0, sizeof(struct vram_stats));
	fh2m_inno_memset(&invisible_stats, 0, sizeof(struct vram_stats));
	fh2m_inno_memset(&role, 0, sizeof(struct role_target));

	visible_mem = fh2m_inno_atomic64_read(&(vpu_drv_ctx->drv_context.visible_mem));
	invisible_mem = fh2m_inno_atomic64_read(&(vpu_drv_ctx->drv_context.invisible_mem));
	external_mem = fh2m_inno_atomic64_read(&(vpu_drv_ctx->drv_context.external_mem));
	vpu_load = fh2m_inno_atomic64_read(&(vpu_drv_ctx->drv_context.statistic_load));

	role.vram_role = HAL_VRAM_ROLE_VPU;
	role.id = vpu_drv_ctx->drv_context.vpu_id;
	role.sub_id = 0;
	if (fh2m_hal_get_vram_stats(vpu_drv_ctx->drv_context.parent, &role, true, &visible_stats) != 0) {
		vpu_error(vpu_drv_ctx->drv_context.dev, "fh2m_hal_get_vram_stats failed\n");
		return -EINVAL;
	}

	if (vpu_drv_ctx->drv_context.chip_info.bar_size & INNO_GPU_BAR_SIZE_SMALL) {
		if (fh2m_hal_get_vram_stats(vpu_drv_ctx->drv_context.parent, &role, false, &invisible_stats) != 0) {
			vpu_error(vpu_drv_ctx->drv_context.dev, "fh2m_hal_get_vram_stats failed\n");
			return -EINVAL;
		}
	}

	buf = (char *)__get_free_page(GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	ret += sprintf(buf, "RunInstanceNum: %d\n", vpu_drv_ctx->drv_context.vpu_open_ref_count);
	ret += sprintf(buf + ret, "VpuLoadUsage: %llu%%\n", vpu_load / 100 + ((vpu_load % 100 > 0) ? 1 :0));
	size_to_string(visible_mem, tmp_buf, sizeof(tmp_buf));
	ret += sprintf(buf + ret, "VisibleMem: %s\n", tmp_buf);
	size_to_string(visible_stats.total_size, tmp_buf, sizeof(tmp_buf));
	ret += sprintf(buf + ret, "VisibleTotalMem: %s\n", tmp_buf);
	size_to_string(visible_stats.free_size, tmp_buf, sizeof(tmp_buf));
	ret += sprintf(buf + ret, "VisibleTotalFreeMem: %s\n", tmp_buf);
	if (visible_stats.total_size > 0) {
		ret += sprintf(buf + ret, "VisibleTotalUsage: %llu%%\n", (visible_stats.total_size - visible_stats.free_size) * 100 / visible_stats.total_size);
	}

	size_to_string(invisible_mem, tmp_buf, sizeof(tmp_buf));
	ret += sprintf(buf + ret, "InvisibleMem: %s\n", tmp_buf);
	size_to_string(invisible_stats.total_size, tmp_buf, sizeof(tmp_buf));
	ret += sprintf(buf + ret, "InvisibleTotalMem: %s\n", tmp_buf);
	size_to_string(invisible_stats.free_size, tmp_buf, sizeof(tmp_buf));
	ret += sprintf(buf + ret, "InvisibleTotalFreeMem: %s\n", tmp_buf);
	if (invisible_stats.total_size > 0) {
		ret += sprintf(buf + ret, "InvisibleTotalUsage: %llu%%\n", (invisible_stats.total_size - invisible_stats.free_size) * 100 / invisible_stats.total_size);
	}

	size_to_string(external_mem, tmp_buf, sizeof(tmp_buf));
	ret += sprintf(buf + ret, "ImportMem: %s\n", tmp_buf);
	ret = simple_read_from_buffer(user_buf, count, ppos, buf, strlen(buf));

	free_page((unsigned long)buf);
	return ret;
}

static int vpu_irq[] = {HAL_INTERRUPT_CODEC_0,
						HAL_INTERRUPT_CODEC_1,
						HAL_INTERRUPT_CODEC_2,
						HAL_INTERRUPT_CODEC_3,
						HAL_INTERRUPT_CODEC_4,
						HAL_INTERRUPT_CODEC_5};

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,6,0)) || defined(CONFIG_DEBUG_FS)
static const struct file_operations status_debugfs_fops = {
	.open = simple_open,
	.read = vpu_read_file,
};

static const struct file_operations info_debugfs_fops = {
	.open = simple_open,
	.read = info_fops_read_file,
};

static const struct file_operations mem_debugfs_fops = {
	.open = simple_open,
	.read = mem_fops_read_file,
};

DEFINE_SIMPLE_ATTRIBUTE(debug_debugfs_fops, debug_fops_read_file, debug_fops_write_file, "%llu\n");

#else
static const struct proc_ops status_debugfs_fops = {
	.proc_open = simple_open,
	.proc_read = vpu_read_file,
};

static const struct proc_ops info_debugfs_fops = {
	.proc_open = simple_open,
	.proc_read = info_fops_read_file,
};

static const struct proc_ops mem_debugfs_fops = {
	.proc_open = simple_open,
	.proc_read = mem_fops_read_file,
};
#endif

static int vpu_create_debugfs(vpu_drv_ctxs *vpu_drv_ctx)
{
	/*create vpu debug sys*/
	vpu_drv_ctx->vpu_dir = fh2m_inno_debugfs_or_procfs_create_dir(vpu_drv_ctx->drv_context.vpuname, NULL);
	if (IS_ERR(vpu_drv_ctx->vpu_dir)) {
		vpu_error(vpu_drv_ctx->drv_context.dev, "failed to create vpu dir debugfs\n");
		goto ERR_CREATE_DIR;
	}

	vpu_drv_ctx->vpu_node = fh2m_inno_debugfs_or_procfs_create_file(vpu_drv_ctx, "vpu_status",
							0444,
							vpu_drv_ctx->vpu_dir,
							&status_debugfs_fops);
	if (IS_ERR(vpu_drv_ctx->vpu_node)) {
		vpu_error(vpu_drv_ctx->drv_context.dev, "failed to create vpu node debugfs\n");
		goto ERR_DEBUGFS_FILE;
	}

	return 0;

ERR_DEBUGFS_FILE:
	fh2m_inno_debugfs_or_procfs_remove_dir(vpu_drv_ctx->vpu_dir);

ERR_CREATE_DIR:
	return -1;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 2, 0)
static char *vpu_devnode(const struct device *dev, umode_t *mode)
#else
static char *vpu_devnode(struct device *dev, umode_t *mode)
#endif
{
	if (mode) {
		*mode = 0666;
	}

	return NULL;
}

static int vpu_create_device(vpu_drv_ctxs *vpu_drv_ctx)
{
	int ret = -1;
	struct device *vpu_device;

	if (!vpu_drv_ctx) {
		vpu_prerr("%s: vpu_drv_ctx is NULL \n", __func__);
		return -1;
	}

	//alloc cdev
	vpu_drv_ctx->cdevid = (dev_t *)fh2m_inno_kzalloc_kernel(sizeof(dev_t));
	if (!vpu_drv_ctx->cdevid) {
		vpu_prerr("%s: cdevid kzalloc failed\n", __func__);
		return -1;
	}

	ret = alloc_chrdev_region(vpu_drv_ctx->cdevid, 0, 1, vpu_drv_ctx->drv_context.vpuname);
	if (ret < 0) {
		vpu_error(vpu_drv_ctx->drv_context.dev, "alloc_chrdev_region failed(ret=%d)\n",ret);
		goto ERR_ALLOC_CDEV;
	}

	//init cdev
	vpu_drv_ctx->vpucdev.owner = THIS_MODULE;
	cdev_init(&vpu_drv_ctx->vpucdev, &vpu_fops);

	//register cdev
	ret = cdev_add(&vpu_drv_ctx->vpucdev, *vpu_drv_ctx->cdevid, 1);
	if (ret < 0) {
		vpu_error(vpu_drv_ctx->drv_context.dev, "cdev_add failed (ret=%d)\n",ret);
		goto ERR_ADD_CDEV;
	}

	//create class
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
	vpu_drv_ctx->vpucls = class_create(vpu_drv_ctx->drv_context.vpuname);
#else
	vpu_drv_ctx->vpucls = class_create(THIS_MODULE, vpu_drv_ctx->drv_context.vpuname);
#endif
	if (IS_ERR(vpu_drv_ctx->vpucls)) {
		vpu_error(vpu_drv_ctx->drv_context.dev, "class_create failed\n");
		goto ERR_CLASS_CREATE;
	}

	//set /dev/vpu* user mode 0666
	vpu_drv_ctx->vpucls->devnode = vpu_devnode;

	//create vpu device
	vpu_device = device_create(vpu_drv_ctx->vpucls, NULL, *vpu_drv_ctx->cdevid, NULL, vpu_drv_ctx->drv_context.vpuname);
	if (IS_ERR(vpu_device)) {
		vpu_error(vpu_drv_ctx->drv_context.dev, "device_create failed\n");
		goto ERR_DEVICE_CREATE;
	}
	vpu_drv_ctx->drv_context.vpudev = vpu_device;

	return 0;

ERR_DEVICE_CREATE:
	class_destroy(vpu_drv_ctx->vpucls);

ERR_CLASS_CREATE:
	cdev_del(&vpu_drv_ctx->vpucdev);

ERR_ADD_CDEV:
	unregister_chrdev_region(*vpu_drv_ctx->cdevid, 1);

ERR_ALLOC_CDEV:
	fh2m_inno_kfree(vpu_drv_ctx->cdevid);

	return -ENOMEM;
}

static void vpu_destroy_device(vpu_drv_ctxs *vpu_drv_ctx)
{
	if (!vpu_drv_ctx) {
		vpu_prerr("%s: vpu_drv_ctx is NULL \n", __func__);
		return;
	}

	device_destroy(vpu_drv_ctx->vpucls, *vpu_drv_ctx->cdevid);
	class_destroy(vpu_drv_ctx->vpucls);
	cdev_del(&vpu_drv_ctx->vpucdev);
	unregister_chrdev_region(*vpu_drv_ctx->cdevid, 1);
	fh2m_inno_kfree(vpu_drv_ctx->cdevid);
}

static void innovpu_handle_irq(void *data)
{
	vpu_drv_ctxs *vpu_drv_ctx = (vpu_drv_ctxs *)data;

	if (!vpu_drv_ctx) {
		vpu_prerr("%s: vpu_drv_ctx is NULL \n", __func__);
		return;
	}

	vpu_handle_irq(&(vpu_drv_ctx->drv_context));
}

static int vpu_unset_irq(vpu_drv_ctxs *vpu_drv_ctx)
{
	int ret = 0;

	if (!vpu_drv_ctx) {
		vpu_prerr("%s: vpu_drv_ctx is NULL \n", __func__);
		return -1;
	}

	ret = fh2m_hal_dev_disable_irq(vpu_drv_ctx->drv_context.parent, vpu_drv_ctx->drv_context.irq_handler_num);
	if (ret) {
		vpu_error(vpu_drv_ctx->drv_context.dev, "fh2m_hal_dev_disable_irq failed (ret=%d)\n", ret);
	}

	return ret;
}

static int vpu_set_irq(vpu_drv_ctxs *vpu_drv_ctx)
{
	int ret = 0;

	if (!vpu_drv_ctx) {
		vpu_prerr("%s: vpu_drv_ctx is NULL \n", __func__);
		return -1;
	}

	ret = fh2m_hal_set_irq_handler(vpu_drv_ctx->drv_context.parent, vpu_drv_ctx->drv_context.irq_handler_num, innovpu_handle_irq, vpu_drv_ctx);
	if (ret) {
		vpu_error(vpu_drv_ctx->drv_context.dev, "failed to set codec interrupt handler (ret=%d)\n", ret);
		return -1;
	}

	ret = fh2m_hal_dev_enable_irq(vpu_drv_ctx->drv_context.parent, vpu_drv_ctx->drv_context.irq_handler_num);
	if (ret) {
		vpu_error(vpu_drv_ctx->drv_context.dev, "failed to enable codec interrupts (ret=%d)\n", ret);
		return -1;
	}

	return 0;
}

static int vpu_get_hwinfo(vpu_drv_ctxs *vpu_drv_ctx)
{
	if (!vpu_drv_ctx) {
		vpu_prerr("%s: vpu_drv_ctx is NULL \n", __func__);
		return -1;
	}

	vpu_drv_ctx->drv_context.chip_info.chip_type = fh2m_hal_get_chiptype(vpu_drv_ctx->drv_context.parent);
	if (0 == vpu_drv_ctx->drv_context.chip_info.chip_type) {
		vpu_error(vpu_drv_ctx->drv_context.dev, "fh2m_hal_get_chiptype unknown\n");
		return -1;
	}

	vpu_drv_ctx->drv_context.chip_info.vpu_num = fh2m_hal_get_dev_nums(vpu_drv_ctx->drv_context.parent, DEV_VPU);
	vpu_drv_ctx->drv_context.chip_info.vf_id = fh2m_hal_get_vfid(vpu_drv_ctx->drv_context.parent);
	if (fh2m_hal_has_inv_mem(vpu_drv_ctx->drv_context.parent)) {
		vpu_drv_ctx->drv_context.chip_info.bar_size = INNO_GPU_BAR_SIZE_SMALL;
	} else {
		vpu_drv_ctx->drv_context.chip_info.bar_size = INNO_GPU_BAR_SIZE_4G;
	}

	vpu_drv_ctx->drv_context.irq_handler_num = vpu_irq[vpu_drv_ctx->drv_context.vpu_id];
	vpu_drv_ctx->drv_context.chip_info.host_bar2_base_addr = fh2m_hal_get_ddrbase(vpu_drv_ctx->drv_context.parent);
	vpu_drv_ctx->drv_context.chip_info.host_bar2_end_addr = vpu_drv_ctx->drv_context.chip_info.host_bar2_base_addr + fh2m_hal_get_ddr_bar_len(vpu_drv_ctx->drv_context.parent);
	vpu_drv_ctx->drv_context.chip_info.vram_dev_base_addr = fh2m_hal_get_vram_dev_base(vpu_drv_ctx->drv_context.parent);
	vpu_drv_ctx->drv_context.chip_info.vram_dev_end_addr = vpu_drv_ctx->drv_context.chip_info.vram_dev_base_addr + fh2m_hal_get_ddr_bar_len(vpu_drv_ctx->drv_context.parent);
	vpu_drv_ctx->drv_context.is_sharing_gpu_heap = fh2m_hal_is_sharing_gpu_heap(vpu_drv_ctx->drv_context.parent);
	/* 由于硬复位之后才能正常读取vpu寄存器值，但是硬复位需要product code会判断，所以根据芯片type手动赋值 */
	vpu_drv_ctx->drv_context.product_code = vpu_get_product_code_manual(&vpu_drv_ctx->drv_context);
	vpu_drv_ctx->drv_context.vpu_ppu_id = vpu_get_ppu_id(&vpu_drv_ctx->drv_context);
#ifdef USE_REFACTOR_LOGIC
	vpu_drv_ctx->drv_context.chip_info.vpu_id = vpu_drv_ctx->drv_context.vpu_id;
	vpu_drv_ctx->drv_context.chip_info.product_code = vpu_drv_ctx->drv_context.product_code;
#endif
	vpu_info(vpu_drv_ctx->drv_context.dev, "vpu%d product code=0x%x\n", vpu_drv_ctx->drv_context.vpu_id, vpu_drv_ctx->drv_context.product_code);

	return 0;
}

static void vpu_deinit_context(vpu_drv_ctxs *vpu_drv_ctx)
{
	int i;

	if (!vpu_drv_ctx) {
		vpu_prerr("%s: vpu_drv_ctx is NULL \n", __func__);
		return;
	}

	if (vpu_drv_ctx->drv_context.vpu_sem) {
		fh2m_inno_kfree(vpu_drv_ctx->drv_context.vpu_sem);
		vpu_drv_ctx->drv_context.vpu_sem = NULL;
	}

	if (vpu_drv_ctx->drv_context.enc_sem) {
		fh2m_inno_kfree(vpu_drv_ctx->drv_context.enc_sem);
		vpu_drv_ctx->drv_context.enc_sem = NULL;
	}

	if (vpu_drv_ctx->drv_context.vpu_lock) {
		fh2m_inno_spinlock_free(vpu_drv_ctx->drv_context.vpu_lock);
		vpu_drv_ctx->drv_context.vpu_lock = NULL;
	}

	if (vpu_drv_ctx->drv_context.kfifo_lock) {
		fh2m_inno_spinlock_free(vpu_drv_ctx->drv_context.kfifo_lock);
		vpu_drv_ctx->drv_context.kfifo_lock = NULL;
	}

	if (vpu_drv_ctx->drv_context.inst_index_lock) {
		fh2m_inno_spinlock_free(vpu_drv_ctx->drv_context.inst_index_lock);
		vpu_drv_ctx->drv_context.inst_index_lock = NULL;
	}

	if (vpu_drv_ctx->drv_context.async_cmd_processed_q_lock) {
		fh2m_inno_spinlock_free(vpu_drv_ctx->drv_context.async_cmd_processed_q_lock);
		vpu_drv_ctx->drv_context.async_cmd_processed_q_lock = NULL;
	}
	vpu_dbg(vpu_drv_ctx->drv_context.dev, "john: sync_cmd_q_lock async_cmd_q_lock async_cmd_processed_q_lock free");

	for (i = 0; i < VPU_MAX_NUM_INSTANCE; i++) {
		if (vpu_drv_ctx->drv_context.interrupt_pending_q[i]) {
			fh2m_inno_kfifo_free(vpu_drv_ctx->drv_context.interrupt_pending_q[i]);
			fh2m_inno_kfree(vpu_drv_ctx->drv_context.interrupt_pending_q[i]);
			vpu_drv_ctx->drv_context.interrupt_pending_q[i] = NULL;
		}

		if (vpu_drv_ctx->drv_context.interrupt_wait_q[i]) {
			fh2m_inno_kfree(vpu_drv_ctx->drv_context.interrupt_wait_q[i]);
			vpu_drv_ctx->drv_context.interrupt_wait_q[i] = NULL;
		}

		if (vpu_drv_ctx->drv_context.inst_lock_info_lock[i]) {
			fh2m_inno_spinlock_free(vpu_drv_ctx->drv_context.inst_lock_info_lock[i]);
			vpu_drv_ctx->drv_context.inst_lock_info_lock[i] = NULL;
		}

		if (vpu_drv_ctx->drv_context.inst_lock_wait_q[i]) {
			fh2m_inno_waitqueue_head_free(vpu_drv_ctx->drv_context.inst_lock_wait_q[i]);
			vpu_drv_ctx->drv_context.inst_lock_wait_q[i] = NULL;
		}

		if (vpu_drv_ctx->drv_context.cmd_message_lock[i]) {
			fh2m_inno_spinlock_free(vpu_drv_ctx->drv_context.cmd_message_lock[i]);
			vpu_drv_ctx->drv_context.cmd_message_lock[i] = NULL;
		}

		if (vpu_drv_ctx->drv_context.input_buffer_lock[i]) {
			fh2m_inno_spinlock_free(vpu_drv_ctx->drv_context.input_buffer_lock[i]);
			vpu_drv_ctx->drv_context.input_buffer_lock[i] = NULL;
		}

		if (vpu_drv_ctx->drv_context.source_buffer_lock[i]) {
			fh2m_inno_spinlock_free(vpu_drv_ctx->drv_context.source_buffer_lock[i]);
			vpu_drv_ctx->drv_context.source_buffer_lock[i] = NULL;
		}
	}

	if (vpu_drv_ctx->drv_context.async_cmd_processed_q) {
		fh2m_inno_kfifo_free(vpu_drv_ctx->drv_context.async_cmd_processed_q);
		fh2m_inno_kfree(vpu_drv_ctx->drv_context.async_cmd_processed_q);
		vpu_drv_ctx->drv_context.async_cmd_processed_q = NULL;
	}

	vpu_dbg(vpu_drv_ctx->drv_context.dev, "john: sync_cmd_q async_cmd_q async_cmd_processed_q free");
	if (vpu_drv_ctx->drv_context.vpu_exec_wq) {
		fh2m_inno_waitqueue_head_free(vpu_drv_ctx->drv_context.vpu_exec_wq);
		vpu_drv_ctx->drv_context.vpu_exec_wq = NULL;
	}
	vpu_dbg(vpu_drv_ctx->drv_context.dev, "john: vpu_exec_wq free");

	if (vpu_drv_ctx->drv_context.vpu_dpc_wq) {
		fh2m_inno_waitqueue_head_free(vpu_drv_ctx->drv_context.vpu_dpc_wq);
		vpu_drv_ctx->drv_context.vpu_dpc_wq = NULL;
	}

	if(vpu_drv_ctx->drv_context.mirror_buffer) {
		fh2m_inno_vfree(vpu_drv_ctx->drv_context.mirror_buffer);
		vpu_drv_ctx->drv_context.mirror_buffer = NULL;
	}
}

static int vpu_init_context(vpu_drv_ctxs *ctx)
{
	int i;
	int ret;
	uint32_t        mirror_buffer_size = 0;
	uint32_t        cmd_buffer_size = 0;
	struct dev_rsrc *pdev_rsrc = fh2m_inno_rsrc_devres_find(ctx->drv_context.parent);
	if (!ctx) {
		vpu_prerr("%s: vpu_drv_ctx is NULL \n", __func__);
		return -1;
	}
	fh2m_inno_atomic64_set(&(ctx->drv_context.invisible_mem), 0);
	fh2m_inno_atomic64_set(&(ctx->drv_context.visible_mem), 0);
	fh2m_inno_atomic64_set(&(ctx->drv_context.external_mem), 0);
	fh2m_inno_atomic64_set(&(ctx->drv_context.statistic_load), 0);
	fh2m_inno_atomic64_set(&(ctx->drv_context.tick_start), 0);
	fh2m_inno_atomic64_set(&(ctx->drv_context.tick_end), 0);
	fh2m_inno_atomic64_set(&(ctx->drv_context.cumulative_tick), 0);
	fh2m_inno_atomic64_set(&(ctx->drv_context.time_start), 0);
	fh2m_inno_atomic64_set(&(ctx->drv_context.time_end), 0);
	fh2m_inno_atomic64_set(&(ctx->drv_context.cumulative_time), 0);
	fh2m_inno_atomic64_set(&(ctx->drv_context.timer_start), 0);
	fh2m_inno_atomic64_set(&(pdev_rsrc->vpuinfo.vpu_usage[ctx->drv_context.vpu_id]), 0);
	fh2m_inno_atomic64_set(&(pdev_rsrc->vpuinfo.vpu_type[ctx->drv_context.vpu_id]), vpu_get_type(ctx->drv_context.product_code));
	fh2m_inno_atomic64_set(&(pdev_rsrc->vpuinfo.vpu_mem_total), 0);
	fh2m_inno_atomic64_set(&(pdev_rsrc->vpuinfo.vpu_mem_used), 0);
	fh2m_inno_atomic64_set(&(pdev_rsrc->vpuinfo.vpu_mem_free), 0);

	ctx->drv_context.common_memory.base = 0;
	ctx->drv_context.instance_pool.base = 0;
	ctx->drv_context.pvric_memory.base = 0;
	ctx->drv_context.kfifo_lock = fh2m_inno_spinlock_alloc();
	if (!ctx->drv_context.kfifo_lock) {
		vpu_error(ctx->drv_context.dev, "kfifo_lock fh2m_inno_spinlock_alloc failed\n");
		goto DEINIT;
	}
	ctx->drv_context.vpu_lock = fh2m_inno_spinlock_alloc();
	if (!ctx->drv_context.vpu_lock) {
		vpu_error(ctx->drv_context.dev, "vpu_lock fh2m_inno_spinlock_alloc failed\n");
		goto DEINIT;
	}
	ctx->drv_context.inst_index_lock = fh2m_inno_spinlock_alloc();
	if (!ctx->drv_context.inst_index_lock) {
		vpu_error(ctx->drv_context.dev, "inst_index_lock fh2m_inno_spinlock_alloc failed\n");
		goto DEINIT;
	}
	ctx->drv_context.vpu_sem = (inno_semaphore *)fh2m_inno_kzalloc_kernel(sizeof(struct semaphore));
	if (!ctx->drv_context.vpu_sem) {
		vpu_error(ctx->drv_context.dev, "vpu_sem kzalloc failed\n");
		goto DEINIT;
	} else {
		fh2m_inno_sema_init(ctx->drv_context.vpu_sem, 1);
	}
	ctx->drv_context.enc_sem = (inno_semaphore *)fh2m_inno_kzalloc_kernel(sizeof(struct semaphore));
	if (!ctx->drv_context.enc_sem) {
		vpu_error(ctx->drv_context.dev, "enc_sem kzalloc failed\n");
		goto DEINIT;
	} else {
		fh2m_inno_sema_init(ctx->drv_context.enc_sem, 1);
	}
	INIT_LIST_HEAD(&ctx->drv_context.vbp_head);
	INIT_LIST_HEAD(&ctx->drv_context.pm_buf_head);
	INIT_LIST_HEAD(&ctx->drv_context.inst_list_head);
	for (i = 0; i < VPU_MAX_NUM_INSTANCE; i++) {
		ctx->drv_context.interrupt_wait_q[i] = (inno_waitqueue_head *)fh2m_inno_kzalloc_kernel(sizeof(wait_queue_head_t));
		if (!ctx->drv_context.interrupt_wait_q[i]) {
			vpu_error(ctx->drv_context.dev, "interrupt_wait_q[i] kzalloc failed i:%d\n", i);
			goto DEINIT;
		} else {
			fh2m_inno_init_waitqueue_head(ctx->drv_context.interrupt_wait_q[i]);
		}
		ctx->drv_context.interrupt_pending_q[i] = (vpu_kfifo *)fh2m_inno_kzalloc_kernel(sizeof(struct kfifo));
		if (!ctx->drv_context.interrupt_pending_q[i]) {
			vpu_error(ctx->drv_context.dev, "interrupt_pending_q[i] kzalloc failed i:%d\n", i);
			goto DEINIT;
		}
		ret = fh2m_inno_kfifo_alloc(ctx->drv_context.interrupt_pending_q[i], MAX_INTERRUPT_QUEUE * sizeof(u32), GFP_KERNEL);
		if (ret) {
			vpu_error(ctx->drv_context.dev, " kfifo_alloc failed 0x%x\n", ret);
			goto DEINIT;
		}
		INIT_LIST_HEAD(&ctx->drv_context.cmd_message_head[i]);
		ctx->drv_context.cmd_message_lock[i] = fh2m_inno_spinlock_alloc();
		if (!ctx->drv_context.cmd_message_lock[i]) {
			vpu_error(ctx->drv_context.dev, "cmd_message_lock[i] fh2m_inno_spinlock_alloc failed i:%d\n", i);
			goto DEINIT;
		}
		INIT_LIST_HEAD(&ctx->drv_context.inst_lock_info_head[i]);
		ctx->drv_context.inst_lock_info_lock[i] = fh2m_inno_spinlock_alloc();
		if (!ctx->drv_context.inst_lock_info_lock[i]) {
			vpu_error(ctx->drv_context.dev, "inst_lock_info_lock[i] fh2m_inno_spinlock_alloc failed i:%d\n", i);
			goto DEINIT;
		}
		ctx->drv_context.inst_lock_wait_q[i] = fh2m_inno_waitqueue_head_alloc();
		if (!ctx->drv_context.inst_lock_wait_q[i]) {
			vpu_error(ctx->drv_context.dev, "inst_lock_wait_q[i] fh2m_inno_spinlock_alloc failed i:%d\n", i);
			goto DEINIT;
		} else {
			fh2m_inno_init_waitqueue_head(ctx->drv_context.inst_lock_wait_q[i]);
		}
		ctx->drv_context.inst_lock_wake_up_flag[i] = 0;
		INIT_LIST_HEAD(&ctx->drv_context.input_buffer_head[i]);
		ctx->drv_context.input_buffer_lock[i] = fh2m_inno_spinlock_alloc();
		if (!ctx->drv_context.input_buffer_lock[i]) {
			vpu_error(ctx->drv_context.dev, "input_buffer_lock[i] fh2m_inno_spinlock_alloc failed i:%d\n", i);
			goto DEINIT;
		}

		INIT_LIST_HEAD(&ctx->drv_context.source_buffer_head[i]);
		ctx->drv_context.source_buffer_lock[i] = fh2m_inno_spinlock_alloc();
		if (!ctx->drv_context.source_buffer_lock[i]) {
			vpu_error(ctx->drv_context.dev, "source_buffer_lock[i] fh2m_inno_spinlock_alloc failed i:%d\n", i);
			goto DEINIT;
		}
	}
	ctx->drv_context.async_cmd_processed_q = (vpu_kfifo *)fh2m_inno_kzalloc_kernel(sizeof(struct kfifo));
	ret = fh2m_inno_kfifo_alloc(ctx->drv_context.async_cmd_processed_q, MAX_INTERRUPT_QUEUE * sizeof(vpu_cmd_message_t*), GFP_KERNEL);
	if (ret) {
		vpu_error(ctx->drv_context.dev, "async_cmd_processed_q kfifo_alloc failed 0x%x\n", ret);
		goto DEINIT;
	}
	vpu_dbg(ctx->drv_context.dev, "john: ctx->async_cmd_processed_q = %px\n", ctx->drv_context.async_cmd_processed_q);
	ctx->drv_context.async_cmd_processed_q_lock = fh2m_inno_spinlock_alloc();
	if (!ctx->drv_context.async_cmd_processed_q_lock) {
		vpu_error(ctx->drv_context.dev, "async_cmd_processed_q_lock fh2m_inno_spinlock_alloc failed\n");
		goto DEINIT;
	}
	vpu_dbg(ctx->drv_context.dev, "john: ctx->async_cmd_processed_q_lock = %px\n", ctx->drv_context.async_cmd_processed_q_lock);
	ctx->drv_context.vpu_exec_wq = fh2m_inno_waitqueue_head_alloc();
	if (!ctx->drv_context.vpu_exec_wq) {
		vpu_error(ctx->drv_context.dev, "vpu_exec_wq fh2m_inno_waitqueue_head_alloc failed\n");
		goto DEINIT;
	} else {
		fh2m_inno_init_waitqueue_head(ctx->drv_context.vpu_exec_wq);
	}
	vpu_dbg(ctx->drv_context.dev, "john: ctx->vpu_exec_wq = %px\n", ctx->drv_context.vpu_exec_wq);
	ctx->drv_context.vpu_dpc_wq = fh2m_inno_waitqueue_head_alloc();
	if (!ctx->drv_context.vpu_dpc_wq) {
		vpu_error(ctx->drv_context.dev, "vpu_dpc_wq fh2m_inno_waitqueue_head_alloc failed\n");
		goto DEINIT;
	} else {
		fh2m_inno_init_waitqueue_head(ctx->drv_context.vpu_dpc_wq);
	}
	vpu_dbg(ctx->drv_context.dev, "john: ctx->vpu_dpc_wq = %px\n", ctx->drv_context.vpu_dpc_wq);
	vpu_get_cmd_mirror_buffer_size(&(ctx->drv_context), &cmd_buffer_size, &mirror_buffer_size);
	ctx->drv_context.mirror_buffer = (uint8_t *)fh2m_inno_vmalloc(mirror_buffer_size);
	if (!ctx->drv_context.mirror_buffer) {
		vpu_error(ctx->drv_context.dev, "mirror_buffer vmalloc failed size:%d*4\n", mirror_buffer_size);
		goto DEINIT;
	}

	return 0;
DEINIT:
	vpu_deinit_context(ctx);
	return -1;
}

#ifdef CONFIG_PM_SLEEP
static int vpu_notifier(struct notifier_block *nb, unsigned long action, void *data)
{
	vpu_drv_ctxs *ctx = container_of(nb, vpu_drv_ctxs, pm_notifier);

	int ret = 0;

	switch (action) {
	case PM_HIBERNATION_PREPARE:
	case PM_SUSPEND_PREPARE:
		vpu_prinfo("%s: prepare suspend time:%ld\n", __func__, fh2m_inno_clockmonotonic_raw());
		if (g_vpu_drv_info != NULL && g_vpu_drv_info->workload_timer != NULL &&
			g_vpu_drv_info->workload_timer->timer_suspend == false) {
			while (1) {
				ret = del_timer_sync(&(g_vpu_drv_info->workload_timer->timer));
				if (ret == 0) {
					break;
				}
				udelay(50);
			}
			g_vpu_drv_info->workload_timer->timer_suspend = true;
		}
#ifndef USE_REFACTOR_LOGIC
		if (ctx->drv_context.instance_pool.base != 0) {
			ret = vpu_mutex_lock(0, (u32)VPU_MAX_NUM_INSTANCE, VPUDRV_MUTEX_VPU, &ctx->drv_context);
			if (ret != 0) {
				vpu_error(ctx->drv_context.dev, "%s: vpu_mutex_lock failed preparing...\n", __func__);
			}
		}
#endif
		break;
	case PM_POST_HIBERNATION:
	case PM_POST_SUSPEND:
		vpu_prinfo("%s: prepare resume time:%ld\n", __func__, fh2m_inno_clockmonotonic_raw());
#ifndef USE_REFACTOR_LOGIC
		if (ctx->drv_context.instance_pool.base != 0) {
			vpu_mutex_unlock(0, (u32)VPU_MAX_NUM_INSTANCE, VPUDRV_MUTEX_VPU, &ctx->drv_context);
			vpu_error(ctx->drv_context.dev, "%s: vpu_mutex_unlock.\n", __func__);
		}
#endif

		if (g_vpu_drv_info != NULL && g_vpu_drv_info->workload_timer != NULL &&
			g_vpu_drv_info->workload_timer->timer_suspend == true) {
			g_vpu_drv_info->workload_timer->jiffies = jiffies + HZ;
			mod_timer(&(g_vpu_drv_info->workload_timer->timer), g_vpu_drv_info->workload_timer->jiffies);
			g_vpu_drv_info->workload_timer->timer_suspend = false;
		}

		break;
	case PM_POST_RESTORE:
	case PM_RESTORE_PREPARE:
	default:
		vpu_warn(ctx->drv_context.dev, "%s: untested action %d...\n", __func__, action);
		break;
	}

	return NOTIFY_DONE;
}
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0))
static void workload_timer_callback(unsigned long data) {
	vpu_timer_t *t =(vpu_timer_t *)data;
#else
static void workload_timer_callback(struct timer_list *timer) {
	vpu_timer_t *t =(vpu_timer_t *)timer;
#endif
	int i = 0;
	int total_visible_mem = 0;
	int total_invisible_mem = 0;
	int total_external_mem = 0;
	struct dev_rsrc *pdev_rsrc = NULL;

	if (t == NULL || t->drv_info == NULL || t->drv_info->workload_timer == NULL) {
		return;
	}

	pdev_rsrc = fh2m_inno_rsrc_devres_find(t->drv_info->vpu_ctxs[i]->drv_context.parent);

	t->jiffies = jiffies + HZ;
	mod_timer(&(t->timer), t->jiffies);

	for (i = 0; i < fh2m_inno_atomic64_read(&(t->drv_info->vpu_num)); i++) {
		if (t->drv_info->vpu_ctxs[i] != NULL) {
			innovpu_workload_update(&(t->drv_info->vpu_ctxs[i]->drv_context));
		}

		total_visible_mem += fh2m_inno_atomic64_read(&t->drv_info->vpu_ctxs[i]->drv_context.visible_mem);
		total_invisible_mem += fh2m_inno_atomic64_read(&t->drv_info->vpu_ctxs[i]->drv_context.invisible_mem);
		total_external_mem += fh2m_inno_atomic64_read(&t->drv_info->vpu_ctxs[i]->drv_context.external_mem);
	}

	fh2m_inno_atomic64_set(&(pdev_rsrc->vpuinfo.vpu_mem_used), total_visible_mem + total_invisible_mem + total_external_mem);
}

static int vpu_start_workload_timer(vpu_drv_info *drv_info) {
	vpu_timer_t *t;
	if (drv_info->workload_timer == NULL) {
		drv_info->workload_timer = kmalloc(sizeof(vpu_timer_t), GFP_KERNEL);
		t = drv_info->workload_timer;
		if (t == NULL) {
			vpu_prerr("workload_timer allocation error \n");
			return -ENOMEM;
		}
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0))
		setup_timer(&(t->timer), workload_timer_callback, (unsigned long)t);
#else
		timer_setup(&(t->timer), workload_timer_callback, 0);
#endif
		t->jiffies = jiffies + HZ;
		t->drv_info = drv_info;
		t->last_timestamp = fh2m_inno_clockmonotonic_raw();
		mod_timer(&(t->timer), t->jiffies);
	}
	return 0;
}

static void vpu_stop_workload_timer(vpu_drv_info *drv_info) {
	int ret;
	if (drv_info->workload_timer == NULL) {
		return;
	}
	drv_info->workload_timer->drv_info = NULL;
	while (1) {
		ret = del_timer_sync(&(drv_info->workload_timer->timer));
		if (ret == 0) {
			break;
		}
		msleep(10);
	}
	kfree(drv_info->workload_timer);
	drv_info->workload_timer = NULL;
}

static int inno_init_drv_info(void)
{
	g_vpu_drv_info = fh2m_inno_kzalloc_kernel(sizeof(vpu_drv_info));
	if (unlikely(!g_vpu_drv_info)) {
		vpu_prerr("no memory for g_vpu_drv_info!");
		return -ENOMEM;
	}

	fh2m_inno_atomic64_set(&(g_vpu_drv_info->vpu_num), 0);

	g_vpu_drv_info->vpu_ctxs = fh2m_inno_kzalloc_kernel(sizeof(vpu_drv_ctxs *) * MAX_VPU_NODE);
	if (unlikely(!g_vpu_drv_info->vpu_ctxs)) {
		vpu_prerr("no memory for g_vpu_drv_info!");
		return -ENOMEM;
	}

	g_vpu_drv_info->vpu_dir = fh2m_inno_debugfs_or_procfs_create_dir("vpu", NULL);
	if (IS_ERR(g_vpu_drv_info->vpu_dir)) {
		vpu_prerr("failed to create vpu dir debugfs\n");
		return -EINVAL;
	}

	g_vpu_drv_info->info_node = fh2m_inno_debugfs_or_procfs_create_file(g_vpu_drv_info, "info", 0444,
								g_vpu_drv_info->vpu_dir, &info_debugfs_fops);

	if (IS_ERR(g_vpu_drv_info->info_node)) {
		vpu_prerr("failed to create vpu dir debugfs\n");
		return -EINVAL;
	}

	g_vpu_drv_info->mem_node = fh2m_inno_debugfs_or_procfs_create_file(g_vpu_drv_info, "mem", 0444,
								g_vpu_drv_info->vpu_dir, &mem_debugfs_fops);
	if (IS_ERR(g_vpu_drv_info->mem_node)) {
		vpu_prerr("failed to create vpu dir debugfs\n");
		return -EINVAL;
	}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,6,0)) || defined(CONFIG_DEBUG_FS)
	g_vpu_drv_info->debug_node = fh2m_inno_debugfs_or_procfs_create_file(g_vpu_drv_info, "debug", 0644,
								g_vpu_drv_info->vpu_dir, &debug_debugfs_fops);
	if (IS_ERR(g_vpu_drv_info->debug_node)) {
		vpu_prerr("failed to create vpu dir debugfs\n");
		return -EINVAL;
	}
#endif

	if (vpu_start_workload_timer(g_vpu_drv_info) != 0) {
		vpu_prerr("failed to start workload timer\n");
		return -EINVAL;
	}

	return 0;
}

static int inno_uninit_drv_info(void)
{
	if (g_vpu_drv_info == NULL) {
		return 0;
	}

	if (g_vpu_drv_info->workload_timer) {
		vpu_stop_workload_timer(g_vpu_drv_info);
	}

	if (g_vpu_drv_info->mem_node != NULL) {
		fh2m_inno_debugfs_or_procfs_remove_file(g_vpu_drv_info->mem_node, "mem", g_vpu_drv_info->vpu_dir);
		g_vpu_drv_info->mem_node = NULL;
	}

	if (g_vpu_drv_info->info_node != NULL) {
		fh2m_inno_debugfs_or_procfs_remove_file(g_vpu_drv_info->info_node, "info", g_vpu_drv_info->vpu_dir);
		g_vpu_drv_info->info_node = NULL;
	}

	if (g_vpu_drv_info->debug_node != NULL) {
		fh2m_inno_debugfs_or_procfs_remove_file(g_vpu_drv_info->debug_node, "debug", g_vpu_drv_info->vpu_dir);
		g_vpu_drv_info->debug_node = NULL;
	}

	if (g_vpu_drv_info->vpu_dir != NULL) {
		fh2m_inno_debugfs_or_procfs_remove_dir(g_vpu_drv_info->vpu_dir);
		g_vpu_drv_info->vpu_dir = NULL;
	}

	fh2m_inno_kfree(g_vpu_drv_info->vpu_ctxs);

	fh2m_inno_kfree(g_vpu_drv_info);
	g_vpu_drv_info = NULL;

	return 0;
}

static int innovpu_probe(struct platform_device *pdev)
{
	int ret = 0;
	u32 vpu_id, pdev_id;
	plat_data_t *pdata;
	struct resource *res = NULL;
	vpu_drv_ctxs *vpu_drv_ctx;

	if (!pdev) {
		vpu_prerr("%s platform_device is null", __func__);
		return -EINVAL;
	}

	fh2m_inno_atomic64_add(1, &g_vpu_node_num);
	if (fh2m_inno_atomic64_read(&g_vpu_node_num) == 1) {
		if (inno_init_drv_info() != 0) {
			vpu_error(&(pdev->dev), "inno_init_drv_info error\n");
			ret = -EINVAL;
			goto ERR_INIT_DRV_INFO;
		}
	}

	pdata = dev_get_platdata(&pdev->dev);
	if (!pdata) {
		vpu_prerr("%s dev_get_platdata failed", __func__);
		ret = -EINVAL;
		goto ERR_INIT_DRV_INFO;
	}
	vpu_id = pdata->dev_idx;
	pdev_id = pdata->pdev_idx;

	vpu_info(&(pdev->dev), "vpu_id: %d:%d, irq: %d\n", pdev_id, vpu_id, vpu_irq[vpu_id]);

	vpu_drv_ctx = fh2m_inno_kzalloc_kernel(sizeof(vpu_drv_ctxs));
	if (unlikely(!vpu_drv_ctx)) {
		vpu_error(&(pdev->dev), "no memory for vpu driver context!\n");
		ret = -ENOMEM;
		goto ERR_INIT_DRV_INFO;
	}

	vpu_drv_ctx->drv_context.dev = &(pdev->dev);
	vpu_drv_ctx->drv_context.parent = pdev->dev.parent;
	vpu_drv_ctx->drv_context.vpu_id = vpu_id;
	vpu_drv_ctx->drv_context.hostmem_buffers = fh2m_inno_idr_kalloc( );

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "vpu-regs");
	if (res) {/* if platform driver is implemented */
		vpu_drv_ctx->drv_context.vpu_register.phys_addr = res->start;
		vpu_drv_ctx->drv_context.vpu_register.virt_addr = (u64)ioremap_nocache(res->start, res->end - res->start);
		vpu_drv_ctx->drv_context.vpu_register.size = (res->end - res->start + 1);
		vpu_info(&(pdev->dev), "vpu-regs paddr=0x%llx, vaddr=0x%llx\n",
			vpu_drv_ctx->drv_context.vpu_register.phys_addr ,  vpu_drv_ctx->drv_context.vpu_register.virt_addr);
	} else {
		vpu_info(&(pdev->dev), "vpu-regs paddr=0x%llx, vaddr=0x%llx\n",
			vpu_drv_ctx->drv_context.vpu_register.phys_addr ,  vpu_drv_ctx->drv_context.vpu_register.virt_addr);
		ret = -EINVAL;
		goto ERR_GET_RESOURCE;
	}
	vpu_drv_ctx->drv_context.vpu_register.domain = INNO_VPU_GEM_DOMAIN_CPU;

	ret = vpu_get_hwinfo(vpu_drv_ctx);
	if (ret) {
		vpu_error(&(pdev->dev), "vpu_get_hwinfo failed\n");
		goto ERR_GET_HWINFO;
	}

	ret = vpu_init_context(vpu_drv_ctx);
	if (ret) {
		vpu_error(&(pdev->dev), "vpu_init_context failed\n");
		goto ERR_INIT_CONTEXT;
	}

	g_vpu_drv_info->vpu_ctxs[fh2m_inno_atomic64_read(&g_vpu_drv_info->vpu_num)] = vpu_drv_ctx;
	fh2m_inno_atomic64_add(1, &g_vpu_drv_info->vpu_num);
	if (fh2m_inno_atomic64_read(&g_vpu_drv_info->vpu_num) >= MAX_VPU_NODE) {
		vpu_error(&(pdev->dev), "vpu node num exceed the limit %d!\n", MAX_VPU_NODE);
		goto ERR_INIT_CONTEXT;
	}

	/* move vpu res init from xx_soc.c, xx_soc.c only vpu platform register */
	ret = vpu_hw_res_init(&vpu_drv_ctx->drv_context);
	if (ret) {
		vpu_error(&(pdev->dev), "vpu_hw_res_init failed\n");
		goto ERR_RES_INIT;
	}

	/* read product code from reg after hw reset */
	if (vpu_drv_ctx->drv_context.product_code != vpu_get_product_code(&vpu_drv_ctx->drv_context)) {
		vpu_error(&(pdev->dev), "vpu get product code failed from reg\n");
		goto ERR_RES_INIT;
	}

	ret = vpu_set_irq(vpu_drv_ctx);
	if (ret) {
		vpu_error(&(pdev->dev), "vpu_set_irq failed\n");
		goto ERR_RES_INIT;
	}

#ifdef USE_CODEC_NAME//TODO...
	vpu_set_vpudev_name(vpu_drv_ctx->drv_context.dev, vpu_drv_ctx->drv_context.product_code, (char *)vpu_drv_ctx->drv_context.vpuname);
#else
	sprintf((char *)vpu_drv_ctx->drv_context.vpuname, "vpu%d", (pdev_id * vpu_drv_ctx->drv_context.chip_info.vpu_num + vpu_id));
#endif

	ret = vpu_create_device(vpu_drv_ctx);
	if (ret) {
		vpu_error(&(pdev->dev), "failed to create vpu device\n");
		goto ERR_CREATE_DEVICE;
	}

	ret = vpu_create_debugfs(vpu_drv_ctx);
	if (ret) {
		vpu_error(&(pdev->dev), "failed to create vpu debugfs\n");
		goto ERR_CREATE_DEBUGFS;
	}

	platform_set_drvdata(pdev, vpu_drv_ctx);
	vpu_dbg(vpu_drv_ctx->drv_context.dev, "<-john: dev %px pdev %px vpu_drv_ctx %px->\n", &pdev->dev, pdev, vpu_drv_ctx);
#ifdef CONFIG_PM_SLEEP
	vpu_drv_ctx->pm_notifier.notifier_call = vpu_notifier;
	register_pm_notifier(&vpu_drv_ctx->pm_notifier);
#endif
#ifdef USE_REFACTOR_LOGIC
	ret = vpu_start_exec_thread(&(vpu_drv_ctx->drv_context));
	if (ret) {
		vpu_error(&(pdev->dev), "failed to create vpu debugfs\n");
		goto ERR_START_THREAD;
	}

	/* load firmware and INIT_VPU, tested on 517 and 627 for now */
	if (vpu_load_firmware_and_INIT_VPU(vpu_drv_ctx->drv_context.product_code)) {
		ret = vpu_register_request_firmware(&(vpu_drv_ctx->drv_context));
		if(ret != 0) {
			vpu_error(&(pdev->dev), "failed to vpu_register_request_firmware\n");
			 goto ERR_REGISTER_FIRMWARE; /* silently pass to give legacy umd application a chance. */
		}
	}
#endif

	vpu_dbg(&(pdev->dev), " %s probe success\n", vpu_drv_ctx->drv_context.vpuname);

	return 0;
#ifdef USE_REFACTOR_LOGIC
ERR_REGISTER_FIRMWARE:
	vpu_drv_ctx->drv_context.vpu_exec_stop = true;
	fh2m_inno_wake_up_interruptible(vpu_drv_ctx->drv_context.vpu_exec_wq);
	fh2m_inno_task_stop(vpu_drv_ctx->drv_context.vpu_exec_thread);
	fh2m_inno_wake_up_interruptible(vpu_drv_ctx->drv_context.vpu_dpc_wq);
	fh2m_inno_task_stop(vpu_drv_ctx->drv_context.vpu_dpc_thread);
#ifdef VPU_DEBUG
	if(vpu_drv_ctx->drv_context.vpu_sw_uart_run) {
		vpu_disable_sw_uart(&(vpu_drv_ctx->drv_context));
		fh2m_inno_task_stop(vpu_drv_ctx->drv_context.vpu_sw_uart_thread);
	}
#endif
ERR_START_THREAD:
	if (vpu_drv_ctx->vpu_node)
		debugfs_remove(vpu_drv_ctx->vpu_node);

	if (vpu_drv_ctx->vpu_dir)
		debugfs_remove(vpu_drv_ctx->vpu_dir);
#endif
ERR_CREATE_DEBUGFS:
	vpu_destroy_device(vpu_drv_ctx);

ERR_CREATE_DEVICE:
#ifdef USE_CODEC_NAME
	vpu_reset_vpudev_num(vpu_drv_ctx->drv_context.dev, vpu_drv_ctx->drv_context.product_code);
#endif

	vpu_unset_irq(vpu_drv_ctx);

ERR_RES_INIT:
	vpu_deinit_context(vpu_drv_ctx);

ERR_INIT_CONTEXT:
ERR_GET_HWINFO:
	if (vpu_drv_ctx->drv_context.vpu_register.virt_addr) {
		fh2m_inno_iounmap((void *) vpu_drv_ctx->drv_context.vpu_register.virt_addr);
	}
ERR_GET_RESOURCE:
	fh2m_inno_kfree(vpu_drv_ctx);
ERR_INIT_DRV_INFO:
	inno_uninit_drv_info();

	vpu_error(&(pdev->dev), "vpu driver probe failed\n");
	return ret;
}

#ifdef VPU_SUPPORT_PLATFORM_DRIVER_REGISTER
static int innovpu_remove(struct platform_device *pdev)
{
	vpu_drv_ctxs *vpu_drv_ctx = platform_get_drvdata(pdev);
	if (!vpu_drv_ctx) {
		vpu_prerr("%s: vpu_drv_ctx is NULL \n", __func__);
		return -EINVAL;
	}

	vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "vpu_remove\n");

	if (vpu_drv_ctx->vpu_node)
		fh2m_inno_debugfs_or_procfs_remove_file(vpu_drv_ctx->vpu_node, "vpu_status", vpu_drv_ctx->vpu_dir);

	if (vpu_drv_ctx->vpu_dir)
		fh2m_inno_debugfs_or_procfs_remove_dir(vpu_drv_ctx->vpu_dir);

	if (vpu_drv_ctx->drv_context.instance_pool.base) {
		fh2m_inno_vfree((const void *)vpu_drv_ctx->drv_context.instance_pool.base);
		vpu_drv_ctx->drv_context.instance_pool.base = 0;
	}

	if (vpu_drv_ctx->drv_context.common_memory.base) {
		vpu_free_dma_buffer(&vpu_drv_ctx->drv_context.common_memory, &vpu_drv_ctx->drv_context);
		vpu_drv_ctx->drv_context.common_memory.base = 0;
	}

	if (vpu_drv_ctx->drv_context.pvric_memory.base) {
		vpu_free_dma_buffer(&vpu_drv_ctx->drv_context.pvric_memory, &vpu_drv_ctx->drv_context);
		vpu_drv_ctx->drv_context.pvric_memory.base = 0;
	}
#ifdef USE_REFACTOR_LOGIC
	{
		BUG_ON(vpu_drv_ctx->drv_context.vpu_exec_thread == NULL);
		vpu_drv_ctx->drv_context.vpu_exec_stop = true;
		fh2m_inno_wake_up_interruptible(vpu_drv_ctx->drv_context.vpu_exec_wq);
		fh2m_inno_task_stop(vpu_drv_ctx->drv_context.vpu_exec_thread);
		vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "john: vpu_exec_thread stopped\n");

		BUG_ON(vpu_drv_ctx->drv_context.vpu_dpc_thread == NULL);
		fh2m_inno_wake_up_interruptible(vpu_drv_ctx->drv_context.vpu_dpc_wq);
		fh2m_inno_task_stop(vpu_drv_ctx->drv_context.vpu_dpc_thread);
		vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "john: vpu_dpc_thread stopped\n");
#ifdef VPU_DEBUG
		if (vpu_drv_ctx->drv_context.vpu_sw_uart_run) {
			BUG_ON(vpu_drv_ctx->drv_context.vpu_sw_uart_thread == NULL);
			vpu_disable_sw_uart(&(vpu_drv_ctx->drv_context));
			fh2m_inno_task_stop(vpu_drv_ctx->drv_context.vpu_sw_uart_thread);
			vpu_dbg(vpu_drv_ctx->drv_context.vpudev, "john: vpu_sw_uart_thread stopped\n");
		}
#endif
	}
#endif
	if (vpu_drv_ctx->drv_context.hostmem_buffers) {
		/* free all host mem buffer*/
		fh2m_inno_idr_foreach(vpu_drv_ctx->drv_context.hostmem_buffers, vpu_idr_dma_unmap_hostmem, vpu_drv_ctx->drv_context.parent);
		/* release all internal memory from an IDR */
		fh2m_inno_idr_destroy(vpu_drv_ctx->drv_context.hostmem_buffers);
		/* release IDR */
		fh2m_inno_idr_free(vpu_drv_ctx->drv_context.hostmem_buffers);
		vpu_drv_ctx->drv_context.hostmem_buffers = NULL;
	}
	vpu_destroy_device(vpu_drv_ctx);

	fh2m_inno_atomic64_sub(1, &g_vpu_drv_info->vpu_num);
	g_vpu_drv_info->vpu_ctxs[vpu_drv_ctx->drv_context.vpu_id] = NULL;
	fh2m_inno_atomic64_sub(1, &g_vpu_node_num);
	if (fh2m_inno_atomic64_read(&g_vpu_node_num) == 0) {
		inno_uninit_drv_info();
	}

#ifdef USE_CODEC_NAME
	vpu_reset_vpudev_num(vpu_drv_ctx->drv_context.dev, vpu_drv_ctx->drv_context.product_code);
#endif

	vpu_unset_irq(vpu_drv_ctx);

	vpu_deinit_context(vpu_drv_ctx);

	if (vpu_drv_ctx->drv_context.vpu_register.virt_addr)
		fh2m_inno_iounmap((void *)vpu_drv_ctx->drv_context.vpu_register.virt_addr);

#ifdef CONFIG_PM_SLEEP
	unregister_pm_notifier(&vpu_drv_ctx->pm_notifier);
#endif

	fh2m_inno_kfree(vpu_drv_ctx);

	return 0;
}

static void innovpu_shutdown(struct platform_device *pdev)
{
	vpu_drv_ctxs *vpu_drv_ctx = platform_get_drvdata(pdev);

	vpu_prinfo("%s:%d\n", __func__, __LINE__);

	vpu_poweroff(&vpu_drv_ctx->drv_context);
}

static struct platform_device_id s_inno_vpu_platform_device_id_table[] = {
	{ .name = INNO_CODEC_DEVICE_NAME, .driver_data = 0 },
	{ },
};
MODULE_DEVICE_TABLE(platform, s_inno_vpu_platform_device_id_table);

static int innovpu_suspend(struct device *dev)
{
	struct platform_device *pdev;
	vpu_drv_ctxs *vpu_drv_ctx;

	vpu_prinfo("%s:%d\n", __func__, __LINE__);

	pdev = container_of(dev, struct platform_device, dev);
	vpu_drv_ctx = platform_get_drvdata(pdev);

	return vpu_suspend(&vpu_drv_ctx->drv_context);
}

static int innovpu_resume(struct device *dev)
{
	struct platform_device *pdev;
	vpu_drv_ctxs *vpu_drv_ctx;

	vpu_prinfo("%s:%d\n", __func__, __LINE__);

	pdev = container_of(dev, struct platform_device, dev);
	vpu_drv_ctx = platform_get_drvdata(pdev);

	return vpu_resume(&vpu_drv_ctx->drv_context);
}

static int innovpu_freeze(struct device *dev)
{
	vpu_prinfo("%s:%d\n", __func__, __LINE__);

	return innovpu_suspend(dev);
}

static int innovpu_thaw(struct device *dev)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	vpu_drv_ctxs *vpu_drv_ctx = platform_get_drvdata(pdev);

	vpu_prinfo("%s:%d\n", __func__, __LINE__);

	return vpu_thaw(&vpu_drv_ctx->drv_context);
}

static int innovpu_poweroff(struct device *dev)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	vpu_drv_ctxs *vpu_drv_ctx = platform_get_drvdata(pdev);

	vpu_prinfo("%s:%d\n", __func__, __LINE__);

	return vpu_poweroff(&vpu_drv_ctx->drv_context);
}

static int innovpu_restore(struct device *dev)
{
	vpu_prinfo("%s:%d\n", __func__, __LINE__);

	return innovpu_resume(dev);
}

void vpu_dma_buf_signal(int fd, bool write, void *dma_fence_out) {
	vpu_dma_fence_signal(fd, write, dma_fence_out);
	dma_buf_put(dma_fence_out);/* pair match with dma_buf_get */
}

void vpu_dma_buf_wait(int fd, void *dma_buf_in, void *dma_fence_out) {
	vpu_dma_fence_wait(fd, dma_buf_in, dma_fence_out);
}

u32 vpu_get_semaphore_size(void) {
	return (u32)sizeof(struct semaphore);
}
u32 vpu_get_kfifo_size(void) {
	return (u32)sizeof(struct kfifo);
}

static const struct dev_pm_ops innovpu_pm_ops = {
#ifdef CONFIG_PM
	.suspend = innovpu_suspend,
	.resume = innovpu_resume,
	.freeze = innovpu_freeze,
	.restore = innovpu_restore,
#endif
	.thaw = innovpu_thaw,
	.poweroff = innovpu_poweroff,
};

static struct platform_driver innovpu_driver = {
	.driver = {
		.name = INNO_CODEC_DEVICE_NAME,
		.pm = &innovpu_pm_ops,
	},
	.probe = innovpu_probe,
	.remove = innovpu_remove,
	.shutdown = innovpu_shutdown,
	.id_table = s_inno_vpu_platform_device_id_table,
};
#endif /* VPU_SUPPORT_PLATFORM_DRIVER_REGISTER */

#ifdef CONFIG_DRM_INNO_VPU
int innovpu_init(void)
#else
static int __init vpu_init(void)
#endif
{
	int ret;
	fh2m_gpu_extern_vram_flag(2);
	ret = platform_driver_register(&innovpu_driver);
	return ret;
}

#ifdef CONFIG_DRM_INNO_VPU
void innovpu_exit(void)
#else
static void __exit vpu_exit(void)
#endif
{
	platform_driver_unregister(&innovpu_driver);
	return;
}

#ifndef CONFIG_DRM_INNO_VPU
module_init(vpu_init);
module_exit(vpu_exit);

MODULE_AUTHOR("Innosilicon Technologies Ltd. <support@innosilicon.com.cn>");
MODULE_DESCRIPTION("VPU linux driver");
MODULE_LICENSE("Dual BSD/GPL");
#endif

