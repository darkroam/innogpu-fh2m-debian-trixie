/*************************************************************************/ /*!
@File			g3_pdp0_hw.c
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

#include "g3_pdp0_hw.h"
extern unsigned int s_combi_dual_sel;
extern unsigned int s_hw_cursor;

bool g3_pdp_filter(int dpu_id)
{
	bool ret = false;

	if ((dpu_id == 1) && (s_combi_dual_sel & BIT(0)))
		ret = true;

	if ((dpu_id == 3) && (s_combi_dual_sel & BIT(1)))
		ret = true;

	if ((dpu_id == 5) && (s_combi_dual_sel & BIT(3)))
		ret = true;

	return ret;
}


void g3_pdp0_hw_init(struct innodpu_pdp0_hw_device *hwdev, int dpu_id)
{
	hwdev->bus_align = 32;
	hwdev->setqos = true;
	hwdev->sethw_cursor = true;
	hwdev->sethw_patch = false;
//	hwdev->sethw_model = true;
	hwdev->setvga = false;

	hwdev->output_width[0] = 8;
	hwdev->output_width[1] = 8;
	hwdev->output_width[2] = 8;

	if ((dpu_id == 0 && (s_combi_dual_sel & BIT(0)))||
		 (dpu_id == 2 && (s_combi_dual_sel & BIT(1)))) {
		hwdev->features |= INNO_PDP_COMBINE;
		hwdev->combi = true;
		hwdev->max_width = 4096;
		hwdev->max_height = 4096;
	} else if((dpu_id == 4 && (s_combi_dual_sel & BIT(3)))) {
		hwdev->features |= INNO_PDP_COMBINE;
		hwdev->combi = true;
		hwdev->max_width = 7680;
		hwdev->max_height = 7680;
	} else {
		hwdev->combi = false;
		hwdev->max_width = 2048;
		hwdev->max_height = 2048;
	}

	hwdev->dual_link = false;

	//hwdev->color_channel_indep = true;
	//hwdev->gamma_bypass = true;

	if (s_hw_cursor & INNO_BIT(0)){
		hwdev->sethw_cursor = true;
	} else {
		hwdev->sethw_cursor = false;
	}

	hwdev->min_width = 2;
	hwdev->min_height = 2;

	hwdev->features |= INNO_PDP_PVRIC;
	hwdev->features |= INNO_PDP_ASYNC;
	hwdev->features |= INNO_PDP_REG_NEW;
	hwdev->features |= INNO_PDP_FUS_SCALER;
}
