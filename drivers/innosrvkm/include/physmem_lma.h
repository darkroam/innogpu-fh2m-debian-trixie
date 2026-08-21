/**************************************************************************/ /*!
@File
@Title          Header for local card memory allocator
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
                Additional changes in this file are
                Copyright (c) Innosilicon Technology Ltd. All Rights Reserved
@Description    Part of the memory management. This module is responsible for
                implementing the function callbacks for local card memory.
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
*/ /***************************************************************************/

#ifndef SRVSRV_PHYSMEM_LMA_H
#define SRVSRV_PHYSMEM_LMA_H

/* include/ */
#include "img_types.h"
#include "pvrsrv_error.h"
#include "pvrsrv_memallocflags.h"
#include "device.h"

/* services/server/include/ */
#include "pmr.h"
#include "pmr_impl.h"


/*************************************************************************/ /*!
@Function       PhysmemCreateHeapLMA
@Description    Create and register new LMA heap with LMA specific details.
@Input          psDevNode    Pointer to device node struct.
@Input          uiPolicy     Heap allocation policy flags
@Input          psConfig     Heap configuration.
@Input          pszLabel     Debug identifier label
@Output         ppsPhysHeap  Pointer to the created heap.
@Return         PVRSRV_ERROR PVRSRV_OK or error code
*/ /**************************************************************************/
PVRSRV_ERROR
PhysmemCreateHeapLMA(PVRSRV_DEVICE_NODE *psDevNode,
                     PHYS_HEAP_POLICY uiPolicy,
                     PHYS_HEAP_CONFIG *psConfig,
                     IMG_CHAR *pszLabel,
                     PHYS_HEAP **ppsPhysHeap);


typedef struct _PHYSMEM_LMA_EXTERNAL_DATA_
{
	/* Device for which this allocation has been made */
	PVRSRV_DEVICE_NODE *psDevNode;
	/* The pid that made this allocation */
	IMG_PID uiPid;

	/*
	 * uiNumPagesAllocated:
	 * Number of pages allocated in this PMR so far.
	 * This allows for up to (2^31 - 1) pages. With 4KB pages, that's 8TB of memory for each PMR.
	 */
	IMG_UINT32 uiNumPagesAllocated;
	IMG_UINT64 uiLog2AllocSize;

	IMG_BOOL bZero;
	IMG_BOOL bNonContig;
	IMG_BOOL bPoisonOnAlloc;
	IMG_BOOL bPoisonOnFree;

	IMG_UINT64 *pasDevPAddr;
	RA_BASE_ARRAY_T aBaseArray;
} Physmem_LMA_EXTERNAL_DATA;
/*
 * Physmem_Externl_Alloc
 *
 * This function will alloc buffer from physical heap for device
 */
PVRSRV_ERROR
Physmem_Externl_Alloc(PVRSRV_DEVICE_NODE *psDevNode,
                      PHYS_HEAP_USAGE_FLAGS ui32HeapFlags,
                      PHYS_HEAP_USAGE_FLAGS ui32RequestHeapFlags,
                      PVRSRV_MEMALLOCFLAGS_T uiMemAllocFlags,
                      IMG_UINT32 id,
                      IMG_UINT64 size,
                      IMG_UINT64 *puiCardAddr,
                      IMG_UINT64 *puiActualSize,
                      Physmem_LMA_EXTERNAL_DATA *psPhysmemLMAExtData);

/*
 * Physmem_Externl_Alloc
 *
 * This function will free buffer for device
 */
PVRSRV_ERROR
Physmem_Externl_Free(PVRSRV_DEVICE_NODE *psDevNode,
                     PHYS_HEAP_USAGE_FLAGS ui32HeapFlags,
                     IMG_UINT32 id,
                     IMG_UINT64 base,
                     IMG_UINT64 *size,
                     Physmem_LMA_EXTERNAL_DATA *psPhysmemLMAExtData);

void
Physmem_Externl_AddMem_Stats(PVRSRV_DEVICE_NODE *psDevNode, PHYS_HEAP_USAGE_FLAGS ui32HeapFlags, IMG_UINT64 size);

void
Physmem_Externl_DecrMem_Stats(PVRSRV_DEVICE_NODE *psDevNode, PHYS_HEAP_USAGE_FLAGS ui32HeapFlags, IMG_UINT64 size);

void
Physmem_Externl_AddInvMem_Stats(PVRSRV_DEVICE_NODE *psDevNode, PHYS_HEAP_USAGE_FLAGS ui32HeapFlags, IMG_UINT64 size);

void
Physmem_Externl_DecrInvMem_Stats(PVRSRV_DEVICE_NODE *psDevNode, PHYS_HEAP_USAGE_FLAGS ui32HeapFlags, IMG_UINT64 size);

PVRSRV_ERROR
PhysmemGetArenaLMA(PHYS_HEAP *psPhysHeap, RA_ARENA **ppsArena);

IMG_INT GetMultiMemRegionsEnable(void);

#endif /* #ifndef SRVSRV_PHYSMEM_LMA_H */
