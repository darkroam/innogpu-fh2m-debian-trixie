/*************************************************************************/ /*!
@File           inno_axi_dma_drv.c
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
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dmaengine.h>
#include <linux/dmapool.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/property.h>
#include <linux/types.h>
#include <linux/slab.h>

#include "innodma_debug.h"
#include "axi_dmac.h"
#include "inno_axi_dma_drv.h"
#include "hal_interface.h"
#include "utils.h"
#include "virt_dma.h"
#include "inno_task.h"

#define DMA_DRV_NAME	"inno-dma"
/*
 * The set of bus widths supported by the DMA controller. AXI DMAC supports
 * master data bus width up to 512 bits (for both AXI master interfaces), but
 * it depends on IP block configurarion.
 */
#define AXI_DMA_BUSWIDTHS	DMA_SLAVE_BUSWIDTH_32_BYTES

struct axi_dma_chan {
	void   *private;
	unsigned long time;
	/* these other elements are all protected by vc.lock */
	struct virt_dma_chan    vc;
};

struct axi_dma_desc
{
	void   *private;
	struct virt_dma_desc vd;
	struct axi_dma_chan  *chan;
};

static void inno_axi_dma_chan_handle_err(struct axi_dma_chan *chan);
static void inno_axi_dma_timeout_callback(unsigned long data);

static inline struct device *dchan2dev(struct dma_chan *dchan)
{
	return &dchan->dev->device;
}

static inline struct device *chan2dev(struct axi_dma_chan *chan)
{
	return &chan->vc.chan.dev->device;
}

static inline struct axi_dma_desc *vd_to_axi_desc(struct virt_dma_desc *vd)
{
	return container_of(vd, struct axi_dma_desc, vd);
}

static inline struct axi_dma_chan *vc_to_axi_dma_chan(struct virt_dma_chan *vc)
{
	return container_of(vc, struct axi_dma_chan, vc);
}

static inline struct axi_dma_chan *dchan_to_axi_dma_chan(struct dma_chan *dchan)
{
	return vc_to_axi_dma_chan(innodma_to_virt_chan(dchan));
}

static inline const char *axi_chan_name(struct axi_dma_chan *chan)
{
	return fh2m_inno_dma_chan_name(&(chan->vc.chan));
}

void* inno_axi_dma_alloc_chan(void *dev, int chancnt)
{
	return (void *)devm_kcalloc((struct device *)dev, chancnt, sizeof(struct axi_dma_chan), GFP_KERNEL);
}

void inno_axi_dma_set_chan_private(void *chan_ptr, int chanidx, void *data)
{
	struct axi_dma_chan *chan = ((struct axi_dma_chan *)chan_ptr + chanidx);
	chan->private = data;
}

void* inno_axi_dma_get_chan_private(void *chan_ptr, int chanidx)
{
	struct axi_dma_chan *chan = ((struct axi_dma_chan *)chan_ptr + chanidx);
	return chan->private;
}

static void* inno_axi_dma_rsc_init(struct platform_device *pdev)
{
	return (void *)axi_dma_rsc_init((void *)pdev);
}

static struct axi_dma_desc* inno_axi_dma_desc_alloc(void)
{
	struct axi_dma_desc *desc;

	desc = kmalloc(sizeof(struct axi_dma_desc), GFP_KERNEL);
	if (!desc)
		return NULL;

	memset(desc, 0, sizeof(struct axi_dma_desc));

	return desc;
}

static void inno_axi_dma_desc_free(struct virt_dma_desc *vd)
{
	struct axi_dma_desc *desc = vd_to_axi_desc(vd);

	axi_dmac_desc_put(desc->private);
	kfree(desc);
}

