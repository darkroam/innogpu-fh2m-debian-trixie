/*************************************************************************/ /*!
@File           inno_pcie_dma_drv.c
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
#include <linux/pci.h>
#include <linux/msi.h>
#include <linux/slab.h>


#include "innodma_debug.h"
#include "pcie_dmac.h"
#include "inno_dmaengine.h"
#include "virt_dma.h"
#include "innogpu.h"
#include "inno_pcie_dma_drv.h"
#include "inno_plat_dev.h"
#include "inno_debug.h"

#define PDMA_DRV_NAME	"inno-pdma"

struct pcie_dma_chan {
	void *private;
	unsigned long time;
	struct virt_dma_chan	vc;
};

struct pcie_dma_desc {
	void *private;
	struct virt_dma_desc vd;
	struct pcie_dma_chan *chan;
};

static void inno_pcie_dma_handle_err(struct pcie_dma_chan *chan);
static void inno_pcie_dma_timeout_callback(unsigned long data);

static inline struct device *pdchan2dev(struct dma_chan *dchan)
{
	return &dchan->dev->device;
}

static inline struct device *pchan2dev(struct pcie_dma_chan *chan)
{
	return &chan->vc.chan.dev->device;
}

static inline struct pcie_dma_desc *vd_to_pcie_desc(struct virt_dma_desc *vd)
{
	return container_of(vd, struct pcie_dma_desc, vd);
}

static inline struct pcie_dma_chan *vc_to_pcie_dma_chan(struct virt_dma_chan *vc)
{
	return container_of(vc, struct pcie_dma_chan, vc);
}

static inline struct pcie_dma_chan *dchan_to_pcie_dma_chan(struct dma_chan *dchan)
{
	return vc_to_pcie_dma_chan(innodma_to_virt_chan(dchan));
}

static inline const char *pcie_chan_name(struct pcie_dma_chan *chan)
{
	return dma_chan_name(&chan->vc.chan);
}

void* inno_pcie_dma_alloc_chan(void *dev, int chancnt)
{
	return (void *)devm_kcalloc((struct device *)dev, chancnt, sizeof(struct pcie_dma_chan), GFP_KERNEL);
}

void inno_pcie_dma_set_chan_private(void *chan_ptr, int chanidx, void *data)
{
	struct pcie_dma_chan *chan = ((struct pcie_dma_chan *)chan_ptr + chanidx);
	chan->private = data;
}

void* inno_pcie_dma_get_chan_private(void *chan_ptr, int chanidx)
{
	struct pcie_dma_chan *chan = ((struct pcie_dma_chan *)chan_ptr + chanidx);
	return chan->private;
}

static void* inno_pcie_dma_rsc_init(struct platform_device *pdev)
{
	return (void *)pcie_dmac_rsc_init(pdev);
}

static struct pcie_dma_desc* inno_pcie_dma_desc_alloc(void)
{
	struct pcie_dma_desc *desc;

	desc = kmalloc(sizeof(struct pcie_dma_desc), GFP_KERNEL);
	if (!desc)
		return NULL;

	memset(desc, 0, sizeof(struct pcie_dma_desc));

	return desc;
}

static void inno_pcie_dma_desc_free(struct virt_dma_desc *vd)
{
	struct pcie_dma_desc *desc = vd_to_pcie_desc(vd);

	pcie_dmac_desc_put(desc->private);
	kfree(desc);
}

static void inno_pcie_dma_timeout_process(struct pcie_dma_chan *chan)
{
	bool is_reuse = false;
	struct virt_dma_desc *vd;

	innodma_warn(pchan2dev(chan), "pcie-dma transfer timeout.\n");

	vd = innodma_vchan_next_desc(&chan->vc);
	if (!vd)
		return;

	is_reuse = dmaengine_desc_test_reuse(&vd->tx);
	if (is_reuse) {
		dmaengine_terminate_all(&chan->vc.chan);
		dmaengine_desc_clear_reuse(&vd->tx);
		dmaengine_submit(&vd->tx);
		dma_async_issue_pending(&chan->vc.chan);
		innodma_warn(pchan2dev(chan), "pcie-dma transfer timeout.retry xfer!\n");
	}
	else {
		inno_pcie_dma_handle_err(chan);
		innodma_error(pchan2dev(chan), "pcie-dma transfer timeout.handler err!\n");
	}
}

static void inno_pcie_dma_timeout_callback(unsigned long data)
{
	int i;
	struct pcie_dma_chan *chanbase = NULL, *chan = NULL;
	struct virt_dma_chan *vc;
	unsigned int chancnt = 0;
	struct inno_timer *pcie_dma_timer;
	unsigned long timeout;
	chanbase = pcie_dmac_get_chan((void *)data);
	chancnt = pcie_dmac_get_chancnt((void *)data);
	pcie_dma_timer = pcie_dmac_get_timer((void *)data);

	for (i = 0;i < chancnt;i++)
	{
		chan = chanbase + i;
		vc = &chan->vc;
		if (innodma_vchan_is_busy(vc))
		{
			timeout = chan->time + fh2m_inno_msecs_to_jiffies(DMA_TRANSFER_TIMEOUT);
			if (fh2m_inno_time_after(timeout))
				inno_pcie_dma_timeout_process(chan);
		}
	}
	fh2m_inno_timer_start(pcie_dma_timer, fh2m_inno_msecs_to_jiffies(DMA_TIMER_INTERVAL));
}

static void inno_pcie_dma_chan_init(void *data)
{
	int i = 0;
	int chancnt = pcie_dmac_get_chancnt(data);
	struct pcie_dma_chan *chan_base, *chan;
	struct dma_device *dma_dev = pcie_dmac_get_dma_device(data);
	struct inno_timer *pcie_dma_timer;

	chan_base = pcie_dmac_get_chan(data);
	if (!chan_base)
	{
		innodma_error((inno_dev *)pcie_dmac_get_platform_device(data), "get chan failed.\n");
		return;
	}

	pcie_dma_timer = pcie_dmac_get_timer(data);
	fh2m_inno_timer_setup(pcie_dma_timer, inno_pcie_dma_timeout_callback, (unsigned long)data);
	fh2m_inno_timer_start(pcie_dma_timer, fh2m_inno_msecs_to_jiffies(DMA_TIMER_INTERVAL));

	for (i = 0; i < chancnt; i++)
	{
		chan = chan_base + i;

		chan->vc.desc_free = inno_pcie_dma_desc_free;
		chan->vc.chan.private = INNO_PDMA_DEVICE_NAME;
		innodma_vchan_init(&chan->vc, dma_dev);
	}
}

static enum dma_status inno_pcie_dma_tx_status(struct dma_chan *dchan, dma_cookie_t cookie, struct dma_tx_state *txstate)
{
	bool tx_stat;
	enum dma_status dma_stat;
	struct pcie_dma_chan *chan = dchan_to_pcie_dma_chan(dchan);

	dma_stat = dma_cookie_status(dchan, cookie, txstate);
	tx_stat = pcie_dmac_get_chan_tx_stat(chan->private);

	if (tx_stat && DMA_IN_PROGRESS == dma_stat)
		dma_stat = DMA_PAUSED;

	return dma_stat;
}

static void inno_pcie_dma_start_first_queued(struct pcie_dma_chan *chan)
{
	struct pcie_dma_desc *desc;
	struct virt_dma_desc *vd;
	struct virt_dma_chan *vc = &chan->vc;
	bool status;

	status = innodma_vchan_is_busy(vc);
	vd = innodma_vchan_next_desc(&chan->vc);
	if (!vd || status)
		return;

	chan->time = fh2m_inno_time_jiffies();
	vc->channel_status = BUSY;
	desc = vd_to_pcie_desc(vd);
	innodma_debug(pchan2dev(chan), INNODMA_DEBUG_PCIE_DMA, "%s: started %x\n", pcie_chan_name(chan), vd->tx.cookie);

	pcie_chan_block_xfer_start(desc->private);
}

static void inno_pcie_dma_issue_pending(struct dma_chan *dchan)
{
	struct pcie_dma_chan *chan = dchan_to_pcie_dma_chan(dchan);
	unsigned long flags;

	spin_lock_irqsave(&chan->vc.lock, flags);

	if (innodma_vchan_issue_pending(&chan->vc)) {
		inno_pcie_dma_start_first_queued(chan);
	}

	spin_unlock_irqrestore(&chan->vc.lock, flags);
}

static void inno_pcie_dma_retry_or_free(struct pcie_dma_chan *chan)
{
	struct virt_dma_desc *vd;
	struct pcie_dma_desc *desc;

	vd = innodma_vchan_next_desc(&chan->vc);
	if (vd == NULL)
		return;

	desc = vd_to_pcie_desc(vd);
	pcie_dmac_dump_lli_info(chan->private, desc->private);
	pcie_dmac_dump_hw_info(chan->private);

	list_del(&vd->node);
	if (!dmaengine_desc_test_reuse(&vd->tx)) {
		chan->vc.desc_free(vd);
	}
}

static int inno_pcie_dma_terminate_all(struct dma_chan *dchan)
{
	int ret = 0;
	unsigned long flags;
	struct pcie_dma_chan *chan = dchan_to_pcie_dma_chan(dchan);
	struct virt_dma_chan *vc = &chan->vc;
	LIST_HEAD(head);

	spin_lock_irqsave(&chan->vc.lock, flags);
	vc->channel_status = IDLE;
	ret = pcie_dmac_chan_terminate(chan->private);
	if (ret)
	{
		innodma_error((inno_dev *)pdchan2dev(dchan), "pcie-dma device_terminate_all failed.\n");
		spin_unlock_irqrestore(&chan->vc.lock, flags);
		return -EAGAIN;
	}

	inno_pcie_dma_retry_or_free(chan);

	spin_unlock_irqrestore(&chan->vc.lock, flags);

	return ret;
}

static int inno_pcie_dma_pause(struct dma_chan *dchan)
{
	int ret = 0;
	struct pcie_dma_chan *chan = dchan_to_pcie_dma_chan(dchan);

	ret = pcie_dmac_chan_pause(chan->private);
	if (ret)
	{
		innodma_error((inno_dev *)pdchan2dev(dchan), "%s failed.\n", __func__);
		return -EAGAIN;
	}

	return ret;
}

static int inno_pcie_dma_resume(struct dma_chan *dchan)
{
	int ret = 0;
	struct pcie_dma_chan *chan = dchan_to_pcie_dma_chan(dchan);

	ret = pcie_dmac_chan_resume(chan->private);
	if (ret)
	{
		innodma_error((inno_dev *)pdchan2dev(dchan), "pcie-dma device_resume failed.\n");
		return -EAGAIN;
	}

	return ret;
}

static int inno_pcie_dma_alloc_chan_resources(struct dma_chan *dchan)
{
	int ret;
	struct pcie_dma_chan *chan = dchan_to_pcie_dma_chan(dchan);

	ret = pcie_dmac_chan_is_available(chan->private);
	if (ret)
	{
		innodma_error(pdchan2dev(dchan), "pcie-dma device_alloc_chan_resources failed.\n");
		return -EBUSY;
	}

	/* pm_runtime_get(chan->chip->dev); */
	return ret;
}

