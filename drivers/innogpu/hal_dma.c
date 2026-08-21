/*************************************************************************/ /*!
@File           hal_dma.c
@Title
@Copyright      Copyright (c) Innosilicon Technology Ltd. All Rights Reserved
@Description
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
#include <linux/rbtree.h>
#include "inno_plat_dev.h"
#include "inno_debug.h"
#include "inno_dma.h"
#include "inno_mm.h"
#include "inno_misc.h"
#include "inno_timer.h"
#include "inno_lock.h"
#include "inno_drm_version.h"
#include "kernel_compat.h"
#include "hal_interface.h"
#include "hal.h"


// #define ENABLE_DMA_INTERNAL_MANAGER_CHAN
#define KBUILD_HAL "hal_dma"
#define pr_fmt_irq(fmt) "[%s][%s:%d]" fmt,KBUILD_HAL,__func__,__LINE__

#define hal_dma_error(dev, fmt, ...) \
	do { \
		fh2m_inno_dev_printk(KERN_ERR, dev, pr_fmt_irq(fmt), ##__VA_ARGS__); \
	} while (0);

#define hal_dma_warn(dev, fmt, ...) \
	do { \
		fh2m_inno_dev_printk(KERN_WARNING, dev, pr_fmt_irq(fmt), ##__VA_ARGS__); \
	} while (0);

#define hal_dma_debug(dev, fmt, ...) \
	do { \
		fh2m_inno_dev_printk(KERN_DEBUG, dev, pr_fmt_irq(fmt), ##__VA_ARGS__); \
	} while (0);

#define DMA_POOL_DWORK_INTERVAL 2000

struct hal_dma_chan_filter_param
{
	inno_dev* dev;
	int      chan_type;
};

struct hal_dma_phys_chan {
	int task_cnt;
	int dma_type;
	struct rb_node node;
	inno_dma_chan* chan;
	inno_mutex* chan_lock;
};

struct hal_dma_chan {
	int dma_type;
	int phys_chan_cnt;
#if (DRM_VERSION >= KERNEL_VERSION(4,14,0))
	struct rb_root_cached chan_root_cached;
#else
	struct rb_root chan_root;
	struct rb_node *rb_leftmost;
#endif
	struct hal_dma_phys_chan *chan_list;
};

struct hal_dma_chan_pool {
	inno_dev* dev;
	inno_spinlock* pool_lock;
	inno_workqueue* cleanup_wk_queue;
	int dma_total_load;
	uint32_t           dwork_msec;
	struct inno_dwork* dma_dwork; /* release dma channel */
	struct hal_dma_chan chans[HAL_DMA_CHAN_TYPE_MAX];
};

struct hal_dma_memcpy_data {
	uint64_t dma_mapped_addr;
	size_t   size;
};

struct hal_dma_callback_param
{
	inno_dev* dev;
	inno_dma_async_tx_desc* tx;

	void* cleanup_param;

	int dir;
	uint64_t src_addr;
	uint64_t dst_addr;
	struct hal_dma_mapped_buffer dma_mapped_buffer;

	struct hal_dma_phys_chan* chan;
	struct hal_dma_chan_pool* chan_pool;
};

struct hal_dma_cleanup_param
{
	inno_dev* dev;

	atomic_t ref_count;			/* 成功下发DMA传输的计数 */
	atomic_t done_ref_count;	/* DMA回调函数调用的计数 */
	atomic_t err_ref_count;		/* 下发请求出现错误的标志 */
	int num_dmas;

	pfn_dma_func cb;
	pfn_dma_func fast_cb;
	void* cb_param;

	inno_work* cleanup_wk;
	inno_workqueue* cleanup_wk_queue;

	struct hal_dma_callback_param** callback_params;
};

#if (DRM_VERSION >= KERNEL_VERSION(4,14,0))
static void chan_insert(struct rb_root_cached *root_cached, struct hal_dma_phys_chan *pchan)
{
	struct rb_root *root;
	struct rb_node **new ;
	struct rb_node *parent = NULL;
	bool leftmost = true;
	root = &root_cached->rb_root;
	new = &(root->rb_node);

	/* Figure out where to put new node */
	while (*new) {
		struct hal_dma_phys_chan *this = container_of(*new, struct hal_dma_phys_chan, node);
		parent = *new;
		if (pchan->task_cnt <= this->task_cnt)
			new = &((*new)->rb_left);
		else {
			new = &((*new)->rb_right);
			leftmost = false;
		}
	}

	/* Add new node and rebalance tree. */
	rb_link_node(&pchan->node, parent, new);
	rb_insert_color_cached(&pchan->node, root_cached, leftmost);
}

static struct hal_dma_phys_chan *get_min_load_chan(struct rb_root_cached *root_cached)
{
	struct rb_node *rb_leftmost;
	struct hal_dma_phys_chan *pchan;

	rb_leftmost = root_cached->rb_leftmost;
	pchan = container_of(rb_leftmost, struct hal_dma_phys_chan, node);
	return pchan;
}

#else
static void chan_insert(struct rb_root *root, struct hal_dma_phys_chan *pchan)
{
	struct rb_node **new ;
	struct rb_node *parent = NULL;
	struct hal_dma_chan *hchan;
	bool leftmost = true;
	new = &(root->rb_node);
	hchan = container_of(root, struct hal_dma_chan, chan_root);

	/* Figure out where to put new node */
	while (*new) {
		struct hal_dma_phys_chan *this = container_of(*new, struct hal_dma_phys_chan, node);
		parent = *new;
		if (pchan->task_cnt <= this->task_cnt)
			new = &((*new)->rb_left);
		else {
			new = &((*new)->rb_right);
			leftmost = false;
		}
	}

	/* Add new node and rebalance tree. */
	rb_link_node(&pchan->node, parent, new);
	rb_insert_color(&pchan->node, root);
	if(leftmost)
		hchan->rb_leftmost = &pchan->node;
}

static struct hal_dma_phys_chan *get_min_load_chan(struct rb_root *root)
{
	struct rb_node *rb_leftmost;
	struct hal_dma_phys_chan *pchan;
	struct hal_dma_chan *hchan = container_of(root, struct hal_dma_chan, chan_root);
	rb_leftmost = hchan->rb_leftmost;
	pchan = container_of(rb_leftmost, struct hal_dma_phys_chan, node);
	return pchan;
}
#endif

static struct hal_dma_memcpy_data* hal_dma_get_memcpy_data(inno_dev* dev, void* address, size_t size)
{
	inno_page* pg;
	uint64_t pg_off = 0;
	struct hal_dma_memcpy_data* memcpy_data = NULL;

	memcpy_data = (struct hal_dma_memcpy_data*)fh2m_inno_kvmalloc_kernel(sizeof(struct hal_dma_memcpy_data));
	if (!memcpy_data) {
		hal_dma_error(dev, "alloc sg_data failed\n");
		goto e0;
	}