static void inno_axi_dma_timeout_process(struct axi_dma_chan *chan)
{
	bool is_reuse = false;
	struct virt_dma_desc *vd;

	innodma_warn(chan2dev(chan), "axi-dma transfer timeout.\n");

	vd = innodma_vchan_next_desc(&chan->vc);
	if (!vd)
		return;

	is_reuse = dmaengine_desc_test_reuse(&vd->tx);
	if (is_reuse) {
		dmaengine_terminate_all(&chan->vc.chan);
		dmaengine_desc_clear_reuse(&vd->tx);
		dmaengine_submit(&vd->tx);
		dma_async_issue_pending(&chan->vc.chan);
		innodma_warn(chan2dev(chan), "axi-dma transfer timeout.retry xfer!\n");
	}
	else {
		inno_axi_dma_chan_handle_err(chan);
		innodma_error(chan2dev(chan), "axi-dma transfer timeout.handler err!\n");
	}
}

static void inno_axi_dma_timeout_callback(unsigned long data)
{
	int i = 0, chancnt = 0;
	struct axi_dma_chan *chanbase = NULL, *chan = NULL;
	struct virt_dma_chan *vc;
	struct inno_timer *axi_dma_timer = NULL;
	unsigned long timeout;
	chanbase = axi_dmac_get_chan((void *)data);
	chancnt = axi_dmac_get_chancnt((void *)data);
	axi_dma_timer = axi_dmac_get_timer((void *)data);

	for (i = 0;i < chancnt;i++)
	{
		chan = chanbase + i;
		vc = &chan->vc;
		if (innodma_vchan_is_busy(vc))
		{
			timeout = chan->time + fh2m_inno_msecs_to_jiffies(DMA_TRANSFER_TIMEOUT);
			if (fh2m_inno_time_after(timeout))
				inno_axi_dma_timeout_process(chan);
		}
	}

	fh2m_inno_timer_start(axi_dma_timer, fh2m_inno_msecs_to_jiffies(DMA_TIMER_INTERVAL));
}

static void inno_axi_dma_chan_init(void *data)
{
	int i = 0;
	int chancnt = axi_dmac_get_chancnt(data);
	struct axi_dma_chan *chan_base, *chan;
	struct dma_device *dma_dev = axi_dmac_get_dma_device(data);
	int dma_pos = axi_dmac_get_pos(data);
	struct inno_timer *axi_dma_timer = NULL;

	chan_base = axi_dmac_get_chan(data);
	if (!chan_base)
	{
		innodma_error((inno_dev *)axi_dmac_get_platform_device(data), "failed to get chan.\n");
		return;
	}

	axi_dma_timer= axi_dmac_get_timer(data);

	fh2m_inno_timer_setup(axi_dma_timer, inno_axi_dma_timeout_callback, (unsigned long)data);
	fh2m_inno_timer_start(axi_dma_timer, fh2m_inno_msecs_to_jiffies(DMA_TIMER_INTERVAL));

	for (i = 0; i < chancnt; i++)
	{
		chan = chan_base + i;

		chan->vc.desc_free = inno_axi_dma_desc_free;
		innodma_vchan_init(&chan->vc, dma_dev);
		if (!dma_pos)
			chan->vc.chan.private = INNO_DMA_DEVICE_NAME;
		else
			chan->vc.chan.private = INNO_DMAR_DEVICE_NAME;
	}
}

static enum dma_status inno_axi_dma_tx_status(struct dma_chan *dchan, dma_cookie_t cookie,
											struct dma_tx_state *txstate)
{
	bool tx_stat;
	enum dma_status dma_stat;

	dma_stat = dma_cookie_status(dchan, cookie, txstate);
	tx_stat = axi_dmac_get_chan_tx_stat(dchan_to_axi_dma_chan(dchan)->private);

	if (tx_stat && DMA_IN_PROGRESS == dma_stat)
		dma_stat = DMA_PAUSED;

	return dma_stat;
}

static void inno_axi_dma_chan_tx_start(struct axi_dma_chan *chan)
{
	struct axi_dma_desc *desc;
	struct virt_dma_desc *vd;
	struct virt_dma_chan *vc = &chan->vc;
	bool status;

	status = innodma_vchan_is_busy(vc);
	vd = innodma_vchan_next_desc(&chan->vc);
	if (!vd || status)
		return;

	chan->time = fh2m_inno_time_jiffies( );
	vc->channel_status = BUSY;
	desc = vd_to_axi_desc(vd);
	axi_dmac_chan_block_xfer_start(desc->private);

	innodma_debug(chan2dev(chan), INNODMA_DEBUG_AXI_DMA, "%s: started %u.\n", axi_chan_name(chan), vd->tx.cookie);
}