static void inno_pcie_dma_free_chan_resources(struct dma_chan *dchan)
{
	int ret;
	struct pcie_dma_chan *chan = dchan_to_pcie_dma_chan(dchan);

	ret = pcie_dmac_chan_release(chan->private);
	if (ret) {
		innodma_error(pdchan2dev(dchan), "pcie-dma device_free_chan_resources failed.\n");
	}

	innodma_vchan_free_chan_resources(&chan->vc);

	/* pm_runtime_put(chan->chip->dev); */
}

static struct dma_async_tx_descriptor*
inno_pcie_dma_prep_dma_memcpy(struct dma_chan *dchan, dma_addr_t dst, dma_addr_t src,
								size_t len, unsigned long flags)
{
	void *data;
	struct pcie_dma_desc *desc;
	struct pcie_dma_chan *chan = dchan_to_pcie_dma_chan(dchan);

	data = pcie_dmac_chan_prep_dma_memcpy(chan->private, dst, src, len);
	if (!data)
	{
		innodma_error(pchan2dev(chan), "pcie-dma device_prep_dma_memcpy failed.\n");
		return NULL;
	}

	desc = inno_pcie_dma_desc_alloc();
	if (!desc)
	{
		innodma_error(pchan2dev(chan), "pcie-dma alloc descriptor failed.\n");
		return NULL;
	}

	desc->private = data;

	return innodma_vchan_tx_prep(&chan->vc, &desc->vd, flags);
}