	pg = fh2m_inno_virt_to_page(address);
	pg_off = fh2m_inno_offset_in_page(address);
	memcpy_data->dma_mapped_addr = (uint64_t)fh2m_inno_dma_map_page(dev, pg, pg_off, fh2m_inno_page_align(size));
	memcpy_data->size = size;

e0:
	return memcpy_data;
}

static void hal_dma_put_memcpy_data(inno_dev* dev, struct hal_dma_memcpy_data* memcpy_data) {

	if (!memcpy_data) {
		return;
	}

	if (memcpy_data->dma_mapped_addr) {
		fh2m_inno_dma_unmap_page(dev, memcpy_data->dma_mapped_addr, fh2m_inno_page_align(memcpy_data->size));
	}

	fh2m_inno_kvfree(memcpy_data);
}

static int hal_dma_get_pages(inno_dev* dev, bool mem2dev, void* virtual_addr, inno_page** ppages, int num_pages)
{
	int i = 0, ret = HAL_DMA_OK;
	uint32_t gup_flags = 0;
	uint64_t start;
	int num_pinned_pages = 0;

	if (!virtual_addr || !ppages || num_pages == 0) {
		ret = HAL_DMA_ERROR_INVALID_PARAMS;
		goto e0;
	}

	if (fh2m_inno_is_vmalloc_addr(virtual_addr)) {
		for (start = (uint64_t)virtual_addr, i = 0; i < num_pages; start += fh2m_inno_page_size, i++) {
			ppages[i] = fh2m_inno_vmalloc_to_page((void*)start);
			if (!ppages[i]) {
				ret = HAL_DMA_ERROR_VMALLOC_TO_PAGE;
				hal_dma_error(dev, "vmalloc to pages failed, virtual_addr: %#llx\n", start);
				goto e1;
			}
			/* pin memory, the performance is poor, if no problem occurs, do not use it*/
			/* fh2m_inno_set_page_reserved(ppages[i]); */
			num_pinned_pages++;
		}
	} else {
		gup_flags |= (mem2dev ? 0 : 1);
		num_pinned_pages = fh2m_inno_get_user_pages_fast((uint64_t)virtual_addr, num_pages, gup_flags, ppages);
		if (num_pinned_pages != num_pages) {
			ret = HAL_DMA_ERROR_PINNED_PAGES;
			hal_dma_error(dev, "get_user_pages_fast failed, ret(num_pinned_pages): %d, num_pages: %d, addr: %#llx - cur_pid: %d - name: %s",
				num_pinned_pages, num_pages, (uint64_t)virtual_addr, fh2m_inno_task_pid(), fh2m_inno_task_name());
			goto e1;
		}
	}

	return HAL_DMA_OK;

e1:
	for (i = 0; i < num_pinned_pages; i++) {
		if (!fh2m_inno_is_err_or_null(ppages[i])) {
			if (fh2m_inno_is_vmalloc_addr(virtual_addr)) {
				/* fh2m_inno_clear_page_reserved(ppages[i]); */
			} else {
				fh2m_inno_put_page(ppages[i]);
			}
		}
	}
e0:
	return ret;
}

static void hal_dma_put_pages(inno_page** ppages, int num_pages, bool is_user_addr)
{
	int i = 0;

	if (ppages) {
		for (i = 0; i < num_pages; i++) {
			if (!fh2m_inno_is_err_or_null(ppages[i])) {
				if (is_user_addr) {
					fh2m_inno_put_page(ppages[i]);
				} else {
					/* fh2m_inno_clear_page_reserved(ppages[i]); */
				}
			}
		}
	}
}

static struct hal_dma_slave_sg_data* hal_dma_get_slave_sg_data(inno_dev* dev, bool mem2dev, void* address, size_t size)
{
	int ret = 0;
	void* tmp = address;
	uint64_t offset = 0;
	int num_pages = 0;
	inno_page** ppages = NULL;
	inno_sg_table* psSg = NULL;
	struct hal_dma_slave_sg_data* slave_sg_data = NULL;

	slave_sg_data = fh2m_inno_kvmalloc_kernel(sizeof(struct hal_dma_slave_sg_data));
	if (!slave_sg_data) {
		hal_dma_error(dev, "alloc slave_sg_data failed\n");
		goto e0;
	}

	slave_sg_data->is_user_addr = !fh2m_inno_is_vmalloc_addr(address);

	offset = (uint64_t)tmp & ((1 << fh2m_inno_page_shift) - 1);
	num_pages = (size + offset + fh2m_inno_page_size - 1) >> fh2m_inno_page_shift;
	ppages = fh2m_inno_kvmalloc_kernel(num_pages * sizeof(inno_page*));
	if (!ppages) {
		hal_dma_error(dev, "alloc pages failed\n");
		goto e1;
	}

	ret = hal_dma_get_pages(dev,  mem2dev, tmp, ppages, num_pages);
	if (ret) {
		hal_dma_error(dev, "get pages failed\n");
		goto e2;
	}

	psSg = fh2m_inno_sg_table_alloc();
	if (!psSg) {
		hal_dma_error(dev, "failed to alloc sg_table.\n");
		goto e3;
	}

	ret = sg_alloc_table_from_pages(psSg, ppages, num_pages, offset, size, fh2m_inno_page_flag_kernel());
	if (ret) {
		hal_dma_error(dev, "sg_alloc_table_from_pages failed, ret: %d, num_pages: %d, offset: %#llx, size: %#x\n",
					ret, num_pages, offset, size);
		goto e4;
	}

	ret = fh2m_inno_dma_map_sg(dev, psSg);
	if (0 == ret) {
		hal_dma_error(dev, "fh2m_inno_dma_map_sg failed, ret: %d\n", ret);
		goto e5;
	}

	slave_sg_data->psSg = psSg;
	slave_sg_data->ppages = ppages;
	slave_sg_data->num_pages = num_pages;

	return slave_sg_data;
e5:
	sg_free_table(psSg);
e4:
	fh2m_inno_sg_table_free(psSg);
e3:
	hal_dma_put_pages(ppages, num_pages, slave_sg_data->is_user_addr);
e2:
	fh2m_inno_kvfree(ppages);
e1:
	fh2m_inno_kvfree(slave_sg_data);
e0:
	return NULL;
}

static void hal_dma_put_slave_sg_data(inno_dev* dev, struct hal_dma_slave_sg_data* slave_sg_data)
{
	if (dev == NULL || slave_sg_data == NULL) {
		return;
	}

	fh2m_inno_dma_unmap_sg(dev, slave_sg_data->psSg);
	sg_free_table(slave_sg_data->psSg);
	fh2m_inno_sg_table_free(slave_sg_data->psSg);
	hal_dma_put_pages(slave_sg_data->ppages, slave_sg_data->num_pages, slave_sg_data->is_user_addr);
	fh2m_inno_kvfree(slave_sg_data->ppages);
	fh2m_inno_kvfree(slave_sg_data);
}

