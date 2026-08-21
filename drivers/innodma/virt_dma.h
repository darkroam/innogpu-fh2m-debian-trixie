/*************************************************************************/ /*!
@File           virt_dma.h
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
#ifndef VIRT_DMA_H
#define VIRT_DMA_H

#include <linux/dmaengine.h>
#include <linux/interrupt.h>

#include "inno_dmaengine.h"
#include "inno_timer.h"

#define IDLE 0
#define BUSY 1
#define DMA_TRANSFER_TIMEOUT 10000
#define DMA_TIMER_INTERVAL 2000

struct virt_dma_desc {
	struct dma_async_tx_descriptor tx;
	/* protected by vc.lock */
	struct list_head node;
};

struct virt_dma_chan {
	struct dma_chan	chan;
	struct tasklet_struct task;
	void (*desc_free)(struct virt_dma_desc *);

	spinlock_t lock;

	/* protected by vc.lock */
	struct list_head desc_submitted;
	struct list_head desc_issued;
	struct list_head desc_completed;

	struct virt_dma_desc *cyclic;
	bool channel_status;	/* busy or idle */
};

static inline struct virt_dma_chan *innodma_to_virt_chan(struct dma_chan *chan)
{
	return container_of(chan, struct virt_dma_chan, chan);
}

static inline bool innodma_vchan_is_busy(struct virt_dma_chan *vc)
{
	return vc->channel_status == BUSY;
}

void innodma_vchan_dma_desc_free_list(struct virt_dma_chan *vc, struct list_head *head);
void innodma_vchan_init(struct virt_dma_chan *vc, struct dma_device *dmadev);
struct virt_dma_desc *innodma_vchan_find_desc(struct virt_dma_chan *, dma_cookie_t);
extern dma_cookie_t innodma_vchan_tx_submit(struct dma_async_tx_descriptor *);
extern int innodma_vchan_tx_desc_free(struct dma_async_tx_descriptor *);

/**
 * innodma_vchan_tx_prep - prepare a descriptor
 * @vc: virtual channel allocating this descriptor
 * @vd: virtual descriptor to prepare
 * @tx_flags: flags argument passed in to prepare function
 */
static inline struct dma_async_tx_descriptor *innodma_vchan_tx_prep(struct virt_dma_chan *vc,
	struct virt_dma_desc *vd, unsigned long tx_flags)
{
	dma_async_tx_descriptor_init(&vd->tx, &vc->chan);
	vd->tx.flags = tx_flags;
	vd->tx.tx_submit = innodma_vchan_tx_submit;
	vd->tx.desc_free = innodma_vchan_tx_desc_free;

	return &vd->tx;
}

/**
 * innodma_vchan_issue_pending - move submitted descriptors to issued list
 * @vc: virtual channel to update
 *
 * vc.lock must be held by caller
 */
static inline bool innodma_vchan_issue_pending(struct virt_dma_chan *vc)
{
	list_splice_tail_init(&vc->desc_submitted, &vc->desc_issued);
	return !list_empty(&vc->desc_issued);
}

/**
 * innodma_vchan_cookie_complete - report completion of a descriptor
 * @vd: virtual descriptor to update
 *
 * vc.lock must be held by caller
 */
static inline void innodma_vchan_cookie_complete(struct virt_dma_desc *vd)
{
	struct virt_dma_chan *vc = innodma_to_virt_chan(vd->tx.chan);
	dma_cookie_t cookie;

	cookie = vd->tx.cookie;
	dma_cookie_complete(&vd->tx);
	dev_vdbg(vc->chan.device->dev, "txd %p[%x]: marked complete\n",
		 vd, cookie);
	list_add_tail(&vd->node, &vc->desc_completed);

	tasklet_schedule(&vc->task);
}

/**
 * innodma_vchan_next_desc - peek at the next descriptor to be processed
 * @vc: virtual channel to obtain descriptor from
 *
 * vc.lock must be held by caller
 */
static inline struct virt_dma_desc *innodma_vchan_next_desc(struct virt_dma_chan *vc)
{
	return list_first_entry_or_null(&vc->desc_issued,
					struct virt_dma_desc, node);
}

/**
 * innodma_vchan_get_all_descriptors - obtain all submitted and issued descriptors
 * @vc: virtual channel to get descriptors from
 * @head: list of descriptors found
 *
 * vc.lock must be held by caller
 *
 * Removes all submitted and issued descriptors from internal lists, and
 * provides a list of all descriptors found
 */
static inline void innodma_vchan_get_all_descriptors(struct virt_dma_chan *vc,
	struct list_head *head)
{
	list_splice_tail_init(&vc->desc_submitted, head);
	list_splice_tail_init(&vc->desc_issued, head);
	list_splice_tail_init(&vc->desc_completed, head);
}

static inline void innodma_vchan_free_chan_resources(struct virt_dma_chan *vc)
{
	struct virt_dma_desc *vd;
	unsigned long flags;
	LIST_HEAD(head);

	spin_lock_irqsave(&vc->lock, flags);
	innodma_vchan_get_all_descriptors(vc, &head);
	list_for_each_entry(vd, &head, node)
		dmaengine_desc_clear_reuse(&vd->tx);
	spin_unlock_irqrestore(&vc->lock, flags);

	innodma_vchan_dma_desc_free_list(vc, &head);
}
#endif