static void inno_axi_dma_issue_pending(struct dma_chan *dchan)
{
	struct axi_dma_chan *chan = dchan_to_axi_dma_chan(dchan);
	unsigned long flags;

	spin_lock_irqsave(&chan->vc.lock, flags);

	if (innodma_vchan_issue_pending(&chan->vc))
		inno_axi_dma_chan_tx_start(chan);

	spin_unlock_irqrestore(&chan->vc.lock, flags);
}

/* Must be used during chan->vc.lock locking */
static void inno_axi_dma_retry_or_free(struct axi_dma_chan *chan)
{
	struct virt_dma_desc *vd;
	struct axi_dma_desc *desc;
	vd = innodma_vchan_next_desc(&chan->vc);
	if (vd == NULL)
		return;

	desc = vd_to_axi_desc(vd);
	axi_dmac_dump_lli_info(chan->private, desc->private);
	axi_dmac_dump_hw_info(chan->private);

	list_del(&vd->node);
	if (!dmaengine_desc_test_reuse(&vd->tx)) {
		chan->vc.desc_free(vd);
	}
}

static int inno_axi_dma_terminate_all(struct dma_chan *dchan)
{
	int ret = 0;
	unsigned long flags;
	struct axi_dma_chan *chan = dchan_to_axi_dma_chan(dchan);
	struct virt_dma_chan *vc = &chan->vc;

	LIST_HEAD(head);

	spin_lock_irqsave(&chan->vc.lock, flags);

	ret = axi_dmac_chan_terminate(chan->private);
	if (ret)
	{
		spin_unlock_irqrestore(&chan->vc.lock, flags);
		innodma_error((inno_dev *)dchan2dev(dchan), "axi-dma device_terminate_all failed.\n");
		return -EAGAIN;
	}

	inno_axi_dma_retry_or_free(chan);
	vc->channel_status = IDLE;

	spin_unlock_irqrestore(&chan->vc.lock, flags);

	return ret;
}

static int inno_axi_dma_pause(struct dma_chan *dchan)
{
	int ret;
	unsigned long flags;
	struct axi_dma_chan *chan = dchan_to_axi_dma_chan(dchan);

	spin_lock_irqsave(&chan->vc.lock, flags);

	ret = axi_dmac_chan_pause(chan->private);
	if (ret)
	{
		spin_unlock_irqrestore(&chan->vc.lock, flags);
		innodma_error((inno_dev *)dchan2dev(dchan), "axi-dma device_pause failed.\n");
		return -EAGAIN;
	}

	spin_unlock_irqrestore(&chan->vc.lock, flags);

	return 0;
}

static int inno_axi_dma_resume(struct dma_chan *dchan)
{
	int ret;
	unsigned long flags;
	struct axi_dma_chan *chan = dchan_to_axi_dma_chan(dchan);

	spin_lock_irqsave(&chan->vc.lock, flags);

	ret = axi_dmac_chan_resume(chan->private);
	if (ret)
	{
		spin_unlock_irqrestore(&chan->vc.lock, flags);
		innodma_error((inno_dev *)dchan2dev(dchan), "axi-dma device_resume failed.\n");
		return -EAGAIN;
	}

	spin_unlock_irqrestore(&chan->vc.lock, flags);

	return ret;
}

static int inno_axi_dma_alloc_chan_resources(struct dma_chan *dchan)
{
	int ret;
	struct axi_dma_chan *chan = dchan_to_axi_dma_chan(dchan);

	ret = axi_dmac_chan_is_available(chan->private);
	if (ret)
	{
		innodma_error((inno_dev *)dchan2dev(dchan), "axi-dma device_alloc_chan_resources failed.\n", __func__);
		return -EBUSY;
	}

	pm_runtime_get((struct device *)axi_dmac_chan_get_platform_device(chan->private));

	return ret;
}