static void* hal_dma_get_mapped_data(inno_dev* dev, bool mem2dev, void* address, size_t size, bool is_contig_addr)
{
	void* data = NULL;

	if (is_contig_addr) {
		data = (void*)hal_dma_get_memcpy_data(dev, address, size);
	} else {
		data = (void*)hal_dma_get_slave_sg_data(dev, mem2dev, address, size);
	}

	return data;
}

static void hal_dma_put_mapped_data(inno_dev* dev, struct hal_dma_mapped_buffer* mapped_buffer)
{
	if (!mapped_buffer)
		return;

	if (mapped_buffer->is_contig_addr) {
		hal_dma_put_memcpy_data(dev, mapped_buffer->data);
	} else {
		hal_dma_put_slave_sg_data(dev, mapped_buffer->data);
	}
}

void* fh2m_hal_dma_map(inno_dev* dev, bool mem2dev, void* address, size_t size)
{
	struct hal_dma_mapped_buffer* mapped_buffer = NULL;

	if (dev == NULL || address == NULL || size == 0) {
		inno_error("[%s:%d]invalid params\n", __func__, __LINE__);
		goto e0;
	}

	mapped_buffer = fh2m_inno_kvmalloc_kernel(sizeof(struct hal_dma_mapped_buffer));
	if (!mapped_buffer) {
		hal_dma_error(dev, "alloc dma mapped buffer failed\n");
		goto e0;
	}

	fh2m_inno_memset(mapped_buffer, 0, sizeof(struct hal_dma_mapped_buffer));
	mapped_buffer->virt_addr = address;
	mapped_buffer->size = size;
	mapped_buffer->is_contig_addr = fh2m_inno_is_kmalloc_addr(address);
	mapped_buffer->data = hal_dma_get_mapped_data(dev, mem2dev, address, size, mapped_buffer->is_contig_addr);
	if (!mapped_buffer->data) {
		hal_dma_error(dev, "hal_dma_get_mapped_data failed\n");
		goto e1;
	}

	return mapped_buffer;
e1:
	fh2m_inno_kvfree(mapped_buffer);
e0:
	return NULL;
}

void fh2m_hal_dma_unmap(inno_dev* dev, void* buffer)
{
	struct hal_dma_mapped_buffer* mapped_buffer = (struct hal_dma_mapped_buffer*)buffer;

	if ((dev == NULL) || (buffer == NULL)) {
		inno_error("[%s:%d]invalid params\n", __func__, __LINE__);
		return;
	}

	hal_dma_put_mapped_data(dev, mapped_buffer);

	fh2m_inno_kvfree(mapped_buffer);
}

static void hal_dma_release_channel(struct hal_dma_chan_pool* pool, struct hal_dma_phys_chan* pchan)
{
	if (!pchan) {
		return;
	}

	fh2m_inno_spin_lock_irq(pool->pool_lock);
#if (DRM_VERSION >= KERNEL_VERSION(4,14,0))
	rb_erase(&pchan->node, &pool->chans[pchan->dma_type].chan_root_cached.rb_root);
#else
	rb_erase(&pchan->node, &pool->chans[pchan->dma_type].chan_root);
#endif
	pchan->task_cnt--;
	pool->dma_total_load--;
	if (pool->dma_total_load < 1) {
		fh2m_inno_schedule_dwork(pool->dma_dwork, pool->dwork_msec);
	}
#if (DRM_VERSION >= KERNEL_VERSION(4,14,0))
	chan_insert(&pool->chans[pchan->dma_type].chan_root_cached, pchan);
#else
	chan_insert(&pool->chans[pchan->dma_type].chan_root, pchan);
#endif
	fh2m_inno_spin_unlock_irq(pool->pool_lock);
}

static void hal_dma_cleanup_param_free(struct hal_dma_cleanup_param* cleanup_param)
{
	int i = 0;

	if (!cleanup_param) {
		return;
	}

	for (i = 0; i < cleanup_param->num_dmas; i++) {
		if (cleanup_param->callback_params[i] == NULL)
			continue;

		fh2m_inno_kvfree(cleanup_param->callback_params[i]);
		cleanup_param->callback_params[i] = NULL;
	}
	fh2m_inno_kvfree(cleanup_param->callback_params);
	fh2m_inno_work_destroy(cleanup_param->cleanup_wk);
	fh2m_inno_kvfree(cleanup_param);
	cleanup_param = NULL;
}

static void hal_dma_cleanup_work(void* data)
{
	struct hal_dma_cleanup_param* cleanup_param = (struct hal_dma_cleanup_param*)data;

	if (!cleanup_param)
		return;

	if (cleanup_param->cb != NULL)
		cleanup_param->cb(cleanup_param->cb_param);

	hal_dma_cleanup_param_free(cleanup_param);
}

static struct hal_dma_cleanup_param* hal_dma_cleanup_param_alloc(inno_dev* dev, int num_dmas, pfn_dma_func fast_cb, pfn_dma_func cb, void* cb_param)
{
	struct hal_dma_cleanup_param* cleanup_param = NULL;
	struct dev_rsrc* pdev_rsrc = NULL;
	struct hal_dma_chan_pool* pool;

	if (!dev || num_dmas == 0 || !cb || !cb_param) {
		inno_error("[%s:%d]invaild param\n", __func__, __LINE__);
		goto e0;
	}

	pdev_rsrc = (struct dev_rsrc*)fh2m_inno_rsrc_devres_find(dev);
	if (!pdev_rsrc || !pdev_rsrc->dma.chan_pool) {
		hal_dma_error(dev, "find device resource failed\n");
		goto e0;
	}
	pool = pdev_rsrc->dma.chan_pool;

	cleanup_param = (struct hal_dma_cleanup_param*)fh2m_inno_kvmalloc_kernel(sizeof(struct hal_dma_cleanup_param));
	if (!cleanup_param) {
		hal_dma_error(dev, "alloc cleanup_param failed\n");
		goto e0;
	}
	fh2m_inno_memset(cleanup_param, 0, sizeof(struct hal_dma_cleanup_param));

	cleanup_param->cleanup_wk = fh2m_inno_work_alloc(hal_dma_cleanup_work, cleanup_param);
	if (!cleanup_param->cleanup_wk) {
		hal_dma_error(dev, "alloc cleanup_wk failed\n");
		goto e1;
	}
	cleanup_param->cleanup_wk_queue = pool->cleanup_wk_queue;

