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
#include <linux/timer.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/delay.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0))
#include <linux/sched/clock.h>
#include <linux/sched/signal.h>
#else
#include <linux/sched.h>
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)) */

#include "inno_misc.h"
#include "inno_task.h"

#include "inno_waitqueue.h"

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 13, 0))
#define wait_queue_entry __wait_queue
#define wait_queue_head __wait_queue_head
#endif

void *fh2m_inno_waitqueue_entry_alloc(void)
{
	struct wait_queue_entry *entry = (struct wait_queue_entry *)
		kzalloc(sizeof(struct wait_queue_entry), GFP_KERNEL);

	if (!entry)
		return NULL;

	return entry;
}
INNO_EXT_SYM(fh2m_inno_waitqueue_entry_alloc);

void fh2m_inno_waitqueue_entry_free(void *wq_entry)
{
	kfree(wq_entry);
}
INNO_EXT_SYM(fh2m_inno_waitqueue_entry_free);

void *fh2m_inno_waitqueue_head_alloc(void)
{
	struct wait_queue_head *head = (struct wait_queue_head *)
			kzalloc(sizeof(struct wait_queue_head), GFP_KERNEL);

	if (!head)
		return NULL;

	return head;
}
INNO_EXT_SYM(fh2m_inno_waitqueue_head_alloc);

void fh2m_inno_waitqueue_head_free(void *head)
{
	kfree(head);
}
INNO_EXT_SYM(fh2m_inno_waitqueue_head_free);


void fh2m_inno_init_waitqueue_head(inno_waitqueue_head *wq)
{
	init_waitqueue_head((struct wait_queue_head *)wq);
}
INNO_EXT_SYM(fh2m_inno_init_waitqueue_head);

void fh2m_inno_wake_up_interruptible(inno_waitqueue_head *wq)
{
	wake_up_interruptible((struct wait_queue_head *)wq);
}
INNO_EXT_SYM(fh2m_inno_wake_up_interruptible);

void fh2m_inno_wake_up_all(inno_waitqueue_head *wq)
{
	wake_up_all((struct wait_queue_head *)wq);
}
INNO_EXT_SYM(fh2m_inno_wake_up_all);

void fh2m_inno_init_wait_entry(void *wq_entry, int flags)
{
	init_wait_entry(wq_entry, flags);
}
INNO_EXT_SYM(fh2m_inno_init_wait_entry);

long fh2m_inno_prepare_to_wait_event(inno_waitqueue_head *wq_head,
                                        void *wq_entry, int state)
{
	return prepare_to_wait_event((struct wait_queue_head *)wq_head, wq_entry, state);
}
INNO_EXT_SYM(fh2m_inno_prepare_to_wait_event);

void fh2m_inno_finish_wait(inno_waitqueue_head *wq_head, void *wq_entry)
{
	finish_wait((struct wait_queue_head *)wq_head, wq_entry);
}
INNO_EXT_SYM(fh2m_inno_finish_wait);

void fh2m_inno_wake_up(inno_waitqueue_head *wq_head)
{
    struct wait_queue_head *head = (struct wait_queue_head *)wq_head;

    wake_up(head);
}
INNO_EXT_SYM(fh2m_inno_wake_up);

