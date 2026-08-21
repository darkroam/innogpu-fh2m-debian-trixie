/*************************************************************************/ /*!
@File           virt_dma.c
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
#include <linux/device.h>
#include <linux/dmaengine.h>
#include <linux/module.h>
#include <linux/spinlock.h>

#include "virt_dma.h"

static struct virt_dma_desc *innodma_to_virt_desc(struct dma_async_tx_descriptor *tx)
{
	return container_of(tx, struct virt_dma_desc, tx);
}

dma_cookie_t innodma_vchan_tx_submit(struct dma_async_tx_descriptor *tx)
{
	struct virt_dma_chan *vc = innodma_to_virt_chan(tx->chan);
	struct virt_dma_desc *vd = innodma_to_virt_desc(tx);
	unsigned long flags;
	dma_cookie_t cookie;
	
	spin_lock_irqsave(&vc->lock, flags);
	cookie = dma_cookie_assign(tx);
	
	list_add_tail(&vd->node, &vc->desc_submitted);
	spin_unlock_irqrestore(&vc->lock, flags);

	//dev_dbg(vc->chan.device->dev, "vchan %p: txd %p[%x]: submitted\n",
	//	vc, vd, cookie);

	return cookie;
}

/**
 * innodma_vchan_tx_desc_free - free a reusable descriptor
 * @tx: the transfer
 *
 * This function frees a previously allocated reusable descriptor. The only
 * other way is to clear the DMA_CTRL_REUSE flag and submit one last time the
 * transfer.
 *
 * Returns 0 upon success
 */

int innodma_vchan_tx_desc_free(struct dma_async_tx_descriptor *tx)
{
	struct virt_dma_chan *vc = innodma_to_virt_chan(tx->chan);
	struct virt_dma_desc *vd = innodma_to_virt_desc(tx);
	unsigned long flags;

	spin_lock_irqsave(&vc->lock, flags);
	list_del(&vd->node);
	spin_unlock_irqrestore(&vc->lock, flags);

	dev_dbg(vc->chan.device->dev, "vchan %p: txd %p[%x]: freeing\n",
		vc, vd, vd->tx.cookie);
	vc->desc_free(vd);
	return 0;
}

struct virt_dma_desc *innodma_vchan_find_desc(struct virt_dma_chan *vc,
	dma_cookie_t cookie)
{
	struct virt_dma_desc *vd;

	list_for_each_entry(vd, &vc->desc_issued, node)
		if (vd->tx.cookie == cookie)
			return vd;

	return NULL;
}

/*
 * This tasklet handles the completion of a DMA descriptor by
 * calling its callback and freeing it.
 */
static void innodma_vchan_complete(unsigned long arg)
{
	struct virt_dma_chan *vc = (struct virt_dma_chan *)arg;
	struct virt_dma_desc *vd, *_vd = NULL;
	struct dmaengine_desc_callback cb;
	LIST_HEAD(head);

	spin_lock_irq(&vc->lock);
	list_splice_tail_init(&vc->desc_completed, &head);
	vd = vc->cyclic;
	if (vd) {
		vc->cyclic = NULL;
		dmaengine_desc_get_callback(&vd->tx, &cb);
	} else {
		memset(&cb, 0, sizeof(cb));
	}
	spin_unlock_irq(&vc->lock);

	dmaengine_desc_callback_invoke(&cb, NULL);

	list_for_each_entry_safe(vd, _vd, &head, node) {
		dmaengine_desc_get_callback(&vd->tx, &cb);

		/* process cb first, cb may clear desc reuse */
		dmaengine_desc_callback_invoke(&cb, NULL);

		list_del(&vd->node);
		vc->desc_free(vd);
	}
}

void innodma_vchan_dma_desc_free_list(struct virt_dma_chan *vc, struct list_head *head)
{
	struct virt_dma_desc *vd = NULL, *_vd = NULL;

	list_for_each_entry_safe(vd, _vd, head, node) {
		dev_dbg(vc->chan.device->dev, "txd %p: freeing\n", vd);
		list_del(&vd->node);
		vc->desc_free(vd);
	}
}

void innodma_vchan_init(struct virt_dma_chan *vc, struct dma_device *dmadev)
{
	dma_cookie_init(&vc->chan);
	spin_lock_init(&vc->lock);
	INIT_LIST_HEAD(&vc->desc_submitted);
	INIT_LIST_HEAD(&vc->desc_issued);
	INIT_LIST_HEAD(&vc->desc_completed);

	tasklet_init(&vc->task, innodma_vchan_complete, (unsigned long)vc);

	vc->chan.device = dmadev;
	vc->channel_status = IDLE;
	list_add_tail(&vc->chan.device_node, &dmadev->channels);
}