	cleanup_param->callback_params = (struct hal_dma_callback_param**)fh2m_inno_kvmalloc_kernel(sizeof(struct hal_dma_callback_param*) * num_dmas);
	if (!cleanup_param->callback_params) {
		hal_dma_error(dev, "alloc dma callback_params failed\n");
		goto e2;
	}
	fh2m_inno_memset(cleanup_param->callback_params, 0, sizeof(struct hal_dma_callback_param*) * num_dmas);
	cleanup_param->dev = dev;
	cleanup_param->num_dmas = num_dmas;
	cleanup_param->cb = cb;
	cleanup_param->fast_cb = fast_cb;
	cleanup_param->cb_param = cb_param;
	fh2m_inno_atomic_write(&cleanup_param->ref_count, 0);
	fh2m_inno_atomic_write(&cleanup_param->done_ref_count, 0);
	fh2m_inno_atomic_write(&cleanup_param->err_ref_count, 0);
	return cleanup_param;

e2:
	fh2m_inno_work_destroy(cleanup_param->cleanup_wk);
e1:
	fh2m_inno_kvfree(cleanup_param);
e0:
	return NULL;
}

static bool hal_dma_channel_filter(inno_dev* dev, int chan_id, void* chan_private, void* param)
{
	struct hal_dma_chan_filter_param* filter_param = (struct hal_dma_chan_filter_param*)param;

	if (!chan_private || !param) {
		return false;
	}

	// hal_dma_error(dev, "private: %s, dma_type: %s\n", (char *)chan_private, filter_param->name);

	if (dev != filter_param->dev) {
		return false;
	}

	if ((filter_param->chan_type == HAL_AXI_DMA_LEFT) && !fh2m_inno_strcmp((char*)chan_private, INNO_DMA_DEVICE_NAME)) {
		return true;
	}

	if ((filter_param->chan_type == HAL_AXI_DMA_RIGHT) && !fh2m_inno_strcmp((char*)chan_private, INNO_DMAR_DEVICE_NAME)) {
		return true;
	}

	if ((filter_param->chan_type == HAL_PCIE_DMA_WR) &&
		!fh2m_inno_strcmp((char*)chan_private, INNO_PDMA_DEVICE_NAME) &&
		chan_id < HAL_PCIE_DMA_MAX_WR_CHAN) {
		return true;
	}

	if ((filter_param->chan_type == HAL_PCIE_DMA_RD) &&
		!fh2m_inno_strcmp((char*)chan_private, INNO_PDMA_DEVICE_NAME) &&
		chan_id >= HAL_PCIE_DMA_MAX_WR_CHAN) {
		return true;
	}

	return false;
}

static bool hal_dma_judge_gtt_mem(inno_dev *dev, struct hal_dma_mapped_buffer *mapped_buffer)
{
	bool ret = false;
	uint64_t cpu_pa = 0;

	if (!mapped_buffer) {
		hal_dma_error(dev, "invalid params\n");
		return false;
	}

	if (mapped_buffer->is_contig_addr) {
		struct hal_dma_memcpy_data *memcpy_data = (struct hal_dma_memcpy_data *)mapped_buffer->data;
		mapped_buffer->cpu_pa = memcpy_data->dma_mapped_addr;
		ret = fh2m_hal_is_gtt_mem(dev, memcpy_data->dma_mapped_addr);
	} else {
		struct hal_dma_slave_sg_data *slave_sg_data = (struct hal_dma_slave_sg_data *)mapped_buffer->data;
		inno_sglist *sgl = fh2m_inno_sg_first(slave_sg_data->psSg), *sg;
		int sg_len = fh2m_inno_sgl_len(slave_sg_data->psSg), i = 0;

		inno_for_each_sg(sgl, sg, sg_len, i) {
			cpu_pa = fh2m_inno_sg_phys(sg);
			ret = fh2m_hal_is_gtt_mem(dev, cpu_pa);
			if (!ret) break;
		}
	}

	return ret;
}

static struct hal_dma_phys_chan* hal_dma_request_channel(inno_dev* dev, struct hal_dma_chan_pool* pool, void* src, void* dst, size_t size, int dir)
{
	uint32_t chan_type = ~(~0 << HAL_DMA_CHAN_TYPE_MAX);
	int i = 0;
	uint64_t src_addr, dst_addr;
	struct hal_dma_mapped_buffer* mapped_buffer = NULL;
	struct hal_dma_chan* hal_dma_chan;

	struct hal_dma_phys_chan* min_load_chan = NULL;
	struct hal_dma_phys_chan* cur_load_chan = NULL;
	struct hal_dma_chan_filter_param filter_param;

	if (!dev || !pool || !src || !dst || size == 0) {
		inno_error("[%s:%d]invalid params, (0x%p-0x%p-0x%p-%d)\n", __func__, __LINE__, pool, src, dst, size);
		return NULL;
	}

	if (dir == DMA_DEV2DEV) {
		/* pcie-dma cannot transfer data from device to device */
		chan_type &= ~(1 << HAL_PCIE_DMA_WR);
		chan_type &= ~(1 << HAL_PCIE_DMA_RD);

		/* only G1P support dmar */
		if (!fh2m_hal_is_support_dmar(dev)) {
			chan_type &= ~(1 << HAL_AXI_DMA_RIGHT);
		}
		src_addr = (uint64_t)src;
		dst_addr = (uint64_t)dst;
	} else {
		if (dir == DMA_DEV2MEM) {
			chan_type &= ~(1 << HAL_PCIE_DMA_RD);

			/* pcie-dma cannot access invisible vram */
			if (!fh2m_hal_is_visible_vram(dev, (uint64_t)src + size)) {
				chan_type &= ~(1 << HAL_PCIE_DMA_WR);
			}

			mapped_buffer = (struct hal_dma_mapped_buffer*)dst;
			/* AXI-DMA cannot access non GTT  */
			if (!hal_dma_judge_gtt_mem(dev, mapped_buffer)) {
				chan_type &= ~(1 << HAL_AXI_DMA_LEFT);
				chan_type &= ~(1 << HAL_AXI_DMA_RIGHT);
			}
			src_addr = (uint64_t)src;
			dst_addr = (uint64_t)mapped_buffer->virt_addr;
		} else {
			chan_type &= ~(1 << HAL_PCIE_DMA_WR);

			if (!fh2m_hal_is_visible_vram(dev, (uint64_t)dst + size)) {
				chan_type &= ~(1 << HAL_PCIE_DMA_RD);
			}

			mapped_buffer = (struct hal_dma_mapped_buffer*)src;
			/* AXI-DMA cannot access non GTT  */
			if (!hal_dma_judge_gtt_mem(dev, mapped_buffer)) {
				chan_type &= ~(1 << HAL_AXI_DMA_LEFT);
				chan_type &= ~(1 << HAL_AXI_DMA_RIGHT);
			}
			src_addr = (uint64_t)mapped_buffer->virt_addr;
			dst_addr = (uint64_t)dst;
		}
	}