static void inno_axi_dma_free_chan_resources(struct dma_chan *dchan)
{
	int ret;
	struct axi_dma_chan *chan = dchan_to_axi_dma_chan(dchan);

	ret = axi_dmac_chan_release(chan->private);
	if (ret)
		innodma_error((inno_dev *)dchan2dev(dchan), "axi-dma device_free_chan_resources failed.\n", __func__);

	innodma_vchan_free_chan_resources(&chan->vc);

	pm_runtime_put((struct device *)axi_dmac_chan_get_platform_device(chan->private));
}

static struct dma_async_tx_descriptor*
inno_axi_dma_prep_dma_memcpy(struct dma_chan *dchan,
							dma_addr_t dst, dma_addr_t src,
							size_t len, unsigned long flags)
{
	void *first;
	struct axi_dma_desc *desc;
	struct axi_dma_chan *chan = dchan_to_axi_dma_chan(dchan);

	first = axi_dmac_chan_prep_dma_memcpy(chan->private, dst, src, len);
	if (!first)
	{
		innodma_error((inno_dev *)dchan2dev(dchan), "axi-dma prepare memcpy failed.\n");
		return NULL;
	}

	desc = inno_axi_dma_desc_alloc();
	if (!desc)
	{
		innodma_error((inno_dev *)dchan2dev(dchan), "axi-dma alloc descriptor failed.\n");
		return NULL;
	}

	desc->private = first;
	desc->chan = chan;

	return innodma_vchan_tx_prep(&chan->vc, &desc->vd, flags);
}

static struct dma_async_tx_descriptor*
inno_axi_dma_prep_slave_sg(struct dma_chan *dchan, struct scatterlist *sgl, unsigned int sg_len,
							enum dma_transfer_direction dir, unsigned long flags, void *context)
{
	void *first;
	struct axi_dma_desc *desc;
	struct axi_dma_chan *chan = dchan_to_axi_dma_chan(dchan);

	first = axi_dmac_chan_prep_slave_sg(chan->private, sgl, sg_len, dir);
	if (!first)
	{
		innodma_error((inno_dev *)dchan2dev(dchan), "axi-dma prepare slave_sg failed.\n");
		return NULL;
	}

	desc = inno_axi_dma_desc_alloc();
	if (!desc)
	{
		innodma_error((inno_dev *)dchan2dev(dchan), "axi-dma alloc descriptor failed.\n");
		return NULL;
	}

	desc->private = first;
	desc->chan = chan;

	return innodma_vchan_tx_prep(&chan->vc, &desc->vd, flags);
}

static struct dma_async_tx_descriptor*
inno_axi_dma_prep_dma_memset(struct dma_chan *dchan, dma_addr_t dest, int value, size_t len, unsigned long flags)
{
	void *first;
	struct axi_dma_desc *desc;
	struct axi_dma_chan *chan = dchan_to_axi_dma_chan(dchan);

	first = axi_dmac_chan_prep_dma_memset(chan->private, dest, value, len);
	if (!first)
	{
		innodma_error((inno_dev *)dchan2dev(dchan), "axi-dma prepare memset failed.\n");
		return NULL;
	}

	desc = inno_axi_dma_desc_alloc();
	if (!desc)
	{
		innodma_error((inno_dev *)dchan2dev(dchan), "axi-dma alloc descriptor failed.\n");
		return NULL;
	}

	desc->private = first;
	desc->chan = chan;

	return innodma_vchan_tx_prep(&chan->vc, &desc->vd, flags);
}

static struct dma_async_tx_descriptor*
inno_axi_dma_prep_dma_memset_sg(struct dma_chan *dchan, struct scatterlist *sgl, unsigned int nents, int value, unsigned long flags)
{
	void *first;
	struct axi_dma_desc *desc;
	struct axi_dma_chan *chan = dchan_to_axi_dma_chan(dchan);

	first = axi_dmac_chan_prep_dma_memset_sg(chan->private, sgl, nents, value);
	if (!first)
	{
		innodma_error((inno_dev *)dchan2dev(dchan), "axi-dma prepare memset_sg failed.\n");
		return NULL;
	}

	desc = inno_axi_dma_desc_alloc();
	if (!desc)
	{
		innodma_error((inno_dev *)dchan2dev(dchan), "axi-dma alloc descriptor failed.\n");
		return NULL;
	}

	desc->private = first;
	desc->chan = chan;

	return innodma_vchan_tx_prep(&chan->vc, &desc->vd, flags);
}

