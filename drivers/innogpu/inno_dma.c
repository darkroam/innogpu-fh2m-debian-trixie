/*
* Copyright (c) Innosilicon Technology Ltd. All Rights Reserved
* Dual MIT/GPLv2
*
* The contents of this file are subject to the MIT license as set out below.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* Alternatively, the contents of this file may be used under the terms of
* the GNU General Public License Version 2 ("GPL") in which case the provisions
* of GPL are applicable instead of those above.
*
* If you wish to allow use of your version of this file only under the terms of
* GPL, and not to allow others to use your version of this file under the terms
* of the MIT license, indicate your decision by deleting the provisions above
* and replace them with the notice and other provisions required by GPL as set
* out in the file called "GPL-COPYING" included in this distribution. If you do
* not delete the provisions above, a recipient may use your version of this file
* under the terms of either the MIT license or GPL.
*
* This License is also included in this distribution in the file called
* "MIT-COPYING".
*
* EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
* PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
* BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
* PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
* COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
* IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
* CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include <linux/version.h>
#include <linux/device.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,18,0)
#include <linux/iosys-map.h>
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5,11,0)
#include <linux/dma-buf-map.h>
#endif
#if defined(CONFIG_ARM)
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,15,0))
 #include <asm/system.h>
#endif
 #include <asm/cacheflush.h>
#endif
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/dmapool.h>
#include "inno_misc.h"
#include "inno_dma.h"
#include "hal_interface.h"
#include "kernel_compat.h"

unsigned long fh2m_inno_dma_slave_config_size = sizeof(struct dma_slave_config);
INNO_EXT_SYM(fh2m_inno_dma_slave_config_size);

int fh2m_inno_dma_cap_slave = DMA_SLAVE;
INNO_EXT_SYM(fh2m_inno_dma_cap_slave);

int fh2m_inno_dma_cap_memcpy = DMA_MEMCPY;
INNO_EXT_SYM(fh2m_inno_dma_cap_memcpy);

int fh2m_inno_dma_cap_memset = DMA_MEMSET;
INNO_EXT_SYM(fh2m_inno_dma_cap_memset);

int fh2m_inno_dma_cap_memset_sg = DMA_MEMSET_SG;
INNO_EXT_SYM(fh2m_inno_dma_cap_memset_sg);

int fh2m_inno_dma_gddr2sys = DMA_DEV_TO_MEM;
INNO_EXT_SYM(fh2m_inno_dma_gddr2sys);

int fh2m_inno_dma_sys2gddr = DMA_MEM_TO_DEV;
INNO_EXT_SYM(fh2m_inno_dma_sys2gddr);

uint64_t fh2m_inno_get_dma_mask(void *dev)
{
	return *((struct device *)dev)->dma_mask;
}
INNO_EXT_SYM(fh2m_inno_get_dma_mask);

int fh2m_inno_dma_set_mask_64(void *dev)
{
	return dma_set_mask(dev, DMA_BIT_MASK(64));
}
INNO_EXT_SYM(fh2m_inno_dma_set_mask_64);

int fh2m_inno_dma_set_mask_32(void *dev)
{
	return dma_set_mask(dev, DMA_BIT_MASK(32));
}
INNO_EXT_SYM(fh2m_inno_dma_set_mask_32);

int fh2m_inno_dma_set_mask(void *dev, uint64_t mask)
{
	return dma_set_mask(dev, mask);
}
INNO_EXT_SYM(fh2m_inno_dma_set_mask);

int fh2m_inno_dma_get_ctl_flag(void)
{
	int flags;
	flags = DMA_CTRL_ACK | DMA_PREP_INTERRUPT;
	return flags;
}
INNO_EXT_SYM(fh2m_inno_dma_get_ctl_flag);

void *fh2m_inno_dma_map_page(inno_dev *dev, inno_page *pg, unsigned long pg_off,
						uint64_t align)
{
	return (void *)dma_map_page((struct device *)dev, (struct page *)pg, pg_off, align, 0);
}
INNO_EXT_SYM(fh2m_inno_dma_map_page);

void fh2m_inno_dma_unmap_page(inno_dev *dev, uint64_t addr, uint64_t align)
{
	dma_unmap_page((struct device *)dev, (dma_addr_t)addr, align, 0);
}
INNO_EXT_SYM(fh2m_inno_dma_unmap_page);

int fh2m_inno_dma_mapping_error(inno_dev *dev, u64 dma_addr)
{
	return dma_mapping_error((struct device *)dev, dma_addr);
}
INNO_EXT_SYM(fh2m_inno_dma_mapping_error);

int fh2m_inno_dmaengine_slave_config(void *chan, bool mem_to_dev,
		uint64_t src_addr, uint64_t dst_addr)
{
	struct dma_slave_config config = { 0 };
	config.direction = mem_to_dev ? DMA_MEM_TO_DEV : DMA_DEV_TO_MEM;
	config.src_addr  = src_addr;
	config.dst_addr  = dst_addr;
	return dmaengine_slave_config(chan, &config);
}
INNO_EXT_SYM(fh2m_inno_dmaengine_slave_config);

int fh2m_inno_dma_map_sg(inno_dev *dev, inno_sg_table *sg_table)
{
	return dma_map_sg((struct device *)dev, ((struct sg_table *)sg_table)->sgl,
			((struct sg_table *)sg_table)->nents,
			DMA_BIDIRECTIONAL);
}
INNO_EXT_SYM(fh2m_inno_dma_map_sg);

void fh2m_inno_dma_unmap_sg(inno_dev *dev, inno_sg_table *sg_table)
{
	dma_unmap_sg((struct device *)dev, ((struct sg_table *)sg_table)->sgl,
			((struct sg_table *)sg_table)->nents,
			DMA_BIDIRECTIONAL);
}
INNO_EXT_SYM(fh2m_inno_dma_unmap_sg);

void fh2m_inno_dma_sync_sg_for_device(inno_dev *dev, inno_sg_table *sg_table)
{
	dma_sync_sg_for_device(dev, ((struct sg_table *)sg_table)->sgl,
			((struct sg_table *)sg_table)->nents,
			DMA_BIDIRECTIONAL);
}
INNO_EXT_SYM(fh2m_inno_dma_sync_sg_for_device);

void fh2m_inno_dma_sync_sg_for_cpu(void *dev, inno_sg_table *sg_table)
{
	dma_sync_sg_for_cpu((struct device *)dev, ((struct sg_table *)sg_table)->sgl,
			((struct sg_table *)sg_table)->nents,
			DMA_BIDIRECTIONAL);
}
INNO_EXT_SYM(fh2m_inno_dma_sync_sg_for_cpu);

void fh2m_inno_dma_sync_single_for_cpu(void *dev, dma_addr_t handle, uint64_t size)
{
	dma_sync_single_for_cpu((struct device *)dev, handle, size,
			DMA_BIDIRECTIONAL);
}
INNO_EXT_SYM(fh2m_inno_dma_sync_single_for_cpu);

void fh2m_inno_dma_sync_single_for_device(void *dev, dma_addr_t handle, uint64_t size)
{
	dma_sync_single_for_device((struct device *)dev, *((dma_addr_t *)handle), size,
	                        DMA_BIDIRECTIONAL);
}
INNO_EXT_SYM(fh2m_inno_dma_sync_single_for_device);

inno_dma_async_tx_desc *fh2m_inno_dmaengine_prep_slave_sg(void *chan, inno_sg_table *sg_table,
		bool mem_to_dev, unsigned long flags)
{
	struct dma_async_tx_descriptor *desc = dmaengine_prep_slave_sg(chan,
			((struct sg_table *)sg_table)->sgl,
			((struct sg_table *)sg_table)->nents,
			mem_to_dev ? DMA_MEM_TO_DEV : DMA_DEV_TO_MEM, flags);
	return desc;
}
INNO_EXT_SYM(fh2m_inno_dmaengine_prep_slave_sg);

inno_dma_async_tx_desc *fh2m_inno_dmaengine_prep_memcpy(inno_dma_chan *chan, void *src, void *dst, int len)
{
	struct dma_async_tx_descriptor *tx = NULL;
	struct dma_device *dma_dev = ((struct dma_chan *)chan)->device;
	enum dma_ctrl_flags flags = DMA_CTRL_ACK | DMA_PREP_INTERRUPT;

	tx = dma_dev->device_prep_dma_memcpy((struct dma_chan *)chan,
										(dma_addr_t)dst, (dma_addr_t)src, len,
										flags);

	return (inno_dma_async_tx_desc *)tx;
}
INNO_EXT_SYM(fh2m_inno_dmaengine_prep_memcpy);

inno_dma_async_tx_desc *fh2m_inno_dmaengine_prep_memset(inno_dma_chan *chan, void *dest_addr, int value, uint64_t size)
{
	struct dma_async_tx_descriptor *tx = NULL;
	struct dma_device *dma_dev = ((struct dma_chan *)chan)->device;
	enum dma_ctrl_flags flags = DMA_CTRL_ACK | DMA_PREP_INTERRUPT;

	tx= dma_dev->device_prep_dma_memset((struct dma_chan *)chan,
									(dma_addr_t)dest_addr, value, size, flags);

	return (inno_dma_async_tx_desc *)tx;
}
INNO_EXT_SYM(fh2m_inno_dmaengine_prep_memset);

inno_dma_async_tx_desc *fh2m_inno_dmaengine_prep_memset_sg(inno_dma_chan *chan, inno_sg_table *sg_table, int value, int flags)
{
	struct dma_async_tx_descriptor *tx = NULL;
	struct dma_device *dma_dev = ((struct dma_chan *)chan)->device;

	tx = dma_dev->device_prep_dma_memset_sg((struct dma_chan *)chan, ((struct sg_table *)sg_table)->sgl,
											((struct sg_table *)sg_table)->nents, value, flags);

	return (inno_dma_async_tx_desc *)tx;
}
INNO_EXT_SYM(fh2m_inno_dmaengine_prep_memset_sg);

void fh2m_inno_dmaengine_set_tx_desc_cb(inno_dma_async_tx_desc *d, void (*cb)(void *), void *data)
{
	struct dma_async_tx_descriptor *desc = (struct dma_async_tx_descriptor *)d;
	desc->callback_param = data;
	desc->callback = cb;
}
INNO_EXT_SYM(fh2m_inno_dmaengine_set_tx_desc_cb);

int fh2m_inno_dmaengine_submit(inno_dma_async_tx_desc *desc)
{
	return dmaengine_submit((struct dma_async_tx_descriptor *)desc);
}
INNO_EXT_SYM(fh2m_inno_dmaengine_submit);

void fh2m_inno_dma_async_issue_pending(void *chan)
{
	dma_async_issue_pending(chan);
}
INNO_EXT_SYM(fh2m_inno_dma_async_issue_pending);

int fh2m_inno_dma_async_is_tx_complete(inno_dma_chan *chan, int cookie)
{
	enum dma_status status = DMA_ERROR;

	status = dma_async_is_tx_complete(chan, cookie, NULL, NULL);
	if (DMA_COMPLETE != status)
		return -1;

	return 0;
}
INNO_EXT_SYM(fh2m_inno_dma_async_is_tx_complete);

int fh2m_inno_dmaengine_terminate_sync(void *chan)
{
	return dmaengine_terminate_sync(chan);
}
INNO_EXT_SYM(fh2m_inno_dmaengine_terminate_sync);

inno_sg_table *fh2m_inno_sg_table_alloc(void)
{
	void *sg = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
	return sg;
}
INNO_EXT_SYM(fh2m_inno_sg_table_alloc);

void fh2m_inno_sg_table_free(inno_sg_table *sg)
{
	kfree(sg);
}
INNO_EXT_SYM(fh2m_inno_sg_table_free);

uint64_t fh2m_inno_sg_dma_address(inno_sglist *sg)
{
	return sg_dma_address((struct scatterlist *)sg);
}
INNO_EXT_SYM(fh2m_inno_sg_dma_address);

uint32_t fh2m_inno_sg_dma_len(inno_sglist *sg)
{
#if defined(PVR_ANDROID_ION_USE_SG_LENGTH)
	return ((struct scatterlist *)sg)->length;
#else
	return sg_dma_len((struct scatterlist *)sg);
#endif
}
INNO_EXT_SYM(fh2m_inno_sg_dma_len);

inno_sglist *fh2m_inno_sg_next(inno_sglist *sg)
{
	return sg_next((struct scatterlist *)sg);
}
INNO_EXT_SYM(fh2m_inno_sg_next);

inno_sglist *fh2m_inno_sg_first(inno_sg_table *table)
{
	return ((struct sg_table *)table)->sgl;
}
INNO_EXT_SYM(fh2m_inno_sg_first);

unsigned int fh2m_inno_sg_nents(inno_sg_table *table)
{
	return ((struct sg_table *)table)->nents;
}
INNO_EXT_SYM(fh2m_inno_sg_nents);

uint64_t fh2m_inno_sg_phys(inno_sglist *sg)
{
	return (uint64_t)sg_phys((struct scatterlist*)sg);
}
INNO_EXT_SYM(fh2m_inno_sg_phys);

void fh2m_inno_cache_flush(void *dev,
		void *start,
		void *end,
		phys_addr_t phys_start,
		phys_addr_t Phys_end)
{
#if defined(CONFIG_ARM)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0))
	arm_dma_ops.sync_single_for_device((struct device *)dev, phys_start, phys_end - phys_start, DMA_TO_DEVICE);
	arm_dma_ops.sync_single_for_cpu((struct device *)dev, phys_start, phys_end - phys_start, DMA_FROM_DEVICE);
#else	/* (LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)) */
	/* Inner cache */
	dmac_flush_range(start, end);

	/* Outer cache */
	outer_flush_range(phys_start, phys_end);