static struct dma_async_tx_descriptor *
inno_pcie_dma_prep_slave_sg(struct dma_chan *dchan, struct scatterlist *sgl, unsigned int sg_len,
							enum dma_transfer_direction dir, unsigned long flags, void *context)
{
	void *data;
	struct pcie_dma_desc *desc;
	struct pcie_dma_chan *chan = dchan_to_pcie_dma_chan(dchan);

	data = pcie_dmac_chan_prep_slave_sg(chan->private, sgl, sg_len, dir);
	if (!data)
	{
		innodma_error(pchan2dev(chan), "pcie-dma device_prep_slave_sg failed.\n");
		return NULL;
	}

	desc = inno_pcie_dma_desc_alloc();
	if (!desc)
	{
		innodma_error(pchan2dev(chan), "pcie-dma alloc descriptor failed.\n");
		return NULL;
	}

	desc->private = data;

	return innodma_vchan_tx_prep(&chan->vc, &desc->vd, flags);
}

static int inno_pcie_dma_config(struct dma_chan *dchan, struct dma_slave_config *config)
{
	int ret;
	struct pcie_dma_chan *chan = dchan_to_pcie_dma_chan(dchan);

	ret = pcie_dmac_slave_config(chan->private, config);
	if (ret) {
		innodma_error(pdchan2dev(dchan), "pcie-dma device_config failed.\n");
	}

	return ret;
}