static int inno_axi_dma_config(struct dma_chan *dchan, struct dma_slave_config *config)
{
	int ret;
	struct axi_dma_chan *chan = dchan_to_axi_dma_chan(dchan);

	ret = axi_dmac_slave_config(chan->private, config);
	if (ret)
		innodma_error((inno_dev *)dchan2dev(dchan), "axi-dma device_config failed.\n");

	return ret;
}

static void inno_dma_dmaengine_init(struct platform_device *pdev)
{
	struct dma_device *dma_dev;
	void *data = platform_get_drvdata(pdev);

	dma_dev = (struct dma_device *)axi_dmac_get_dma_device(data);

	/* Set capabilities */
	dma_cap_set(DMA_MEMCPY, dma_dev->cap_mask);
	dma_cap_set(DMA_SLAVE, dma_dev->cap_mask);
	dma_cap_set(DMA_PRIVATE, dma_dev->cap_mask);
	dma_cap_set(DMA_MEMSET, dma_dev->cap_mask);
	dma_cap_set(DMA_MEMSET_SG, dma_dev->cap_mask);

	/* DMA capabilities */
	dma_dev->chancnt = axi_dmac_get_chancnt(data);
	dma_dev->src_addr_widths = AXI_DMA_BUSWIDTHS;
	dma_dev->dst_addr_widths = AXI_DMA_BUSWIDTHS;
	dma_dev->directions = INNO_BIT(DMA_MEM_TO_MEM);
	dma_dev->residue_granularity = DMA_RESIDUE_GRANULARITY_DESCRIPTOR;

	dma_dev->dev = pdev->dev.parent;
	dma_dev->device_tx_status = inno_axi_dma_tx_status;
	dma_dev->device_issue_pending = inno_axi_dma_issue_pending;
	dma_dev->device_terminate_all = inno_axi_dma_terminate_all;
	dma_dev->device_pause = inno_axi_dma_pause;
	dma_dev->device_resume = inno_axi_dma_resume;

	dma_dev->device_alloc_chan_resources = inno_axi_dma_alloc_chan_resources;
	dma_dev->device_free_chan_resources = inno_axi_dma_free_chan_resources;

	dma_dev->device_prep_dma_memcpy = inno_axi_dma_prep_dma_memcpy;
	dma_dev->device_prep_slave_sg = inno_axi_dma_prep_slave_sg;
	dma_dev->device_prep_dma_memset = inno_axi_dma_prep_dma_memset;
	dma_dev->device_prep_dma_memset_sg = inno_axi_dma_prep_dma_memset_sg;
	dma_dev->device_config = inno_axi_dma_config;
	dma_dev->copy_align = DMA_ALIGN;
	/* We use the descriptor_reuse flag to distinguish the number of timeouts,
	which is true for the first timeout and false for the second timeout */
	dma_dev->descriptor_reuse = true;
}

static void inno_axi_dma_chan_handle_err(struct axi_dma_chan *chan)
{
	struct virt_dma_desc *vd;

	unsigned long flags;

	spin_lock_irqsave(&chan->vc.lock, flags);

	axi_dmac_chan_disable(chan->private);

	/* The bad desc currently is in the head of vc list */
	vd = innodma_vchan_next_desc(&chan->vc);
	if (!vd)
	{
		spin_unlock_irqrestore(&chan->vc.lock, flags);
		return;
	}
	/* remove the complete desc from issued list */
	list_del(&vd->node);

	innodma_error((inno_dev *)chan2dev(chan), "bad axi-dma descriptor submitted for %s, cookie: %d.\n", axi_chan_name(chan), vd->tx.cookie);
	chan->vc.channel_status = IDLE;
	innodma_vchan_cookie_complete(vd);

	/* Try to restart the Controller */
	inno_axi_dma_chan_tx_start(chan);

	spin_unlock_irqrestore(&chan->vc.lock, flags);
}