	/* if addr or len not aligned with 32 byte, can not use axi-dma or dmar */
	/* TODO: only G0C axi-dma has to be aligned with 32 byte */
	if (CHIP_G0_SOC == fh2m_hal_get_chiptype(dev)) {
		if (!fh2m_inno_is_aligned(src_addr, 32) ||
			!fh2m_inno_is_aligned(dst_addr, 32) ||
			!fh2m_inno_is_aligned(size, 32)) {
			chan_type &= ~(1 << HAL_AXI_DMA_LEFT);
			chan_type &= ~(1 << HAL_AXI_DMA_RIGHT);
		}
	}
	if (chan_type == 0) {
		hal_dma_error(dev, "failed to select dma type, (%d %d 0x%llx 0x%llx)\n", size, dir, src_addr, dst_addr);
		dump_stack();
		return NULL;
	}

	/* in all suitable channels select a min load chan */
	fh2m_inno_spin_lock_irq(pool->pool_lock);

	for (i = 0; i < HAL_DMA_CHAN_TYPE_MAX; i++) {

		if ((chan_type & (1 << i)) == 0) {
			continue;
		}

		hal_dma_chan = &pool->chans[i];

		if (hal_dma_chan->phys_chan_cnt == 0) {
			continue;
		}
#if (DRM_VERSION >= KERNEL_VERSION(4,14,0))
		cur_load_chan = get_min_load_chan(&hal_dma_chan->chan_root_cached);
#else
		cur_load_chan = get_min_load_chan(&hal_dma_chan->chan_root);
#endif

		if (cur_load_chan->task_cnt == 0) {
			filter_param.dev = dev;
			filter_param.chan_type = i;
			min_load_chan = cur_load_chan;
			break;
		}

		if (min_load_chan == NULL) {
			filter_param.dev = dev;
			filter_param.chan_type = i;
			min_load_chan = cur_load_chan;
			continue;
		}

		if (cur_load_chan->task_cnt < min_load_chan->task_cnt) {
			filter_param.dev = dev;
			filter_param.chan_type = i;
			min_load_chan = cur_load_chan;
		}
	}

	if (min_load_chan != NULL) {
		pool->dma_total_load++;
		min_load_chan->task_cnt++;
#if (DRM_VERSION >= KERNEL_VERSION(4,14,0))
		rb_erase_cached(&min_load_chan->node, &pool->chans[min_load_chan->dma_type].chan_root_cached);
		chan_insert(&pool->chans[min_load_chan->dma_type].chan_root_cached, min_load_chan);
#else
		rb_erase(&min_load_chan->node, &pool->chans[min_load_chan->dma_type].chan_root);
		chan_insert(&pool->chans[min_load_chan->dma_type].chan_root, min_load_chan);
#endif
	} else {
		inno_error("[%s:%d] not found min_load_chan. (%d %d 0x%llx 0x%llx) chan_type: %#x, i: %d\n",
			__func__, __LINE__, size, dir, src_addr, dst_addr, chan_type, i);
		fh2m_inno_spin_unlock_irq(pool->pool_lock);
		return NULL;
	}

	fh2m_inno_spin_unlock_irq(pool->pool_lock);

	fh2m_inno_mutex_lock(min_load_chan->chan_lock);
	if (min_load_chan->chan == NULL) {
		min_load_chan->chan = fh2m_inno_dma_request_channel(fh2m_inno_dma_cap_slave, hal_dma_channel_filter, &filter_param);
		/* Normally,this should not happen! */
		if (min_load_chan->chan == NULL) {
			hal_dma_error(dev, "dma request channel failed, chan_type: %d\n", filter_param.chan_type);
			min_load_chan->task_cnt--;
			pool->dma_total_load--;
			fh2m_inno_mutex_unlock(min_load_chan->chan_lock);
			return NULL;
		}
	}
	fh2m_inno_mutex_unlock(min_load_chan->chan_lock);
	return min_load_chan;
}

static void hal_dma_callback(void* param)
{
	struct hal_dma_cleanup_param* cleanup_param;
	struct hal_dma_callback_param* callback_param;

	if (!param) {
		return;
	}

	callback_param = (struct hal_dma_callback_param*)param;
	cleanup_param = callback_param->cleanup_param;

	if (!cleanup_param) {
		return;
	}

	fh2m_inno_atomic_inc(&cleanup_param->done_ref_count);

	/* invalid cache after dma transfer*/
	if (callback_param->dir == DMA_DEV2MEM && callback_param->dev) {
		if (callback_param->dma_mapped_buffer.is_contig_addr) {
			struct hal_dma_memcpy_data* memcpy_data = (struct hal_dma_memcpy_data*)callback_param->dma_mapped_buffer.data;
			fh2m_inno_dma_sync_single_for_cpu(callback_param->dev, memcpy_data->dma_mapped_addr, memcpy_data->size);
		} else {
			struct hal_dma_slave_sg_data* slave_sg_data = (struct hal_dma_slave_sg_data*)callback_param->dma_mapped_buffer.data;
			fh2m_inno_dma_sync_sg_for_cpu(callback_param->dev, slave_sg_data->psSg);
		}
	}

	// fast callback must call immediately
	if ((fh2m_inno_atomic_read(&cleanup_param->done_ref_count) == cleanup_param->num_dmas) &&
	    (fh2m_inno_atomic_read(&cleanup_param->err_ref_count) == 0)) {
		if (cleanup_param->fast_cb)
			cleanup_param->fast_cb(cleanup_param->cb_param);
	}

	fh2m_inno_dmaengine_desc_clear_reuse(callback_param->tx);

	hal_dma_release_channel(callback_param->chan_pool, callback_param->chan);
	if ((fh2m_inno_atomic_read(&cleanup_param->done_ref_count) == cleanup_param->num_dmas) &&
	    (fh2m_inno_atomic_read(&cleanup_param->err_ref_count) == 0)) {
		fh2m_inno_queue_work(cleanup_param->cleanup_wk_queue, cleanup_param->cleanup_wk);
	}
	/* all channels have called callback functions and encountered errors, actively releasing resources */
	if ((fh2m_inno_atomic_read(&cleanup_param->done_ref_count) == fh2m_inno_atomic_read(&cleanup_param->ref_count)) &&
	    (fh2m_inno_atomic_read(&cleanup_param->err_ref_count) > 0)) {
		hal_dma_cleanup_param_free(cleanup_param);
	}
}

static inno_dma_async_tx_desc*
hal_dma_prepare_transfer(inno_dev* dev, struct hal_dma_chan_pool* chan_pool, struct hal_dma_phys_chan* pchan, void* src, void* dst, int len, int dir,
	struct hal_dma_cleanup_param* cleanup_param, int transfer_idx)
{
	int ret = HAL_DMA_OK;
	inno_dma_chan* chan = NULL;
	inno_dma_async_tx_desc* tx = NULL;
	struct hal_dma_callback_param* callback_param = NULL;

	if (!dev || !pchan || !src || !dst || len == 0 || !cleanup_param) {
		ret = HAL_DMA_ERROR_INVALID_PARAMS;
		inno_error("[%s:%d]invaild param\n", __func__, __LINE__);
		goto e0;
	}

