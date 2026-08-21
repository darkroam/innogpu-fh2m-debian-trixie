/*************************************************************************/ /*!
@File           sysconfig.h
@Title          System Configuration
@Copyright      Copyright (c) Innosilicon Technology Ltd. All Rights Reserved
@Description    System Configuration functions
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

#ifndef __COMMON_INNOGPU_BRIDGE_H__
#define __COMMON_INNOGPU_BRIDGE_H__

#include "innogpu_defs.h"

#define PVRSRV_BRIDGE_INNOGPU_CMD_FIRST      (0)
#define PVRSRV_BRIDGE_INNOGPU_GETCHIPINFOCNT (PVRSRV_BRIDGE_INNOGPU_CMD_FIRST + 1)   /* get chip info array count */
#define PVRSRV_BRIDGE_INNOGPU_GETCHIPINFO    (PVRSRV_BRIDGE_INNOGPU_CMD_FIRST + 2)   /* get chip info array */
#define PVRSRV_BRIDGE_INNOGPU_CMD_LAST       (PVRSRV_BRIDGE_INNOGPU_CMD_FIRST + 3)

/*******************************************
            GetChipInfoCount
 *******************************************/
/* Bridge in structure for GetChipInfoCount */
typedef struct PVRSRV_BRIDGE_IN_GETCHIPINFOCNT_TAG
{
	IMG_UINT32 ui32EmptyStructPlaceholder;
} __packed PVRSRV_BRIDGE_IN_GETCHIPINFOCNT;

/* Bridge out structure for GetChipInfoCount */
typedef struct PVRSRV_BRIDGE_OUT_GETCHIPINFOCNT_TAG
{
	PVRSRV_ERROR eError;
	IMG_UINT32 ui32ChipInfoCount;
} __packed PVRSRV_BRIDGE_OUT_GETCHIPINFOCNT;

/*******************************************
            GetChipInfo
 *******************************************/
/* Bridge in structure for GetChipInfo */
typedef struct PVRSRV_BRIDGE_IN_GETCHIPINFO_TAG
{
	IMG_UINT32 ui32EmptyStructPlaceholder;
} __packed PVRSRV_BRIDGE_IN_GETCHIPINFO;

/* Bridge out structure for GetChipInfo */
typedef struct PVRSRV_BRIDGE_OUT_GETCHIPINFO_TAG
{
	PVRSRV_ERROR eError;
	IMG_UINT32 ui32ChipInfo[INNOGPU_INFO_IDX_END];
} __packed PVRSRV_BRIDGE_OUT_GETCHIPINFO;

#endif /* __COMMON_INNOGPUINFO_BRIDGE_H__ */