#endif	/* (LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)) */
#endif
}
INNO_EXT_SYM(fh2m_inno_cache_flush);

void fh2m_inno_cache_clean(void *dev,
		void *start,
		void *end,
		phys_addr_t phys_start,
		phys_addr_t phys_end)
{
#if defined(CONFIG_ARM)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0))
	arm_dma_ops.sync_single_for_device((struct device *)dev, phys_start, phys_end - phys_start, DMA_TO_DEVICE);
#else	/* (LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)) */
	/* Inner cache */
	dmac_map_area(start, pvr_dmac_range_len(start, end), DMA_TO_DEVICE);

	/* Outer cache */
	outer_clean_range(phys_start, phys_end);
#endif	/* (LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)) */
#endif
}
INNO_EXT_SYM(fh2m_inno_cache_clean);

void fh2m_inno_cache_invalidate(void *dev,
		void *start,
		void *end,
		phys_addr_t phys_start,
		phys_addr_t phys_end)
{
#if defined(CONFIG_ARM)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0))
	arm_dma_ops.sync_single_for_cpu((struct device *)dev, phys_start, phys_end - phys_start, DMA_FROM_DEVICE);
#else	/* (LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)) */
	/* Inner cache */
	dmac_map_area(start, pvr_dmac_range_len(start, end), DMA_FROM_DEVICE);

	/* Outer cache */
	outer_inv_range(phys_start, phys_end);
#endif	/* (LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)) */
#endif
}
INNO_EXT_SYM(fh2m_inno_cache_invalidate);