	callback_param = (struct hal_dma_callback_param*)fh2m_inno_kvmalloc_kernel(sizeof(struct hal_dma_callback_param));
	if (!callback_param) {
		hal_dma_error(dev, "alloc callback_param failed\n");
		goto e0;
	}

	chan = pchan->chan;

	if (DMA_DEV2DEV == dir) {
		tx = fh2m_inno_dmaengine_prep_memcpy(chan, src, dst, len);
	} else {
		void* src_addr = NULL, * dst_addr = NULL;
		struct hal_dma_mapped_buffer* mapped_buffer = NULL;

		if (DMA_DEV2MEM == dir) {
			mapped_buffer = (struct hal_dma_mapped_buffer*)dst;
		} else {
			mapped_buffer = (struct hal_dma_mapped_buffer*)src;
		}

		if (mapped_buffer->is_contig_addr) {
			struct hal_dma_memcpy_data* memcpy_data = (struct hal_dma_memcpy_data*)mapped_buffer->data;

			src_addr = (DMA_DEV2MEM == dir) ? src : (void*)memcpy_data->dma_mapped_addr;
			dst_addr = (DMA_MEM2DEV == dir) ? dst : (void*)memcpy_data->dma_mapped_addr;

			 /* PCIE-DMA has to use pcie domain addr, AXI-DMA has to use gtt  */
			if (DMA_MEM2DEV == dir) {
				if (pchan->dma_type == HAL_PCIE_DMA_RD)
					dst_addr = (void *)fh2m_cpu_paddr_to_pcie_paddr(dev, fh2m_dev_paddr_to_cpu_paddr(dev, (uint64_t)dst_addr));
				else if (pchan->dma_type == HAL_AXI_DMA_LEFT || pchan->dma_type == HAL_AXI_DMA_RIGHT)
					src_addr = (void *)fh2m_cpu_paddr_to_gtt_paddr(dev, (uint64_t)src_addr);
			} else {
				if (pchan->dma_type == HAL_PCIE_DMA_WR)
					src_addr = (void *)fh2m_cpu_paddr_to_pcie_paddr(dev, fh2m_dev_paddr_to_cpu_paddr(dev, (uint64_t)src_addr));
				else if (pchan->dma_type == HAL_AXI_DMA_LEFT || pchan->dma_type == HAL_AXI_DMA_RIGHT)
					dst_addr = (void *)fh2m_cpu_paddr_to_gtt_paddr(dev, (uint64_t)dst_addr);
			}

			tx = fh2m_inno_dmaengine_prep_memcpy(chan, src_addr, dst_addr, len);

			/* flush the cache to the main memory */
			if (DMA_MEM2DEV == dir) {
				fh2m_inno_dma_sync_single_for_device(dev, (dma_addr_t)&memcpy_data->dma_mapped_addr, len);
			}
		} else {
			struct hal_dma_slave_sg_data* slave_sg_data = (struct hal_dma_slave_sg_data*)mapped_buffer->data;

			fh2m_inno_mutex_lock(pchan->chan_lock);
			if (dir == DMA_DEV2MEM) {
				/* If axi-dma is used, mapped_buffer->offset refers to the page offset;
				if pcie-dma is used, mapped_buffer->offset = 0 */
				fh2m_inno_dmaengine_slave_config(chan, 0, (uint64_t)src, mapped_buffer->offset);
				tx = fh2m_inno_dmaengine_prep_slave_sg(chan, slave_sg_data->psSg, 0, 0);
			} else {
				fh2m_inno_dmaengine_slave_config(chan, 1, mapped_buffer->offset, (uint64_t)dst);
				tx = fh2m_inno_dmaengine_prep_slave_sg(chan, slave_sg_data->psSg, 1, 0);
			}

			fh2m_inno_mutex_unlock(pchan->chan_lock);

			/* flush the cache to the main memory */
			if (DMA_MEM2DEV == dir) {
				fh2m_inno_dma_sync_sg_for_device(dev, slave_sg_data->psSg);
			}
		}

		callback_param->dma_mapped_buffer = *mapped_buffer;
	}

	if (!tx) {
		ret = HAL_DMA_ERROR_INVALID_PARAMS;
		hal_dma_error(dev, "prepare dma transfer failed\n");
		goto e1;
	}

	fh2m_inno_dmaengine_set_tx_desc_cb(tx, hal_dma_callback, callback_param);

	callback_param->tx = tx;
	callback_param->dev = dev;
	callback_param->dir = dir;
	callback_param->chan = pchan;
	callback_param->src_addr = (uint64_t)src;
	callback_param->dst_addr = (uint64_t)dst;
	callback_param->chan_pool = chan_pool;
	callback_param->cleanup_param = cleanup_param;

	fh2m_inno_dmaengine_desc_set_reuse(tx);

	cleanup_param->callback_params[transfer_idx] = callback_param;

	return tx;

e1:
	fh2m_inno_kvfree(callback_param);
e0:
	return NULL;
}

static int hal_dma_submit_transfer(inno_dev* dev, inno_dma_chan* chan, inno_dma_async_tx_desc* tx)
{
	int ret = HAL_DMA_OK, cookie = -1;

	if (!dev || !chan || !tx) {
		ret = HAL_DMA_ERROR_INVALID_PARAMS;
		inno_error("[%s:%d]invalid params\n", __func__, __LINE__);
		goto e0;
	}

	cookie = fh2m_inno_dmaengine_submit(tx);
	if (cookie < 0) {
		ret = HAL_DMA_ERROR_INVALID_PARAMS;
		hal_dma_error(dev, "submit dma transfer failed\n");
		goto e0;
	}

	fh2m_inno_dma_async_issue_pending(chan);

e0:
	return ret;
}