static void inno_axi_dma_chan_tx_complete(struct axi_dma_chan *chan)
{
	struct virt_dma_desc *vd;
	unsigned long flags;

	spin_lock_irqsave(&chan->vc.lock, flags);

	if (unlikely(axi_dmac_chan_is_available(chan->private)))
	{
		innodma_error((inno_dev *)chan2dev(chan), "%s is not idle.\n", axi_chan_name(chan));
		axi_dmac_chan_disable(chan->private);
	}

	/* The completed descriptor currently is in the head of vc list */
	vd = innodma_vchan_next_desc(&chan->vc);
	if (!vd)
	{
		spin_unlock_irqrestore(&chan->vc.lock, flags);
		return;
	}

	/* Remove the completed descriptor from issued list before completing */
	list_del(&vd->node);

	chan->vc.channel_status = IDLE;
	innodma_vchan_cookie_complete(vd);

	/* Submit queued descriptors after processing the completed ones */
	inno_axi_dma_chan_tx_start(chan);

	spin_unlock_irqrestore(&chan->vc.lock, flags);
}

static void inno_axi_dma_interrupt_handler(void *data)
{
	int i = 0;
	int ret = 0, intstat = 0;
	int chancnt = axi_dmac_get_chancnt(data);
	struct axi_dma_chan *chan_base, *chan;

	innodma_info(axi_dmac_get_platform_device(data), INNODMA_DEBUG_AXI_DMA, "enter into axi-dma interrupt handler.\n");

	intstat = axi_dmac_get_irq_status(data);

	chan_base = axi_dmac_get_chan(data);
	if (!chan_base)
	{
		innodma_error((inno_dev *)axi_dmac_get_platform_device(data), "get chan failed.\n");
		return;
	}

	for (i = 0; i < chancnt; i++)
	{
#ifndef SRIOV_VF_MODE
		if (!(INNO_BIT(i) & intstat))
			continue;
#endif
		chan = chan_base + i;

		ret = axi_dmac_chan_irq_process(chan->private);
		if (ret)
			inno_axi_dma_chan_handle_err(chan);
		else
			inno_axi_dma_chan_tx_complete(chan);
	}
}

