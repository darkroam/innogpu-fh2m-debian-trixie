/*************************************************************************/ /*!
@File           pvrversion.h
@Title          PowerVR version numbers and strings.
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Version numbers and strings for PowerVR components.
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

#ifndef PVRVERSION_H
#define PVRVERSION_H

#include "innoversion.h"

#if defined(DESENSITIZED) && (DESENSITIZED == 2)
#define PVRVERSION_MAJ               1U
#define PVRVERSION_MIN               0U
#define PVRVERSION_BRANCHNAME       "1.0"
#define PVRVERSION_FAMILY           "gtddk"
#define PVRVERSION_BSCONTROL        "Gt_DDK_Linux_WS"

#define PVRVERSION_STRING           INNOVERSION_STRING_Gt
#define PVRVERSION_STRING_SHORT     INNOVERSION_STRING_SHORT_Gt

#define COPYRIGHT_TXT               "Copyright (c) GT Technologies Ltd. All Rights Reserved."

//#define PVRVERSION_BUILD_HI          0
//#define PVRVERSION_BUILD_LO          0
//#define PVRVERSION_STRING_NUMERIC   "1.0.0.0"
#elif defined(DESENSITIZED) && (DESENSITIZED == 1)
#define PVRVERSION_MAJ               1U
#define PVRVERSION_MIN               0U
#define PVRVERSION_BRANCHNAME       "1.0"
#define PVRVERSION_FAMILY           "awmddk"
#define PVRVERSION_BSCONTROL        "Awm_DDK_Linux_WS"

#define PVRVERSION_STRING           INNOVERSION_STRING_Awm
#define PVRVERSION_STRING_SHORT     INNOVERSION_STRING_SHORT_Awm

#define COPYRIGHT_TXT               "Copyright (c) AWM Technologies Ltd. All Rights Reserved."

//#define PVRVERSION_BUILD_HI          0
//#define PVRVERSION_BUILD_LO          0
//#define PVRVERSION_STRING_NUMERIC   "1.0.0.0"
#else
#define PVRVERSION_MAJ               23U
#define PVRVERSION_MIN               3U
#define PVRVERSION_BRANCHNAME       INNOVERSION_BRANCHNAME
#define PVRVERSION_FAMILY           "rogueddk"
#define PVRVERSION_BSCONTROL        "Rogue_DDK_Linux_WS"

#define PVRVERSION_STRING           INNOVERSION_STRING
#define PVRVERSION_STRING_SHORT     INNOVERSION_STRING_SHORT

#define COPYRIGHT_TXT               "Copyright (c) Imagination Technologies Ltd. All Rights Reserved."

//#define PVRVERSION_BUILD_HI          634
//#define PVRVERSION_BUILD_LO          5021
//#define PVRVERSION_STRING_NUMERIC   "1.19.634.5021"
#endif

#define PVRVERSION_BUILD            INNOVERSION_REVISION_V

#define PVRVERSION_PACK(MAJOR,MINOR) (((IMG_UINT32)((IMG_UINT32)(MAJOR) & 0xFFFFU) << 16U) | (((MINOR) & 0xFFFFU) << 0U))
#define PVRVERSION_UNPACK_MAJ(VERSION) (((VERSION) >> 16U) & 0xFFFFU)
#define PVRVERSION_UNPACK_MIN(VERSION) (((VERSION) >> 0U) & 0xFFFFU)

#endif /* PVRVERSION_H */