int fh2m_hal_dma_transfer(inno_dev* dev, void** srcs, void** dsts, int* lens, int cnt, int dir, pfn_dma_func fast_cb, pfn_dma_func cb, void* cb_param)
{
	int ret = HAL_DMA_OK, i = 0;
	struct dev_rsrc* pdev_rsrc = NULL;
	inno_dma_async_tx_desc* tx = NULL;
	struct hal_dma_phys_chan* pchan = NULL;
	struct hal_dma_chan_pool* chan_pool = NULL;
	struct hal_dma_cleanup_param* cleanup_param = NULL;

	if (!dev || !srcs || !dsts || !lens || cnt == 0) {
		inno_error("[%s:%d]invaild param\n", __func__, __LINE__);
		return HAL_DMA_ERROR_INVALID_PARAMS;
	}

	/* FIXME:There is a high probability that this function has a performance bottleneck  */
	pdev_rsrc = (struct dev_rsrc*)fh2m_inno_rsrc_devres_find(dev);
	if (!pdev_rsrc) {
		ret = HAL_DMA_ERROR_INVALID_PARAMS;
		hal_dma_error(dev, "find device resource failed\n");
		goto e0;
	}
	chan_pool = pdev_rsrc->dma.chan_pool;

	cleanup_param = hal_dma_cleanup_param_alloc(dev, cnt, fast_cb, cb, cb_param);
	if (!cleanup_param) {
		ret = HAL_DMA_ERROR_NO_MEMORY;
		hal_dma_error(dev, "alloc memory failed\n");
		goto e1;
	}

	for (i = 0; i < cnt; i++) {
		pchan = hal_dma_request_channel(dev, chan_pool, srcs[i], dsts[i], lens[i], dir);
		if (!pchan) {
			ret = HAL_DMA_ERROR_CHAN_BUSY;
			hal_dma_error(dev, "failed to get a dma channel\n");
			goto loop_e0;
		}

		tx = hal_dma_prepare_transfer(dev, chan_pool, pchan, srcs[i], dsts[i], lens[i], dir, cleanup_param, i);
		if (tx == NULL) {
			ret = HAL_DMA_ERROR_PREPARE;
			hal_dma_error(dev, "failed to prepare dma transfer\n");
			goto loop_e1;
		}

		/* FIXME: Place here may cause an problem of mem leak during exception handling. */
		fh2m_inno_atomic_inc(&cleanup_param->ref_count);
		ret = hal_dma_submit_transfer(dev, pchan->chan, tx);
		if (ret != HAL_DMA_OK) {
			hal_dma_error(dev, "failed to submit dma transfer\n");
			goto loop_e1;
		}

		continue;

	loop_e1:
		hal_dma_release_channel(pdev_rsrc->dma.chan_pool, pchan);
	loop_e0:
		fh2m_inno_atomic_inc(&cleanup_param->err_ref_count);
		hal_dma_warn(dev, "transfer failed. (%d-%d)\n", i, ret);
		goto e1;
	}

	return HAL_DMA_OK;

e1:
	if (cleanup_param && fh2m_inno_atomic_read(&cleanup_param->ref_count) == 0 )
		hal_dma_cleanup_param_free(cleanup_param);
e0:
	return ret;
}

static int hal_dma_chan_alloc(inno_dev* dev, struct hal_dma_chan* chan, int phys_chan_cnt, int chan_type)
{
	int ret = 0, i = 0;
	if (dev == NULL || chan == NULL) {
		ret = -1;
		inno_error("[%s:%d]invalid params\n", __func__, __LINE__);
		goto e0;
	}

	chan->phys_chan_cnt = phys_chan_cnt;

	if (phys_chan_cnt == 0) {
		return 0;
	}

	chan->chan_list = (struct hal_dma_phys_chan*)fh2m_inno_kvmalloc_kernel(sizeof(struct hal_dma_phys_chan) * phys_chan_cnt);
	if (!chan->chan_list) {
		ret = -1;
		hal_dma_error(dev, "alloc hal_dma_phys_chan failed\n");
		goto e0;
	}
	fh2m_inno_memset(chan->chan_list, 0, sizeof(struct hal_dma_phys_chan) * phys_chan_cnt);

	chan->dma_type = chan_type;
#if(DRM_VERSION >= KERNEL_VERSION(4,14,0))
	chan->chan_root_cached = RB_ROOT_CACHED;
#else
	chan->chan_root = RB_ROOT;
#endif
	for (i = 0; i < phys_chan_cnt; i++) {
		chan->chan_list[i].chan = NULL;
		chan->chan_list[i].task_cnt = 0;
		chan->chan_list[i].dma_type = chan_type;
		chan->chan_list[i].chan_lock = fh2m_inno_mutex_alloc( );
		if (!chan->chan_list[i].chan_lock) {
			hal_dma_error(dev, "alloc hal_dma_phys_chan failed\n");
			goto e1;
		}
#if(DRM_VERSION >= KERNEL_VERSION(4,14,0))
		chan_insert(&chan->chan_root_cached, (chan->chan_list + i));
#else
		chan_insert(&chan->chan_root, (chan->chan_list + i));
#endif
	}

	return 0;

e1:
	for (i = 0; i < phys_chan_cnt; i++) {
		if (chan->chan_list[i].chan_lock) {
			fh2m_inno_mutex_free(chan->chan_list[i].chan_lock);
		}
	}

	fh2m_inno_kvfree(chan->chan_list);
e0:
	return ret;
}

static void hal_dma_chan_free(inno_dev *dev, struct hal_dma_chan* chan)
{
	int i = 0;

	if (!dev || !chan) {
		inno_error("[%s:%d]invalid params\n", __func__, __LINE__);
		return;
	}

	for (i = 0; i < chan->phys_chan_cnt; i++) {
		if (chan->chan_list[i].chan_lock) {
			fh2m_inno_mutex_free(chan->chan_list[i].chan_lock);
		}

		if (chan->chan_list[i].chan != NULL) {
			fh2m_inno_dma_release_channel(chan->chan_list[i].chan);
			chan->chan_list[i].chan = NULL;
		}

	}

	fh2m_inno_kvfree(chan->chan_list);
}

static void hal_dma_chan_release_callback(void *data)
{
	int i = 0, j = 0;
	int phys_chan_cnt;
	struct dev_rsrc* pdev_rsrc = (struct dev_rsrc*)data;
	struct hal_dma_chan* chan = NULL;
	struct hal_dma_chan_pool *pool = NULL;

	if (!data) {
		inno_error("[%s:%d]invalid params\n", __func__, __LINE__);
		return;
	}

	pool = pdev_rsrc->dma.chan_pool;

	if (pool->dma_total_load > 0)
		return;

	for (j = 0; j < HAL_DMA_CHAN_TYPE_MAX; j++) {
		chan = &pool->chans[j];
		if (!chan)
			continue;

		phys_chan_cnt = chan->phys_chan_cnt;
		for (i = 0; i < phys_chan_cnt; i++) {
			fh2m_inno_mutex_lock(chan->chan_list[i].chan_lock);
			if (chan->chan_list[i].chan != NULL) {
				if (chan->chan_list[i].task_cnt == 0) {
					fh2m_inno_dma_release_channel(chan->chan_list[i].chan);
					chan->chan_list[i].chan = NULL;
				}
			}
			fh2m_inno_mutex_unlock(chan->chan_list[i].chan_lock);
		}
	}
}

static struct hal_dma_chan_pool* hal_dma_chan_pool_init(struct dev_rsrc* pdev_rsrc)
{
	int i = 0, ret = 0;
	struct hal_dma_chan_pool* pool = NULL;

	if (!pdev_rsrc) {
		inno_error("[%s:%d]invalid params\n", __func__, __LINE__);
		goto e0;
	}

	pool = (struct hal_dma_chan_pool*)fh2m_inno_vmalloc(sizeof(struct hal_dma_chan_pool));
	if (!pool) {
		hal_dma_error(pdev_rsrc->dev, "failed to alloc dma channel pool\n");
		goto e0;
	}
	fh2m_inno_memset(pool, 0, sizeof(struct hal_dma_chan_pool));
	pool->dma_total_load = 0;
	pool->pool_lock = fh2m_inno_spinlock_alloc( );
	if (!pool->pool_lock) {
		hal_dma_error(pdev_rsrc->dev, "alloc dma pool_lock failed\n");
		goto e1;
	}