static bool inno_hal_dma_filter(struct dma_chan *chan, void *param)
{
#if !defined(NO_HARDWARE)
	// return hal_dma_filter(param, chan->device->dev, chan->private, chan->chan_id);
	return false;
#else
	return false;
#endif
}

inno_dma_chan *fh2m_inno_request_hal_dma_chan(int dma_cap, void *dma_para)
{
	struct dma_chan *chan;
	dma_cap_mask_t mask;

	dma_cap_zero(mask);
	dma_cap_set(dma_cap, mask);

	chan = dma_request_channel(mask, inno_hal_dma_filter, dma_para);
	if (IS_ERR(chan)) {
		pr_err("%s: fail(%ld)\n", __func__, (long int)PTR_ERR(chan));
		return NULL;
	}
	return chan;
}
INNO_EXT_SYM(fh2m_inno_request_hal_dma_chan);

static bool inno_dma_chan_filter(struct dma_chan *chan, void *param)
{
	struct inno_dma_chan_filter_param *filter_param = (struct inno_dma_chan_filter_param *)param;

	if (!filter_param)
		return false;

#if defined(NO_HARDWARE)
	return false;
#endif

	return filter_param->pfn_filter(chan->device->dev, chan->chan_id, chan->private, filter_param->params);
}

inno_dma_chan* fh2m_inno_dma_request_channel(int dma_cap, pfn_filter filter, void *params)
{
	dma_cap_mask_t mask;
	struct inno_dma_chan_filter_param filter_param;

	dma_cap_zero(mask);
	dma_cap_set(dma_cap, mask);

	filter_param.pfn_filter = filter;
	filter_param.params  = params;

	return dma_request_channel(mask, inno_dma_chan_filter, (void *)&filter_param);
}
INNO_EXT_SYM(fh2m_inno_dma_request_channel);