static int inno_axi_dma_hw_init(struct platform_device *pdev)
{
	int ret = 0;
	void *data = platform_get_drvdata(pdev);

	ret = axi_dmac_irq_init(data, inno_axi_dma_interrupt_handler);
	if (ret)
	{
		innodma_error((inno_dev *)&pdev->dev, "init axi-dma interrupt failed.\n");
		return -1;
	}

	ret = axi_dmac_hw_init(data);
	if (ret)
	{
		innodma_error((inno_dev *)&pdev->dev, "init axi-dma hardware failed.\n");
		return -1;
	}

	pm_runtime_set_suspended(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	return 0;
}

static void dmam_device_release(struct device *dev, void *res)
{
	struct dma_device *device;
	struct axi_dma_chan *chan = NULL, *_chan = NULL;

	device = *(struct dma_device **)res;

	dma_async_device_unregister(device);

	list_for_each_entry_safe(chan, _chan, &device->channels, vc.chan.device_node)
	{
		list_del(&chan->vc.chan.device_node);
		tasklet_kill(&chan->vc.task);
	}
}

static int inno_axi_dma_async_device_register(struct platform_device *pdev)
{
	void **p;
	int ret;
	void *data = platform_get_drvdata(pdev);
	struct dma_device *device = axi_dmac_get_dma_device(data);

	p = devres_alloc(dmam_device_release, sizeof(*p), GFP_KERNEL);
	if (!p)
		return -ENOMEM;

	ret = dma_async_device_register(device);
	if (!ret)
	{
		*(struct dma_device **)p = device;
		devres_add(&pdev->dev, p);
	}
	else
	{
		devres_free(p);
	}

	return ret;
}

static int axi_dma_probe(struct platform_device *pdev)
{
	int ret = 0;
	void *data;

	data = inno_axi_dma_rsc_init(pdev);
	if (!data)
	{
		innodma_error((inno_dev *)&pdev->dev, "init axi-dma resouces failed.\n");
		return -1;
	}

	inno_axi_dma_chan_init(data);

	inno_dma_dmaengine_init(pdev);

	ret = inno_axi_dma_hw_init(pdev);
	if (ret)
	{
		innodma_error((inno_dev *)&pdev->dev, "init axi-dma hardware failed.\n");
		return -1;
	}

	ret = inno_axi_dma_async_device_register(pdev);
	if (ret)
	{
		innodma_error((inno_dev *)&pdev->dev, "regist axi-dma to dmaengine framework failed.\n");
		return -1;
	}

	innodma_info(&pdev->dev, INNODMA_DEBUG_AXI_DMA, "INNO AXI-DMA Controller, %d channels\n", axi_dmac_get_chancnt(data));

	return ret;
}

static int axi_dma_remove(struct platform_device *pdev)
{
	void *data = platform_get_drvdata(pdev);

	pm_runtime_disable(&pdev->dev);
	axi_dmac_irq_remove(data);
	axi_dmac_hw_remove(data);
	axi_dmac_rsc_remove(data);

	return 0;
}

static int __maybe_unused inno_axi_dma_runtime_suspend(struct device *dev)
{
	void *data = dev_get_drvdata(dev);
	axi_dmac_runtime_suspend(data);
	return 0;
}

static int __maybe_unused inno_axi_dma_runtime_resume(struct device *dev)
{
	void *data = dev_get_drvdata(dev);
	axi_dmac_runtime_resume(data);
	return 0;
}
static int inno_axi_dma_sys_suspend(struct device *dev)
{
	int ret = 0;
	void *data = dev_get_drvdata(dev);

	ret = axi_dmac_suspend(data);
	if (ret) {
		innodma_error((inno_dev *)dev, "axi-dma suspend failed.\n");
	}

	return ret;
}

static int inno_axi_dma_sys_resume(struct device *dev)
{
	int ret = 0;
	void *data = dev_get_drvdata(dev);

	ret = axi_dmac_resume(data);
	if (ret) {
		innodma_error((inno_dev *)dev, "axi-dma resume failed.\n");
	}

	return ret;
}

static int inno_axi_dma_sys_freeze(struct device *dev)
{
	return inno_axi_dma_sys_suspend(dev);
}

static int inno_axi_dma_sys_thaw(struct device *dev)
{
	return inno_axi_dma_sys_resume(dev);
}

static int inno_axi_dma_sys_poweroff(struct device *dev)
{
	return inno_axi_dma_sys_suspend(dev);
}

static int inno_axi_dma_sys_restore(struct device *dev)
{
	return inno_axi_dma_sys_resume(dev);
}

static void inno_axi_dma_shutdown(struct platform_device *pdev)
{
	inno_axi_dma_sys_suspend(&pdev->dev);
}

static const struct dev_pm_ops axi_dma_pm_ops = {
	SET_RUNTIME_PM_OPS(inno_axi_dma_runtime_suspend, inno_axi_dma_runtime_resume, NULL)
	.suspend  = inno_axi_dma_sys_suspend,
	.resume   = inno_axi_dma_sys_resume,
	.freeze   = inno_axi_dma_sys_freeze,
	.thaw     = inno_axi_dma_sys_thaw,
	.poweroff = inno_axi_dma_sys_poweroff,
	.restore  = inno_axi_dma_sys_restore,
};

static struct platform_device_id axi_dma_platform_device_id_table[] = {
	{ .name = INNO_DMA_DEVICE_NAME, .driver_data = 0 },
	{ .name = INNO_DMAR_DEVICE_NAME, .driver_data = 0 },
	{ },
};
MODULE_DEVICE_TABLE(platform, axi_dma_platform_device_id_table);

struct platform_driver axi_dma_driver = {
	.probe      = axi_dma_probe,
	.remove     = axi_dma_remove,
	.shutdown   = inno_axi_dma_shutdown,
	.driver = {
		.name   = DMA_DRV_NAME,
		.pm     = &axi_dma_pm_ops,
	},
	.id_table   = axi_dma_platform_device_id_table,
};