	pool->cleanup_wk_queue = fh2m_inno_create_workqueue("dma-cleanup-work");
	if (!pool->cleanup_wk_queue) {
		hal_dma_error(pdev_rsrc->dev, "create dma work queue failed\n");
		goto e2;
	}

	for (i = 0; i < HAL_DMA_CHAN_TYPE_MAX; i++) {
		ret = hal_dma_chan_alloc(pdev_rsrc->dev, &pool->chans[i], pdev_rsrc->dma.chans_info[i].chan_cnt, i);
		if (ret) {
			hal_dma_error(pdev_rsrc->dev, "alloc hal_dma_chan failed\n");
			goto e2;
		}
	}

	pool->dma_dwork = fh2m_inno_dwork_alloc(hal_dma_chan_release_callback, pdev_rsrc);
	pool->dwork_msec = DMA_POOL_DWORK_INTERVAL;

	return pool;

e2:
	for (i = 0; i < HAL_DMA_CHAN_TYPE_MAX; i++) {
		if (pool->chans[i].phys_chan_cnt != 0) {
			hal_dma_chan_free(pdev_rsrc->dev, &pool->chans[i]);
		}
	}

	fh2m_inno_spinlock_free(pool->pool_lock);
e1:
	fh2m_inno_vfree(pool);
e0:
	return NULL;
}

static void hal_dma_chan_pool_deinit(struct dev_rsrc* pdev_rsrc)
{
	int i = 0;
	struct hal_dma_chan_pool* pool = NULL;

	if (!pdev_rsrc) {
		inno_error("[%s:%d]invalid params\n", __func__, __LINE__);
		return;
	}

	pool = pdev_rsrc->dma.chan_pool;

	if (!pool) {
		hal_dma_error(pdev_rsrc->dev, "invalid params\n");
		return;
	}
	fh2m_inno_cancel_dwork_sync(pool->dma_dwork);

	for (i = 0; i < HAL_DMA_CHAN_TYPE_MAX; i++) {
		if (pool->chans[i].phys_chan_cnt != 0) {
		 	hal_dma_chan_free(pdev_rsrc->dev, &pool->chans[i]);
		 }
	}

	fh2m_inno_destroy_workqueue(pool->cleanup_wk_queue);
	fh2m_inno_spinlock_free(pool->pool_lock);
	fh2m_inno_vfree(pool);
}

int fh2m_hal_dma_resume(struct dev_rsrc* pdev_rsrc)
{
	int ret = 0;
#if 0
	int i, j;
	int phys_chan_cnt = 0;
	struct hal_dma_chan* chan;
	struct hal_dma_chan_pool* chan_pool;
	struct hal_dma_chan_filter_param filter_param;

	if (!pdev_rsrc || !pdev_rsrc->dma.chan_pool) {
		ret = -1;
		inno_error("[%s:%d]invalid params\n", __func__, __LINE__);
		goto e0;
	}

	chan_pool = pdev_rsrc->dma.chan_pool;

	/* makesure all dma channel stop */
	for (i = 0; i < HAL_DMA_CHAN_TYPE_MAX; i++) {
		filter_param.dev = pdev_rsrc->dev;
		filter_param.chan_type = i;
		chan = &chan_pool->chans[i];
		phys_chan_cnt = pdev_rsrc->dma.chans_info[i].chan_cnt;

		if ((phys_chan_cnt == 0) || !chan)
			continue;

		for (j = 0; j < phys_chan_cnt; j++) {
			chan->chan_list[j].chan = fh2m_inno_dma_request_channel(fh2m_inno_dma_cap_slave,
				hal_dma_channel_filter, &filter_param);
			if (!chan->chan_list[j].chan) {
				hal_dma_error(dev, "request channel failed (total-%d-%d)\n", phys_chan_cnt, i);
				goto e1;
			}
		}
	}
e1:
	/* nothing to do! */
e0:
#endif
	return ret;
}

int fh2m_hal_dma_suspend(struct dev_rsrc* pdev_rsrc)
{
	int ret = 0;
#ifdef ENABLE_DMA_INTERNAL_MANAGER_CHAN
	int i, j;
	int phys_chan_cnt = 0;
	struct hal_dma_chan* chan;
	struct hal_dma_chan_pool* chan_pool;
	struct hal_dma_chan_filter_param filter_param;

	if (!pdev_rsrc || !pdev_rsrc->dma.chan_pool) {
		ret = -1;
		inno_error("[%s:%d]invalid params\n", __func__, __LINE__);
		goto e0;
	}

	chan_pool = pdev_rsrc->dma.chan_pool;

	/* makesure all dma channel stop */
	for (i = 0; i < HAL_DMA_CHAN_TYPE_MAX; i++) {
		filter_param.dev = pdev_rsrc->dev;
		filter_param.chan_type = i;
		chan = &chan_pool->chans[i];
		phys_chan_cnt = pdev_rsrc->dma.chans_info[i].chan_cnt;

		if ((phys_chan_cnt == 0) || !chan)
			continue;

		for (j = 0; j < phys_chan_cnt; j++) {
			if (chan->chan_list[j].chan) {
				fh2m_inno_dma_release_channel(chan->chan_list[j].chan);
			}
			chan->chan_list[j].chan = NULL;
			chan->chan_list[j].task_cnt = 0;
		}
	}
e0:
#else
	hal_dma_chan_release_callback(pdev_rsrc);
#endif
	return ret;
}

int fh2m_hal_dma_init(struct dev_rsrc* pdev_rsrc)
{
	int ret = 0;

	if (!pdev_rsrc) {
		ret = -1;
		inno_error("[%s:%d]invalid params\n", __func__, __LINE__);
		goto e0;
	}

	if (pdev_rsrc->chip.dma_rsrc_init) {
		pdev_rsrc->chip.dma_rsrc_init(pdev_rsrc);
	} else {
		ret = -1;
		hal_dma_error(pdev_rsrc->dev, "dma_rsrcs_init callback null\n");
		goto e0;
	}

	pdev_rsrc->dma.chan_pool = hal_dma_chan_pool_init(pdev_rsrc);
	if (!pdev_rsrc->dma.chan_pool) {
		ret = -1;
		hal_dma_error(pdev_rsrc->dev, "dma channel pool init failed\n");
	}

e0:
	return ret;
}

void fh2m_hal_dma_deinit(struct dev_rsrc* pdev_rsrc)
{
	if (!pdev_rsrc) {
		inno_error("[%s:%d]invalid params\n", __func__, __LINE__);
		return;
	}

	hal_dma_chan_pool_deinit(pdev_rsrc);
}