static void inno_pcie_dma_dmaengine_init(struct platform_device *pdev)
{
	void *data = platform_get_drvdata(pdev);
	struct dma_device *dma_dev = pcie_dmac_get_dma_device(data);

	/* Set capabilities */
	dma_cap_set(DMA_MEMCPY, dma_dev->cap_mask);
	dma_cap_set(DMA_SLAVE, dma_dev->cap_mask);
	dma_cap_set(DMA_PRIVATE, dma_dev->cap_mask);

	/* DMA capabilities */
	dma_dev->chancnt = pcie_dmac_get_chancnt(data);
	dma_dev->src_addr_widths = INNO_BIT(DMA_SLAVE_BUSWIDTH_1_BYTE);
	dma_dev->dst_addr_widths = INNO_BIT(DMA_SLAVE_BUSWIDTH_1_BYTE);
	dma_dev->directions = INNO_BIT(DMA_MEM_TO_DEV)|INNO_BIT(DMA_DEV_TO_MEM);
	dma_dev->residue_granularity = DMA_RESIDUE_GRANULARITY_DESCRIPTOR;

	dma_dev->dev = pdev->dev.parent;
	dma_dev->device_tx_status = inno_pcie_dma_tx_status;
	dma_dev->device_issue_pending = inno_pcie_dma_issue_pending;
	dma_dev->device_terminate_all = inno_pcie_dma_terminate_all;
	dma_dev->device_pause = inno_pcie_dma_pause;
	dma_dev->device_resume = inno_pcie_dma_resume;
	dma_dev->device_alloc_chan_resources = inno_pcie_dma_alloc_chan_resources;
	dma_dev->device_free_chan_resources = inno_pcie_dma_free_chan_resources;
	dma_dev->device_prep_dma_memcpy = inno_pcie_dma_prep_dma_memcpy;
	dma_dev->device_prep_slave_sg = inno_pcie_dma_prep_slave_sg;
	dma_dev->device_config = inno_pcie_dma_config;
	dma_dev->copy_align = DMAENGINE_ALIGN_1_BYTE;
	/* We use the descriptor_reuse flag to distinguish the number of timeouts,
	which is true for the first timeout and false for the second timeout */
	dma_dev->descriptor_reuse = true;
}