void fh2m_inno_dma_release_channel(inno_dma_chan *chan)
{
	struct dma_chan *dchan = (struct dma_chan *)chan;
	if (!dchan->client_count)
		return;

	dma_release_channel(dchan);
}
INNO_EXT_SYM(fh2m_inno_dma_release_channel);

void fh2m_inno_dmaengine_terminate_all(inno_dma_chan *chan)
{
	dmaengine_terminate_all((struct dma_chan *)chan);
}
INNO_EXT_SYM(fh2m_inno_dmaengine_terminate_all);

const char *fh2m_inno_dma_chan_name(inno_dma_chan *chan)
{
	return dma_chan_name((struct dma_chan *)chan);
}
INNO_EXT_SYM(fh2m_inno_dma_chan_name);

inno_dma_pool *fh2m_inno_dmam_pool_create(const char *name, void *dev, uint64_t size,
									uint64_t align, uint64_t allocation)
{
	return (inno_dma_pool *)dmam_pool_create(name, (struct device *)dev, size, align, allocation);
}
INNO_EXT_SYM(fh2m_inno_dmam_pool_create);

uint64_t fh2m_inno_dma_slave_config_get_dst(inno_dma_slave_config *cfg)
{
	return ((struct dma_slave_config *)cfg)->dst_addr;
}
INNO_EXT_SYM(fh2m_inno_dma_slave_config_get_dst);

