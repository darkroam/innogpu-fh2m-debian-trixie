/*************************************************************************/ /*!
@File			innodpu_module_param.c
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

#include "inno_drm_version.h"
#include <linux/component.h>
#include <linux/platform_device.h>

#include "innodpu_common.h"
#include "innodpu_module_param.h"

/* shared memory control param */
bool s_dpu_has_shared_mem = true;
module_param(s_dpu_has_shared_mem, bool, 0444);
MODULE_PARM_DESC(s_dpu_has_shared_mem, "dpu has shared mem or not (default: true)");

unsigned int s_dpu_shared_mem_size = 0x19000000;
module_param(s_dpu_shared_mem_size, uint, 0444);
MODULE_PARM_DESC(s_dpu_shared_mem_size, "dpu shared mem size (default 400Mbytes)");

bool s_dpu_has_gtt_mem = true;
module_param(s_dpu_has_gtt_mem, bool, 0444);
MODULE_PARM_DESC(s_dpu_has_gtt_mem, "dpu support gtt mem or not (default:true)");

bool s_dpu_gtt_mem_clear = true;
module_param(s_dpu_gtt_mem_clear, bool, 0444);
MODULE_PARM_DESC(s_dpu_gtt_mem_clear, "dpu gtt mem clear or not (default:true)");

bool s_dpu_has_nocontinuous_vram = true;
module_param(s_dpu_has_nocontinuous_vram, bool, 0444);
MODULE_PARM_DESC(s_dpu_has_nocontinuous_vram, "dpu support no continuous vram or not (default:false)");

bool s_dpu_support_smmu = false;
module_param(s_dpu_support_smmu, bool, 0444);
MODULE_PARM_DESC(s_dpu_support_smmu, "dpu support smmu or not (default:true)");

bool s_vga_auto_adapt = true;
module_param(s_vga_auto_adapt, bool, 0600);
MODULE_PARM_DESC(s_vga_auto_adapt, "vga automatic adaptation screen (default: true)");

unsigned int s_vga_hdelay = 0x08;
module_param(s_vga_hdelay, uint, 0600);
MODULE_PARM_DESC(s_vga_hdelay, "vga hsync delay [0-0xf (default: 0x08)");

unsigned int s_vga_vdelay = 0x08;
module_param(s_vga_vdelay, uint, 0600);
MODULE_PARM_DESC(s_vga_vdelay, "vga vsync delay [0-0xf (default: 0x08)");

/* supprot base fd ioctl */
bool s_dpu_support_plane_fd = true;
module_param(s_dpu_support_plane_fd, bool, 0444);
MODULE_PARM_DESC(s_dpu_support_plane_fd, "dpu support plane fd ioctl (default: true)");

bool s_coladj_force = false;
module_param(s_coladj_force, bool, 0600);
MODULE_PARM_DESC(s_coladj_force, "pdp enable coloradj force(default: false)");

bool s_dpu_not_use_shared_mem = false;
module_param(s_dpu_not_use_shared_mem, bool, 0444);
MODULE_PARM_DESC(s_dpu_not_use_shared_mem, "dpu gem not use shared mem (default: false)");

int s_dpu_water_line_adj = 8;
module_param(s_dpu_water_line_adj, int, 0600);
MODULE_PARM_DESC(s_dpu_water_line_adj, "pdp combine water line adjust parameter (default: 8)");


/* virtual resolution module param */
#ifdef TEST_DRM_VIRT
unsigned int drm_reso_virt = 0; // must set TEST_DRM_VIRT macro, HDMI0, HDMI1, DP used  BIT0,1,2
module_param(drm_reso_virt, uint, 0444);
MODULE_PARM_DESC(drm_reso_virt, "test drm_reso_virt, default(0),bitmap\n"
		"\t\tHDMI0_ZOOM_ENABLE,\n"
		"\t\tHDMI1_ZOOM_ENABLE,\n"
		"\t\ttDP0_ZOOM_ENABLE,\n"
		"\t\ttLVDS_ZOOM_ENABLE,\n"
		"\t\ttHDMI2_ZOOM_ENABLE,\n"
		"\t\ttHDMI3_ZOOM_ENABLE,\n"
		"\t\ttDP1_ZOOM_ENABLE,\n"
		"\t\ttVGA_ZOOM_ENABLE,");
#endif