static void inno_pcie_dma_handle_err(struct pcie_dma_chan *chan)
{
	struct virt_dma_desc *vd;
	unsigned long flags;

	spin_lock_irqsave(&chan->vc.lock, flags);

	pcie_dmac_chan_terminate(chan->private);

	vd = innodma_vchan_next_desc(&chan->vc);
	if (!vd)
	{
		spin_unlock_irqrestore(&chan->vc.lock, flags);
		return;
	}

	list_del(&vd->node);

	/* WARN about bad descriptor */
	innodma_error(pchan2dev(chan), "bad pcie-dma descriptor submitted for %s, cookie: %d.\n", pcie_chan_name(chan), vd->tx.cookie);
	chan->vc.channel_status = IDLE;
	innodma_vchan_cookie_complete(vd);

	inno_pcie_dma_start_first_queued(chan);

	spin_unlock_irqrestore(&chan->vc.lock, flags);
}

static void inno_pcie_dma_tx_complete(struct pcie_dma_chan *chan)
{
	struct virt_dma_desc *vd;
	unsigned long flags;

	innodma_info(pchan2dev(chan), INNODMA_DEBUG_PCIE_DMA, "one xfer complete\n");

	spin_lock_irqsave(&chan->vc.lock, flags);

	vd = innodma_vchan_next_desc(&chan->vc);
	if (!vd)
	{
		spin_unlock_irqrestore(&chan->vc.lock, flags);
		return;
	}

	list_del(&vd->node);

	chan->vc.channel_status = IDLE;

	innodma_vchan_cookie_complete(vd);

	inno_pcie_dma_start_first_queued(chan);

	spin_unlock_irqrestore(&chan->vc.lock, flags);
}

static void inno_pcie_dma_interrupt_handler(void *data)
{
	int i = 0, ret = 0;
	uint64_t int_status = 0;
	int chancnt = pcie_dmac_get_chancnt(data);
	struct pcie_dma_chan *chan_base, *chan;

	innodma_debug(pcie_dmac_get_platform_device(data), INNODMA_DEBUG_PCIE_DMA, "enter into pcie-dma interrupt handler.\n");

	chan_base = pcie_dmac_get_chan(data);
	if (!chan_base)
	{
		innodma_error(pcie_dmac_get_platform_device(data), "get chan failed.\n");
		return;
	}

	int_status = pcie_dmac_get_irq_status(data);

	for (i = 0; i < chancnt; i++)
	{
		chan = chan_base + i;
		ret = pcie_dmac_irq_process(chan->private, int_status);
		if (-1 == ret)
			inno_pcie_dma_handle_err(chan);
		else if (0 == ret)
			inno_pcie_dma_tx_complete(chan);
	}
}

static int inno_pcie_dma_hw_init(struct platform_device *pdev)
{
	int ret = 0;
	void *data= platform_get_drvdata(pdev);

	ret = pcie_dmac_irq_init(data, inno_pcie_dma_interrupt_handler);
	if (ret)
	{
		innodma_error(&pdev->dev, "pcie-dma interrupt init failed.\n");
		return -1;
	}

	ret = pcie_dmac_hw_init(data);
	if (ret)
	{
		innodma_error(&pdev->dev, "pcie-dma hardware init failed.\n");
		return -1;
	}

	return 0;
}

static void pcie_dmam_device_release(struct device *dev, void *res)
{
	struct dma_device *device;
	struct pcie_dma_chan *chan = NULL, *_chan = NULL;

	device = *(struct dma_device **)res;

	dma_async_device_unregister(device);

	list_for_each_entry_safe(chan, _chan, &device->channels,
			vc.chan.device_node) {
		list_del(&chan->vc.chan.device_node);
		tasklet_kill(&chan->vc.task);
	}
}

static int pcie_dma_async_device_register(struct platform_device *pdev)
{
	void **p;
	int ret;
	struct dma_device *device;
	void *data = platform_get_drvdata(pdev);
	device = pcie_dmac_get_dma_device(data);
	if (!device)
	{
		inno_error("pcie dma device null\n");
		return -1;
	}

	p = devres_alloc(pcie_dmam_device_release, sizeof(*p), GFP_KERNEL);
	if (!p)
		return -ENOMEM;

	ret = dma_async_device_register(device);
	if (!ret) {
		*(struct dma_device **)p = device;
		devres_add(&pdev->dev, p);
	} else {
		devres_free(p);
	}

	return ret;
}

