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
#ifndef __INNO_FENCE_H__
#define __INNO_FENCE_H__

#include "inno_lock.h"

typedef void inno_dma_fence;
typedef void inno_sync_file;
typedef void inno_dma_fence_array;

struct inno_kref;

struct inno_kref *fh2m_inno_kref_alloc(void (*destroy)(void *data), void *data);

void fh2m_inno_kref_free(struct inno_kref *kref);

int fh2m_inno_kref_put(struct inno_kref *kref);

void fh2m_inno_kref_get(struct inno_kref *kref);

void fh2m_inno_dma_fence_set_error(inno_dma_fence *fence, int error);

int fh2m_inno_dma_fence_get_error(inno_dma_fence *fence);

void fh2m_inno_dma_fence_put(inno_dma_fence *fence);

inno_dma_fence *fh2m_inno_dma_fence_get(inno_dma_fence *fence);

inno_dma_fence* fh2m_inno_dma_fence_alloc(void);

void fh2m_inno_dma_fence_init(inno_dma_fence *fence, const void *ops, inno_spinlock *lock, unsigned context, unsigned seqno);

int fh2m_inno_dma_fence_signal(inno_dma_fence *fence);

bool fh2m_inno_dma_fence_is_signaled(inno_dma_fence *fence);

void fh2m_inno_dma_fence_free(inno_dma_fence* f);

void fh2m_inno_os_free_dma_fence(inno_dma_fence *fence);

inno_dma_fence *fh2m_inno_sync_file_get_fence(int fd);

uint64_t fh2m_inno_dma_fence_context_alloc(unsigned num);

inno_dma_fence_array *fh2m_inno_dma_fence_array_create(int num_fences, inno_dma_fence **fences, uint64_t context, unsigned seqno, bool signal_on_any);

inno_dma_fence *fh2m_inno_dma_fence_array_get_base(inno_dma_fence_array *fence_array);

inno_sync_file *fh2m_inno_sync_file_create(inno_dma_fence *fence);

void *fh2m_inno_sync_file_get_file(inno_sync_file *sync_file);

#endif

