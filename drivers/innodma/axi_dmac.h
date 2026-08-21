/*************************************************************************/ /*!
@File           axi_dmac.h
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

#ifndef _AXI_DMA_PLATFORM_H
#define _AXI_DMA_PLATFORM_H

#include "hal.h"
#include "utils.h"
#include "hal_interface.h"

#define DMAC_MAX_CHANNELS   16
#define DMA_ALIGN 5

#define AXI_DMA_LT 0
#define AXI_DMA_RT 1

struct axi_dma_hcfg {
	u32 nr_channels;
	u32 nr_masters;
	u32 m_data_width;
	u32 block_size[DMAC_MAX_CHANNELS];
	u32 priority[DMAC_MAX_CHANNELS];
	u32 axi_rw_burst_len; /* maximum supported axi burst length */
	bool restrict_axi_burst_len;
	resource_size_t dmam_base; /**just for test*/
	int axi_dma_pos;

	/** g3 add */
	u32 chan_irq_id;
	u32 cmn_irq_id;
	u32 reg_module;
	u8 ctrl_id;
};

void* axi_dma_rsc_init(void *pdev);

void axi_dmac_rsc_remove(void *chip);

void axi_dmac_desc_put(void *data);

void axi_dmac_dump_lli_info(void *chan_data, void *desc_data);

void axi_dmac_dump_hw_info(void *data);

void axi_dmac_chan_block_xfer_start(void *data);

bool axi_dmac_get_chan_tx_stat(void *data);

int axi_dmac_chan_terminate(void *data);

int axi_dmac_chan_pause(void *data);

int axi_dmac_chan_resume(void *data);

int axi_dmac_chan_is_available(void *data);

int axi_dmac_chan_release(void *data);

void* axi_dmac_chan_prep_dma_memcpy(void *data, uint64_t dst_adr, uint64_t src_adr, size_t len);

void* axi_dmac_chan_prep_slave_sg(void *data, inno_sglist *sgl, unsigned int sg_len, int dir);

void* axi_dmac_chan_prep_dma_memset(void *data, uint64_t dest, int val, size_t len);

void* axi_dmac_chan_prep_dma_memset_sg(void *data, inno_sglist *sgl, unsigned int nents, int val);

int axi_dmac_slave_config(void *data, void *config);

int axi_dmac_irq_init(void *data, void (*handler_func)(void*));

void axi_dmac_irq_remove(void *data);

void axi_dmac_runtime_resume(void *data);

void axi_dmac_runtime_suspend(void *data);

int axi_dmac_get_irq_status(void *data);

int axi_dmac_chan_irq_process(void *data);

int axi_dmac_hw_init(void *data);

void axi_dmac_hw_remove(void *data);

int axi_dmac_suspend(void *data);

int axi_dmac_resume(void *data);

void* axi_dmac_get_platform_device(void *data);

void* axi_dmac_chan_get_platform_device(void *data);

void* axi_dmac_get_dma_device(void *data);

int axi_dmac_get_chancnt(void *data);

void* axi_dmac_get_chan(void *data);

void axi_dmac_chan_disable(void *data);

int axi_dmac_get_pos(void *data);

void* axi_dmac_get_timer(void *data);

#endif /* _AXI_DMA_PLATFORM_H */
