/*
 ** * Copyright (c) Innosilicon Technology Ltd. All Rights Reserved
 ** * Dual MIT/GPLv2
 ** *
 ** * The contents of this file are subject to the MIT license as set out below.
 ** *
 ** * Permission is hereby granted, free of charge, to any person obtaining a copy
 ** * of this software and associated documentation files (the "Software"), to deal
 ** * in the Software without restriction, including without limitation the rights
 ** * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 ** * copies of the Software, and to permit persons to whom the Software is
 ** * furnished to do so, subject to the following conditions:
 ** *
 ** * The above copyright notice and this permission notice shall be included in
 ** * all copies or substantial portions of the Software.
 ** *
 ** * Alternatively, the contents of this file may be used under the terms of
 ** * the GNU General Public License Version 2 ("GPL") in which case the provisions
 ** * of GPL are applicable instead of those above.
 ** *
 ** * If you wish to allow use of your version of this file only under the terms of
 ** * GPL, and not to allow others to use your version of this file under the terms
 ** * of the MIT license, indicate your decision by deleting the provisions above
 ** * and replace them with the notice and other provisions required by GPL as set
 ** * out in the file called "GPL-COPYING" included in this distribution. If you do
 ** * not delete the provisions above, a recipient may use your version of this file
 ** * under the terms of either the MIT license or GPL.
 ** *
 ** * This License is also included in this distribution in the file called
 ** * "MIT-COPYING".
 ** *
 ** * EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
 ** * PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 ** * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 ** * PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
 ** * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 ** * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 ** * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 ** */
#ifndef __INNO_IDR_H__
#define __INNO_IDR_H__
#include <linux/types.h>

typedef void inno_idr;

inno_idr *fh2m_inno_idr_kalloc(void);

void fh2m_inno_idr_init(inno_idr *innoidr);

void fh2m_inno_idr_free(inno_idr *innoidr);

int fh2m_inno_idr_alloc(inno_idr *innoidr, void *ptr, int start, int end, int *id);

int fh2m_inno_idr_alloc_cyclic(inno_idr *innoidr, void *entry, int start, int end);

void * fh2m_inno_idr_get_next(inno_idr *innoidr, int *nextid);

void * fh2m_inno_idr_find(inno_idr *innoidr, int id);

bool fh2m_inno_idr_is_empty(inno_idr *innoidr);

void * fh2m_inno_idr_remove(inno_idr *innoidr, int id);

void fh2m_inno_idr_destroy(inno_idr *innoidr);

int fh2m_inno_idr_foreach(inno_idr *innoidr,int (*function)(int id, void *p, void *data), void *data);

void *fh2m_inno_idr_replace(inno_idr *innoidr, void *pvData, int id);

#define inno_idr_for_each_entry(idr, entry, id) \
     for (id = 0; ((entry) = fh2m_inno_idr_get_next(idr, &(id))) != NULL; id += 1U)

#endif
