/*************************************************************************/ /*!
@File           pcie_dmac.h
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

#ifndef _PCIE_DMA_PLATFORM_H
#define _PCIE_DMA_PLATFORM_H

#include "utils.h"
#include "hal_interface.h"
#include "inno_dma.h"

struct pcie_dma_hcfg {
	u32	nr_channels;

	uint64_t dev_base_paddr;
	uint64_t cpu_base_paddr;
	uint64_t pcie_base_paddr;

	/**just for test*/
	resource_size_t dmam_base;
};

void* pcie_dmac_rsc_init(void *pdev);

void pcie_dmac_rsc_remove(void *data);

void pcie_dmac_desc_put(void *data);

void pcie_dma_descs_put_fast(void *data);

bool pcie_dmac_get_chan_tx_stat(void *chan);

void pcie_chan_block_xfer_start(void *data);

void pcie_dmac_dump_lli_info(void *chan_data, void *desc_data);

void pcie_dmac_dump_hw_info(void *data);

int pcie_dmac_chan_terminate(void *data);

int pcie_dmac_chan_pause(void *data);

int pcie_dmac_chan_resume(void *data);

int pcie_dmac_chan_is_available(void *data);

int pcie_dmac_chan_release(void *data);

int pcie_dmac_slave_config(void *data, void *config);

void* pcie_dmac_chan_prep_dma_memcpy(void *data, uint64_t dst_adr, uint64_t src_adr, size_t len);

void* pcie_dmac_chan_prep_slave_sg(void *data, inno_sglist *sgl, unsigned int sg_len, int dir);

uint64_t pcie_dmac_get_irq_status(void *data);

int pcie_dmac_irq_process(void *data, uint64_t int_status);

int pcie_dmac_irq_init(void *data, void (*handler_func)(void*));

void pcie_dmac_irq_remove(void *data);

int pcie_dmac_hw_init(void *data);

void pcie_dmac_hw_remove(void *data);

int pcie_dmac_runtime_resume(void *data);

int pcie_dmac_runtime_suspend(void *data);

int pcie_dmac_suspend(void *data);

int pcie_dmac_resume(void *data);

int pcie_dmac_get_chancnt(void *data);

void* pcie_dmac_get_dma_device(void *data);

void* pcie_dmac_get_platform_device(void *data);

void* pcie_dmac_get_chan(void *data);

void* pcie_dmac_get_timer(void *data);
#endif
