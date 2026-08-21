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
#ifndef __INNO_DMA_H__
#define __INNO_DMA_H__

#include <linux/types.h>
#include "inno_plat_dev.h"
#include "inno_mm.h"
#include "hal_interface.h"

typedef void inno_dma_async_tx_desc;
typedef void inno_sg_table;
typedef void inno_sglist;
typedef void inno_dma_device;
typedef void inno_dma_chan;
typedef void inno_dma_pool;
typedef void inno_dma_slave_config;

typedef bool (*pfn_filter)(inno_dev* dev, int chan_id, void *chan_private, void *params);

extern unsigned long fh2m_inno_dma_slave_config_size;

extern int fh2m_inno_dma_cap_slave;
extern int fh2m_inno_dma_cap_memcpy;
extern int fh2m_inno_dma_cap_memset;
extern int fh2m_inno_dma_cap_memset_sg;

extern int fh2m_inno_dma_gddr2sys;
extern int fh2m_inno_dma_sys2gddr;

enum inno_dma_data_direction {
	INNO_DMA_BIDIRECTIONAL,
	INNO_DMA_TO_DEVICE,
	INNO_DMA_FROM_DEVICE,
	INNO_DMA_NONE,
};

struct inno_dma_chan_filter_param
{
	void *params;
	bool (*pfn_filter)(inno_dev *dev, int chan_id, void *chan_private, void *params);
};

uint64_t fh2m_inno_get_dma_mask(void *dev);

int fh2m_inno_dma_set_mask_64(void *dev);

int fh2m_inno_dma_set_mask_32(void *dev);

int fh2m_inno_dma_get_ctl_flag(void);

inno_sg_table *fh2m_inno_sg_table_alloc(void);

void fh2m_inno_sg_table_free(inno_sg_table *sg);

uint64_t fh2m_inno_sg_dma_address(inno_sglist *sg);

uint32_t fh2m_inno_sg_dma_len(inno_sglist *sg);

int fh2m_inno_dmaengine_slave_config(void *chan, bool mem_to_dev, uint64_t src_addr, uint64_t dst_addr);

void *fh2m_inno_dma_map_page(inno_dev *dev, inno_page *pg, unsigned long pg_off,
						uint64_t align);

void fh2m_inno_dma_unmap_page(inno_dev *dev, uint64_t addr, uint64_t align);

int fh2m_inno_dma_mapping_error(inno_dev *dev, u64 dma_addr);

int fh2m_inno_dma_map_sg(inno_dev *dev, inno_sg_table *sg_table);

void fh2m_inno_dma_unmap_sg(inno_dev *dev, inno_sg_table *sg_table);

void fh2m_inno_dma_sync_sg_for_device(inno_dev *dev, inno_sg_table *sg_table);

void fh2m_inno_dma_sync_sg_for_cpu(inno_dev *dev, inno_sg_table *sg_table);

void fh2m_inno_dma_sync_single_for_cpu(void *dev, dma_addr_t handle, uint64_t size);

void fh2m_inno_dma_sync_single_for_device(void *dev, dma_addr_t handle, uint64_t size);

inno_dma_async_tx_desc *fh2m_inno_dmaengine_prep_slave_sg(void *chan, inno_sg_table *sg_table,
		bool mem_to_dev, unsigned long flags);

inno_dma_async_tx_desc *fh2m_inno_dmaengine_prep_memcpy(inno_dma_chan *chan, void *src, void *dst, int len);

inno_dma_async_tx_desc *fh2m_inno_dmaengine_prep_memset(inno_dma_chan *chan, void *dest_addr, int value, uint64_t len);

inno_dma_async_tx_desc *fh2m_inno_dmaengine_prep_memset_sg(inno_dma_chan *chan, inno_sg_table *sg_table, int value, int flags);

void fh2m_inno_dmaengine_set_tx_desc_cb(inno_dma_async_tx_desc *desc, void (*cb)(void *), void *data);

int fh2m_inno_dmaengine_submit(inno_dma_async_tx_desc *desc);

void fh2m_inno_dma_async_issue_pending(void *chan);

int fh2m_inno_dma_async_is_tx_complete(inno_dma_chan *chan, int cookie);

int fh2m_inno_dmaengine_terminate_sync(void *chan);

inno_sglist *fh2m_inno_sg_next(inno_sglist *sg);

inno_sglist *fh2m_inno_sg_first(inno_sg_table *table);

unsigned int fh2m_inno_sg_nents(inno_sg_table *table);

uint64_t fh2m_inno_sg_phys(inno_sglist *sg);

void fh2m_inno_cache_flush(void *dev,
		void *start,
		void *end,
		phys_addr_t phys_start,
		phys_addr_t phys_end);

void fh2m_inno_cache_clean(void *dev,
		void *start,
		void *end,
		phys_addr_t phys_start,
		phys_addr_t phys_end);

