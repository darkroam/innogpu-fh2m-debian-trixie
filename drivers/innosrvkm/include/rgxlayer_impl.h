/*************************************************************************/ /*!
@File
@Title          Header for DDK implementation of the Services abstraction layer
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Header for DDK implementation of the Services abstraction layer
@License        Strictly Confidential.
*/ /**************************************************************************/

#if !defined(RGXLAYER_IMPL_H)
#define RGXLAYER_IMPL_H

#include "rgxlayer.h"
#include "device_connection.h"

typedef struct _RGX_LAYER_PARAMS_
{
	void *psDevInfo;
	void *psDevConfig;
#if defined(PDUMP)
	IMG_UINT32 ui32PdumpFlags;
#endif

	IMG_DEV_PHYADDR sPCAddr;
#if defined(RGX_FEATURE_MIPS_BIT_MASK)
	IMG_DEV_PHYADDR sGPURegAddr;
	IMG_DEV_PHYADDR sBootRemapAddr;
	IMG_DEV_PHYADDR sCodeRemapAddr;
	IMG_DEV_PHYADDR sDataRemapAddr;
	IMG_DEV_PHYADDR sTrampolineRemapAddr;
	IMG_BOOL bDevicePA0IsValid;
#endif
} RGX_LAYER_PARAMS;

#endif /* RGXLAYER_IMPL_H */