uint64_t fh2m_inno_dma_slave_config_get_src(inno_dma_slave_config *cfg)
{
	return ((struct dma_slave_config *)cfg)->src_addr;
}
INNO_EXT_SYM(fh2m_inno_dma_slave_config_get_src);

uint64_t fh2m_INNO_DMA_BIT_MASK(int n)
{
	return DMA_BIT_MASK(n);
}
INNO_EXT_SYM(fh2m_INNO_DMA_BIT_MASK);

inno_dma_device *fh2m_inno_dma_chan_get_dma_device(inno_dma_chan *chan)
{
	return (inno_dma_device *)((struct dma_chan *)chan)->device;
}
INNO_EXT_SYM(fh2m_inno_dma_chan_get_dma_device);

void *fh2m_inno_alloc_dma_device(void *dev)
{
	struct dma_device *dma_dev;

	dma_dev = devm_kmalloc((struct device *)dev, sizeof(struct dma_device), GFP_KERNEL);
	memset(dma_dev, 0, sizeof(struct dma_device));

	return (void *)dma_dev;
}
INNO_EXT_SYM(fh2m_inno_alloc_dma_device);

void fh2m_inno_free_dma_device(void *dev, void *addr)
{
	devm_kfree((struct device *)dev, addr);
}
INNO_EXT_SYM(fh2m_inno_free_dma_device);

void *fh2m_inno_alloc_dma_slave_config(void *dev)
{
	struct dma_slave_config *slave;

	slave = devm_kmalloc((struct device *)dev, sizeof(struct dma_slave_config), GFP_KERNEL);
	memset(slave, 0, sizeof(struct dma_slave_config));

	return (void *)slave;
}
INNO_EXT_SYM(fh2m_inno_alloc_dma_slave_config);

void fh2m_inno_free_dma_slave_config(void *dev, void *addr)
{
	devm_kfree((struct device *)dev, addr);
}
INNO_EXT_SYM(fh2m_inno_free_dma_slave_config);