static int pcie_dma_probe(struct platform_device *pdev)
{
	int ret = 0;
	void *data;

	data = inno_pcie_dma_rsc_init(pdev);
	if (!data)
	{
		innodma_error(&pdev->dev, "pcie-dma init resources failed.\n");
		return -1;
	}

	inno_pcie_dma_chan_init(data);

	inno_pcie_dma_dmaengine_init(pdev);

	ret = inno_pcie_dma_hw_init(pdev);
	if (ret)
	{
		innodma_error(&pdev->dev, "pcie-dma init hardware failed.\n");
		return -1;
	}

	ret = pcie_dma_async_device_register(pdev);
	if (ret)
	{
		innodma_error(&pdev->dev, "regist pcie-dma to dmaengine framework failed.\n");
		return -1;
	}

	innodma_info(&pdev->dev, INNODMA_DEBUG_PCIE_DMA, "INNO PCIE-DMA Controller, %d channels\n", pcie_dmac_get_chancnt(data));

	return 0;
}

static int pcie_dma_remove(struct platform_device *pdev)
{
	void *data = platform_get_drvdata(pdev);

	pcie_dmac_irq_remove(data);
	pcie_dmac_hw_remove(data);
	pcie_dmac_rsc_remove(data);

	return 0;
}

static int __maybe_unused inno_pcie_dma_runtime_suspend(struct device *dev)
{
	void *data = dev_get_drvdata(dev);
	return pcie_dmac_runtime_suspend(data);
}

static int __maybe_unused inno_pcie_dma_runtime_resume(struct device *dev)
{
	void *data = dev_get_drvdata(dev);
	return pcie_dmac_runtime_resume(data);
}

static int inno_pcie_dma_sys_suspend(struct device *dev)
{
	void *data = dev_get_drvdata(dev);
	return pcie_dmac_suspend(data);
}

static int inno_pcie_dma_sys_resume(struct device *dev)
{
	void *data = dev_get_drvdata(dev);
	return pcie_dmac_resume(data);
}

static int inno_pcie_dma_sys_freeze(struct device *dev)
{
	return inno_pcie_dma_sys_suspend(dev);
}

static int inno_pcie_dma_sys_thaw(struct device *dev)
{
	return inno_pcie_dma_sys_resume(dev);
}

static int inno_pcie_dma_sys_poweroff(struct device *dev)
{
	return inno_pcie_dma_sys_suspend(dev);
}

static int inno_pcie_dma_sys_restore(struct device *dev)
{
	return inno_pcie_dma_sys_resume(dev);
}

static void inno_pcie_dma_shutdown(struct platform_device *pdev)
{
	inno_pcie_dma_sys_suspend(&pdev->dev);
}

static const struct dev_pm_ops pcie_dma_pm_ops = {
	SET_RUNTIME_PM_OPS(inno_pcie_dma_runtime_suspend, inno_pcie_dma_runtime_resume, NULL)
	.suspend  = inno_pcie_dma_sys_suspend,
	.resume   = inno_pcie_dma_sys_resume,
	.freeze   = inno_pcie_dma_sys_freeze,
	.thaw     = inno_pcie_dma_sys_thaw,
	.poweroff = inno_pcie_dma_sys_poweroff,
	.restore  = inno_pcie_dma_sys_restore,
};

static struct platform_device_id pcie_dma_platform_device_id_table[] = {
	{ .name = INNO_PDMA_DEVICE_NAME, .driver_data = 0 },
	{ },
};
MODULE_DEVICE_TABLE(platform, pcie_dma_platform_device_id_table);

struct platform_driver pcie_dma_driver = {
	.probe    = pcie_dma_probe,
	.remove   = pcie_dma_remove,
	.shutdown = inno_pcie_dma_shutdown,
	.driver   = {
		.name = PDMA_DRV_NAME,
		.pm   = &pcie_dma_pm_ops,
	},
	.id_table = pcie_dma_platform_device_id_table,
};
