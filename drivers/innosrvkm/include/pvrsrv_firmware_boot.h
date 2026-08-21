/**************************************************************************/ /*!
@File
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Strictly Confidential.
*/ /***************************************************************************/

#ifndef PVRSRV_FIRMWARE_BOOT_H
#define PVRSRV_FIRMWARE_BOOT_H

#include "img_types.h"
#include "rgx_fwif_shared.h"

#define TD_MAX_NUM_MIPS_PAGETABLE_PAGES (4U)

typedef union _PVRSRV_FW_BOOT_PARAMS_
{
	struct
	{
		IMG_DEV_VIRTADDR sFWCodeDevVAddr;
		IMG_DEV_VIRTADDR sFWDataDevVAddr;
		IMG_DEV_VIRTADDR sFWCorememCodeDevVAddr;
		RGXFWIF_DEV_VIRTADDR sFWCorememCodeFWAddr;
		IMG_DEVMEM_SIZE_T uiFWCorememCodeSize;
		IMG_DEV_VIRTADDR sFWCorememDataDevVAddr;
		RGXFWIF_DEV_VIRTADDR sFWCorememDataFWAddr;
		IMG_UINT32 ui32NumThreads;
	} sMeta;

	struct
	{
		IMG_DEV_PHYADDR sGPURegAddr;
		IMG_DEV_PHYADDR asFWPageTableAddr[TD_MAX_NUM_MIPS_PAGETABLE_PAGES];
		IMG_DEV_PHYADDR sFWStackAddr;
		IMG_UINT32 ui32FWPageTableLog2PageSize;
		IMG_UINT32 ui32FWPageTableNumPages;
	} sMips;

	struct
	{
		IMG_DEV_VIRTADDR sFWCorememCodeDevVAddr;
		RGXFWIF_DEV_VIRTADDR sFWCorememCodeFWAddr;
		IMG_DEVMEM_SIZE_T uiFWCorememCodeSize;

		IMG_DEV_VIRTADDR sFWCorememDataDevVAddr;
		RGXFWIF_DEV_VIRTADDR sFWCorememDataFWAddr;
		IMG_DEVMEM_SIZE_T uiFWCorememDataSize;
	} sRISCV;

} PVRSRV_FW_BOOT_PARAMS;


#endif /* PVRSRV_FIRMWARE_BOOT_H */
