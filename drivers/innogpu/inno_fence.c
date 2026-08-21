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

#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/kref.h>
#include <linux/version.h>
#include "inno_misc.h"
#include "inno_fence.h"
#include "inno_mm.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0)
#include <linux/sync_file.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/anon_inodes.h>
#include <linux/fence.h>
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0)
#include <linux/dma-fence.h>
#include <linux/dma-fence-array.h>
#include <linux/sync_file.h>
#include <linux/file.h>
#include <linux/kernel.h>
#endif

struct inno_kref {
	struct kref kref;
	void (*destroy)(void *data);
	void *data;
};

static void inno_kref_destory_wrapper(struct kref *kref)
{
	struct inno_kref *k = container_of(kref, struct inno_kref, kref);
	WARN_ON(!k);

	k->destroy(k->data);
}

struct inno_kref *fh2m_inno_kref_alloc(void (*destroy)(void *data), void *data)
{
	struct inno_kref *kref = kzalloc(sizeof(struct inno_kref), GFP_KERNEL);
	if (!kref)
		return NULL;

	kref->destroy = destroy;
	kref->data = data;
	kref_init(&kref->kref);
	return kref;
}
INNO_EXT_SYM(fh2m_inno_kref_alloc);

void fh2m_inno_kref_free(struct inno_kref *kref)
{
	kfree(kref);
}
INNO_EXT_SYM(fh2m_inno_kref_free);

int fh2m_inno_kref_put(struct inno_kref *kref)
{
	return kref_put(&kref->kref, inno_kref_destory_wrapper);
}
INNO_EXT_SYM(fh2m_inno_kref_put);

void fh2m_inno_kref_get(struct inno_kref *kref)
{
	kref_get(&kref->kref);
}
INNO_EXT_SYM(fh2m_inno_kref_get);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0)
inno_dma_fence* fh2m_inno_dma_fence_alloc()
{
	return kmalloc(sizeof(struct dma_fence), GFP_KERNEL);
}
INNO_EXT_SYM(fh2m_inno_dma_fence_alloc);

void fh2m_inno_dma_fence_init(inno_dma_fence *fence, const void *ops, inno_spinlock *lock, unsigned context, unsigned seqno)
{
	dma_fence_init((struct dma_fence*)fence,
					(struct dma_fence_ops*)ops,
					(spinlock_t*)lock,
					dma_fence_context_alloc(context),
					seqno);
}
INNO_EXT_SYM(fh2m_inno_dma_fence_init);

int fh2m_inno_dma_fence_signal(inno_dma_fence *fence)
{
	return dma_fence_signal((struct dma_fence *)fence);
}
INNO_EXT_SYM(fh2m_inno_dma_fence_signal);

bool fh2m_inno_dma_fence_is_signaled(inno_dma_fence *fence)
{
	return dma_fence_is_signaled((struct dma_fence*)fence);
}
INNO_EXT_SYM(fh2m_inno_dma_fence_is_signaled);

void fh2m_inno_dma_fence_free(inno_dma_fence* f)
{
	return fh2m_inno_kfree(f);
}
INNO_EXT_SYM(fh2m_inno_dma_fence_free);

void fh2m_inno_os_free_dma_fence(inno_dma_fence *fence)
{
	dma_fence_free((struct dma_fence*)fence);
}
INNO_EXT_SYM(fh2m_inno_os_free_dma_fence);

void fh2m_inno_dma_fence_set_error(inno_dma_fence *fence, int error)
{
	dma_fence_set_error((struct dma_fence *)fence, error);
}
INNO_EXT_SYM(fh2m_inno_dma_fence_set_error);

int fh2m_inno_dma_fence_get_error(inno_dma_fence *fence)
{
	return ((struct dma_fence*)fence)->error;
}
INNO_EXT_SYM(fh2m_inno_dma_fence_get_error);

void fh2m_inno_dma_fence_put(inno_dma_fence *fence)
{
	dma_fence_put((struct dma_fence *)fence);
}
INNO_EXT_SYM(fh2m_inno_dma_fence_put);

inno_dma_fence *fh2m_inno_dma_fence_get(inno_dma_fence *fence)
{
	return dma_fence_get((struct dma_fence *)fence);
}
INNO_EXT_SYM(fh2m_inno_dma_fence_get);

inno_dma_fence *fh2m_inno_sync_file_get_fence(int fd)
{
	return (inno_dma_fence *)sync_file_get_fence(fd);
}
INNO_EXT_SYM(fh2m_inno_sync_file_get_fence);

uint64_t fh2m_inno_dma_fence_context_alloc(unsigned num)
{
	return dma_fence_context_alloc(num);
}
INNO_EXT_SYM(fh2m_inno_dma_fence_context_alloc);

inno_dma_fence_array *fh2m_inno_dma_fence_array_create(int num_fences, inno_dma_fence **fences, uint64_t context, unsigned seqno, bool signal_on_any)
{
	return (inno_dma_fence_array *)dma_fence_array_create(num_fences, (struct dma_fence **)fences, context, seqno, signal_on_any);
}
INNO_EXT_SYM(fh2m_inno_dma_fence_array_create);

inno_dma_fence * fh2m_inno_dma_fence_array_get_base(inno_dma_fence_array *fence_array)
{
	inno_dma_fence *dma_fence = &((struct dma_fence_array *)fence_array)->base;
	return dma_fence;
}
INNO_EXT_SYM(fh2m_inno_dma_fence_array_get_base);

inno_sync_file *fh2m_inno_sync_file_create(inno_dma_fence *fence)
{
	inno_sync_file *sync_file = sync_file_create((struct dma_fence *)fence);
	return sync_file;
}
INNO_EXT_SYM(fh2m_inno_sync_file_create);

void *fh2m_inno_sync_file_get_file(inno_sync_file *sync_file)
{
	struct file *file = ((struct sync_file*)sync_file)->file;
	return file;
}
INNO_EXT_SYM(fh2m_inno_sync_file_get_file);

#endif