struct list_head* fh2m_inno_get_dma_channels(void *dma_dev)
{
	return (struct list_head *)(&((struct dma_device *)dma_dev)->channels);
}
INNO_EXT_SYM(fh2m_inno_get_dma_channels);

int fh2m_inno_is_dma_mem2dev(int dir)
{
	return dir == DMA_MEM_TO_DEV;
}
INNO_EXT_SYM(fh2m_inno_is_dma_mem2dev);

int fh2m_inno_os_sg_alloc_table(inno_sg_table *table, unsigned int nents)
{
	return fh2m_os_sg_alloc_table(table, nents, GFP_KERNEL);
}
INNO_EXT_SYM(fh2m_inno_os_sg_alloc_table);

void fh2m_inno_os_sg_free_table(inno_sg_table *table)
{
	fh2m_os_sg_free_table(table);
}
INNO_EXT_SYM(fh2m_inno_os_sg_free_table);

inno_sglist *fh2m_inno_sgt_list_get(inno_sg_table *table)
{
	return ((struct sg_table *)table)->sgl;
}
INNO_EXT_SYM(fh2m_inno_sgt_list_get);

unsigned int fh2m_inno_sgt_nets_get(inno_sg_table *table)
{
	return ((struct sg_table *)table)->nents;
}
INNO_EXT_SYM(fh2m_inno_sgt_nets_get);

void fh2m_inno_sgt_nents_set(inno_sg_table *table, unsigned int nents)
{
	((struct sg_table *)table)->nents = nents;
}
INNO_EXT_SYM(fh2m_inno_sgt_nents_set);

unsigned int fh2m_inno_sgt_orig_nents_get(inno_sg_table *table)
{
	return ((struct sg_table *)table)->orig_nents;
}
INNO_EXT_SYM(fh2m_inno_sgt_orig_nents_get);

unsigned int fh2m_inno_sg_nents_by_sgl(inno_sglist *sg)
{
	return sg_nents((struct scatterlist*)sg);
}
INNO_EXT_SYM(fh2m_inno_sg_nents_by_sgl);

inno_page *fh2m_inno_sgl_get_page(inno_sglist *sg)
{
	return sg_page((struct scatterlist*)sg);
}
INNO_EXT_SYM(fh2m_inno_sgl_get_page);

enum dma_data_direction fh2m_inno_get_dma_direction(enum inno_dma_data_direction dir)
{
	switch (dir) {
	case INNO_DMA_TO_DEVICE:
		return DMA_TO_DEVICE;
	case INNO_DMA_FROM_DEVICE:
		return DMA_FROM_DEVICE;
	case INNO_DMA_BIDIRECTIONAL:
		return DMA_BIDIRECTIONAL;
	default:
		return DMA_NONE;
	}
}
INNO_EXT_SYM(fh2m_inno_get_dma_direction);

void fh2m_inno_dma_sync_sg_for_cpu_with_dir(inno_dev *dev, inno_sglist *sgl, int nelems, enum inno_dma_data_direction dir)
{
	dma_sync_sg_for_cpu((struct device *)dev, (struct scatterlist *)sgl, nelems, fh2m_inno_get_dma_direction(dir));
}
INNO_EXT_SYM(fh2m_inno_dma_sync_sg_for_cpu_with_dir);

void fh2m_inno_dma_sync_sg_for_device_with_dir(inno_dev *dev, inno_sglist *sgl, int nelems, enum inno_dma_data_direction dir)
{
	dma_sync_sg_for_device((struct device *)dev, (struct scatterlist *)sgl, nelems, fh2m_inno_get_dma_direction(dir));
}
INNO_EXT_SYM(fh2m_inno_dma_sync_sg_for_device_with_dir);

int fh2m_inno_dma_map_sg_attrs(inno_dev *dev, inno_sglist *sg, int nents, enum inno_dma_data_direction dir, unsigned long attrs)
{
	return dma_map_sg_attrs((struct device *)dev, (struct scatterlist *)sg, nents, fh2m_inno_get_dma_direction(dir), attrs);
}
INNO_EXT_SYM(fh2m_inno_dma_map_sg_attrs);

