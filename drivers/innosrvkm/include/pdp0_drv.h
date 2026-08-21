/*************************************************************************/ /*!
@File			pdp0_drv.h
@Title
@Copyright		Copyright (c) Innosilicon Technology Ltd. All Rights Reserved
@Description
@License		Dual MIT/GPLv2

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
#ifndef __PDP0_DRV_H
#define __PDP0_DRV_H
#include "innodpu_common.h"
#include "pdp0_hw.h"

//#define PDP0_WRITEBACK
//#define PDP0_SRC_MW_EN
//#define PDP0_PVR_MW_EN
//#define PDP0_WF_MW_EN

//#define USE_ORIGINAL_DRIVER
#define PDP0_CONFIG_VALID_INIT	0
#define PDP0_CONFIG_VALID_DONE	1
#define PADDING_ALIGN_SIZE		(32)

struct pdp0_crtc_state {
	struct drm_crtc_state base;
	u32 gamma_coeffs[PDP0_COEFFTAB_NUM_COEFFS];
	u32 coloradj_coeffs[PDP0_COLORADJ_NUM_COEFFS];
	bool coladj_en;
	struct pdp0_se_config scaler_config;
	/* Bitfield of all the planes that have requested a scaled output. */
	u8 scaled_planes_mask;

	struct pdp0_crtc_config priv_config;
};
#define to_pdp0_crtc_state(x) container_of(x, struct pdp0_crtc_state, base)

struct pdp0_plane {
	u8 id;
	enum drm_plane_type plane_type;
	struct drm_plane base;
	struct innodpu_pdp0_hw_device *hwdev;
	const struct pdp0_layer *layer;
	struct drm_crtc *crtc;
};
#define to_pdp0_plane(x) container_of(x, struct pdp0_plane, base)

struct pdp0_plane_state {
	struct drm_plane_state base;
	/* size of the required rotation memory if plane is rotated */
	u32 rotmem_size;
	/* internal format ID */
	u8 format;
	u8 n_planes;
	struct pdp0_plane_config priv_config;
};
#define to_pdp0_plane_state(x) container_of(x, struct pdp0_plane_state, base)

void innodpu_pdp0_unbind(struct device *dev, struct device *master,
 void *data, bool null_display);
int innodpu_pdp0_bind(struct device *dev, struct device *master,
 void *data, bool null_display);
int innodpu_pdp0_suspend(struct device *dev, int dpu_id);
int innodpu_pdp0_resume(struct device *dev, int dpu_id);

#endif //__PDP0_DRV_H