void fh2m_inno_cache_invalidate(void *dev,
		void *start,
		void *end,
		phys_addr_t phys_start,
		phys_addr_t phys_end);

#define inno_for_each_sg(sglist, sg, nr, __i) \
	for (__i = 0, sg = (sglist); __i < (nr); __i++, sg = fh2m_inno_sg_next(sg))

inno_dma_chan *fh2m_inno_request_hal_dma_chan(int dma_cap, void *param);

inno_dma_chan* fh2m_inno_dma_request_channel(int dma_cap, pfn_filter filter, void *params);

void fh2m_inno_dma_release_channel(inno_dma_chan *chan);

void fh2m_inno_dmaengine_terminate_all(inno_dma_chan *chan);

const char *fh2m_inno_dma_chan_name(inno_dma_chan *chan);

inno_dma_pool *fh2m_inno_dmam_pool_create(const char *name, void *dev, uint64_t size,
									uint64_t align, uint64_t allocation);

uint64_t fh2m_inno_dma_slave_config_get_dst(inno_dma_slave_config *cfg);

uint64_t fh2m_inno_dma_slave_config_get_src(inno_dma_slave_config *cfg);

uint64_t fh2m_INNO_DMA_BIT_MASK(int n);

void *fh2m_inno_alloc_dma_device(void *dev);

void fh2m_inno_free_dma_device(void *dev, void *addr);

void *fh2m_inno_alloc_dma_slave_config(void *dev);

void fh2m_inno_free_dma_slave_config(void *dev, void *addr);

struct list_head* fh2m_inno_get_dma_channels(void *dma_dev);

int fh2m_inno_is_dma_mem2dev(int dir);

#define INNO_DMA_MAP_SG_ATTRS(d, s, n, r) fh2m_inno_dma_map_sg_attrs(d, s, n, r, 0)

#define INNO_DMA_UNMAP_SG_ATTRS(d, s, n, r) fh2m_inno_dma_unmap_sg_attrs(d, s, n, r, 0)

int fh2m_inno_os_sg_alloc_table(inno_sg_table *table, unsigned int nents);

void fh2m_inno_os_sg_free_table(inno_sg_table *table);

inno_sglist *fh2m_inno_sgt_list_get(inno_sg_table *table);

unsigned int fh2m_inno_sgt_nets_get(inno_sg_table *table);

void fh2m_inno_sgt_nents_set(inno_sg_table *table, unsigned int nents);

unsigned int fh2m_inno_sgt_orig_nents_get(inno_sg_table *table);

unsigned int fh2m_inno_sg_nents_by_sgl(inno_sglist *sg);

inno_page *fh2m_inno_sgl_get_page(inno_sglist *sg);

enum dma_data_direction fh2m_inno_get_dma_direction(enum inno_dma_data_direction dir);

void fh2m_inno_dma_sync_sg_for_cpu_with_dir(inno_dev *dev, inno_sglist *sgl, int nelems, enum inno_dma_data_direction dir);

void fh2m_inno_dma_sync_sg_for_device_with_dir(inno_dev *dev, inno_sglist *sgl, int nelems, enum inno_dma_data_direction dir);

int fh2m_inno_dma_map_sg_attrs(inno_dev *dev, inno_sglist *sg, int nents, enum inno_dma_data_direction dir, unsigned long attrs);

void fh2m_inno_dma_unmap_sg_attrs(inno_dev *dev, inno_sglist *sg, int nents, enum inno_dma_data_direction dir, unsigned long attrs);

void fh2m_inno_sgl_set_page(inno_sglist *sg, inno_page *page, unsigned int len, unsigned int offset);

void fh2m_inno_sgl_set_dma_address(inno_sglist *sgl, u64 addr);

void fh2m_inno_sgl_set_dma_len(inno_sglist *sgl, unsigned int len);

dma_addr_t fh2m_inno_sgl_phys(inno_sglist *sg);

uint32_t fh2m_inno_sgl_len(inno_sglist *sg);

uint32_t fh2m_inno_sgl_offset(inno_sglist *sg);

void fh2m_inno_map_set_vaddr(void* map, void* kptr);

void fh2m_inno_map_current_lock(void);

void fh2m_inno_map_current_unlock(void);

u64 fh2m_inno_dma_get_mask(inno_dev *dev);

int fh2m_inno_dma_set_mask(void *dev, uint64_t mask);

int fh2m_inno_get_dma_chan_id(inno_dma_chan * chan);

bool fh2m_inno_dmaengine_desc_test_reuse(inno_dma_async_tx_desc *tx);

int fh2m_inno_dmaengine_desc_set_reuse(inno_dma_async_tx_desc *tx);

void fh2m_inno_dmaengine_desc_clear_reuse(inno_dma_async_tx_desc *tx);

inno_dma_device *fh2m_inno_dma_chan_get_dma_device(inno_dma_chan *chan);
#endif