void fh2m_inno_dma_unmap_sg_attrs(inno_dev *dev, inno_sglist *sg, int nents, enum inno_dma_data_direction dir, unsigned long attrs)
{
	dma_unmap_sg_attrs((struct device *)dev, (struct scatterlist *)sg, nents, fh2m_inno_get_dma_direction(dir), attrs);
}
INNO_EXT_SYM(fh2m_inno_dma_unmap_sg_attrs);

void fh2m_inno_sgl_set_page(inno_sglist *sg, inno_page *page, unsigned int len, unsigned int offset)
{
	sg_set_page((struct scatterlist*)sg, (struct page*)page, len, offset);
}
INNO_EXT_SYM(fh2m_inno_sgl_set_page);

void fh2m_inno_sgl_set_dma_address(inno_sglist *sgl, u64 addr)
{
	sg_dma_address((struct scatterlist * )sgl) = addr;
}
INNO_EXT_SYM(fh2m_inno_sgl_set_dma_address);

void fh2m_inno_sgl_set_dma_len(inno_sglist *sgl, unsigned int len)
{
	sg_dma_len((struct scatterlist * )sgl) = len;
}
INNO_EXT_SYM(fh2m_inno_sgl_set_dma_len);

dma_addr_t fh2m_inno_sgl_phys(inno_sglist *sg)
{
	return sg_phys((struct scatterlist *)sg);
}
INNO_EXT_SYM(fh2m_inno_sgl_phys);

uint32_t fh2m_inno_sgl_len(inno_sglist *sg)
{
	return ((struct scatterlist *)sg)->length;
}
INNO_EXT_SYM(fh2m_inno_sgl_len);

uint32_t fh2m_inno_sgl_offset(inno_sglist *sg)
{
	return ((struct scatterlist *)sg)->offset;
}
INNO_EXT_SYM(fh2m_inno_sgl_offset);

void fh2m_inno_map_current_lock(void)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 8, 0)
	down_read(&current->mm->mmap_sem);
#else
	struct mm_struct *mm = current->mm;
	mmap_read_lock(mm);
#endif
}
INNO_EXT_SYM(fh2m_inno_map_current_lock);

void fh2m_inno_map_current_unlock(void)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 8, 0)
	up_read(&current->mm->mmap_sem);
#else
	struct mm_struct *mm = current->mm;
	mmap_read_unlock(mm);
#endif
}
INNO_EXT_SYM(fh2m_inno_map_current_unlock);

u64 fh2m_inno_dma_get_mask(inno_dev *dev)
{
	return dma_get_mask((struct device *)dev);
}
INNO_EXT_SYM(fh2m_inno_dma_get_mask);

void fh2m_inno_map_set_vaddr(void* map, void* kptr)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,18,0)
	iosys_map_set_vaddr((struct iosys_map*)map, kptr);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5,11,0)
	dma_buf_map_set_vaddr((struct dma_buf_map*)map, kptr);
#endif
}
INNO_EXT_SYM(fh2m_inno_map_set_vaddr);

int fh2m_inno_get_dma_chan_id(inno_dma_chan *chan)
{
	return ((struct dma_chan*)chan)->chan_id;
}
INNO_EXT_SYM(fh2m_inno_get_dma_chan_id);

bool fh2m_inno_dmaengine_desc_test_reuse(inno_dma_async_tx_desc *tx)
{
	return dmaengine_desc_test_reuse((struct dma_async_tx_descriptor *)tx);
}
INNO_EXT_SYM(fh2m_inno_dmaengine_desc_test_reuse);

int fh2m_inno_dmaengine_desc_set_reuse(inno_dma_async_tx_desc *tx)
{
	return dmaengine_desc_set_reuse((struct dma_async_tx_descriptor *)tx);
}
INNO_EXT_SYM(fh2m_inno_dmaengine_desc_set_reuse);

void fh2m_inno_dmaengine_desc_clear_reuse(inno_dma_async_tx_desc *tx)
{
	dmaengine_desc_clear_reuse((struct dma_async_tx_descriptor *)tx);
}
INNO_EXT_SYM(fh2m_inno_dmaengine_desc_clear_reuse);
