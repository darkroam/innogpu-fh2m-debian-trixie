/*************************************************************************/ /*!
@File			innodpu_hdmi.h
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
#ifndef __INNODPU_HDMI_H
#define __INNODPU_HDMI_H

#include <drm/drm_connector.h>
#include <drm/drm_encoder.h>
#include "innodpu_common.h"
#include "hal_interface.h"
#include "innodpu_compatibility.h"
#include "inno_debug.h"
#include "innodpu_parse_edid.h"
#include "innodpu_hdmi_common.h"
#include "innodpu_connector.h"

#include "inno_misc.h"
#include "inno_plat_dev.h"
#include "inno_task.h"
#include "inno_lock.h"
#include "inno_timer.h"
#include "inno_mm.h"
#include "inno_waitqueue.h"
#include "inno_drm.h"
#include "inno_drm_mode.h"
#include "inno_fs.h"

struct hdmi_device_i2c_t {
	struct hdmi_device_t *hdmi_dev;

	struct i2c_adapter adapter;
	struct i2c_algo_bit_data bit_algo;

	struct mutex mlock;
};

struct hdmi_device_t {
	char *name;
	int connector_type;
	unsigned int hdmi_id;
	struct device *dev;
	inno_dev    *parent;
	struct drm_device *drm_dev;
	struct drm_connector connector;
	struct drm_encoder encoder;
	unsigned int conn_type;	// DRM_MODE_CONNECTOR_XX
	atomic64_t modesetting;

	struct drm_display_mode native_mode;

	struct audio_conn *ac;
	struct delayed_work hotplug_work;
	struct workqueue_struct *hpdwq;
	int hpdout_cnt;

	bool pclk_invert;

	struct hdmi_chip_t chip;
	struct hdmi_device_i2c_t *i2c;
	unsigned char edid[EDID_LENGTH*2];
};


void *inno_hdmi_get_chip_adapater(struct hdmi_chip_t *chip);

int g0_soc_hdmi_chip_init(struct hdmi_chip_t *chip,
                inno_dev *dev, unsigned int hdmi_id);

void g0_soc_hdmi_chip_fini(struct hdmi_chip_t *chip);

int g1_soc_hdmi_chip_init(struct hdmi_chip_t *chip,
                inno_dev *dev, unsigned int hdmi_id);
void g1_soc_hdmi_chip_fini(struct hdmi_chip_t *chip);

int g1p_soc_hdmi_chip_init(struct hdmi_chip_t *chip,
                inno_dev *dev, unsigned int hdmi_id);
void g1p_soc_hdmi_chip_fini(struct hdmi_chip_t *chip);

int g0m_soc_hdmi_chip_init(struct hdmi_chip_t *chip,
                inno_dev *dev, unsigned int hdmi_id);

extern void g0m_soc_hdmi_chip_fini(struct hdmi_chip_t *chip);

extern int g3_ne_hdmi_chip_init(struct hdmi_chip_t *chip,
	                   inno_dev *dev, unsigned int hdmi_id);

extern void g3_ne_hdmi_chip_fini(struct hdmi_chip_t *chip);
#endif//__INNODPU_HDMI_H
