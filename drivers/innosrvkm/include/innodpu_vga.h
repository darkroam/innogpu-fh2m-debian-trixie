/*************************************************************************/ /*!
@File			innodpu_vga.h
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
#ifndef __INNODPU_VGA_H
#define __INNODPU_VGA_H

#include <drm/drm_connector.h>
#include <drm/drm_encoder.h>
#include "innodpu_common.h"
#include "hal_interface.h"
#include "innodpu_compatibility.h"
#include "inno_debug.h"
#include "innodpu_vga_common.h"

#define to_inno_vga(x) container_of(x, struct vga_device_t, x)

struct vga_device_t {
	char *name;
	unsigned int vga_id;
	struct device *dev;
	struct drm_device *drm_dev;
	struct drm_connector connector;
	struct drm_encoder encoder;
	unsigned int conn_type;	// DRM_MODE_CONNECTOR_XX
	struct delayed_work hpdwk;
	struct workqueue_struct *hpdwq;

	struct vga_chip_t chip;

	struct drm_display_mode current_mode; // pdp->max_mode from current_mode
	struct drm_display_mode native_mode;

	struct i2c_adapter *ddc;
	struct i2c_adapter *pmbus_adapter;

	atomic_t adapt_cnt;
	inno_mutex *adapt_lock;
	inno_waitqueue_head *adapt_wait;
	inno_workqueue *auto_setup_wq;
	struct delayed_work auto_setup_work;
	struct list_head auto_setup_list;
	inno_mutex *auto_setup_mutex;
	struct delayed_work hpd_poll_work;
	bool auto_setup_enable;
	bool hpd_poll_en;
};

#define vga_poll_timeout(vga, entity, val, cond, sleep_us, timeout_us)  \
({ \
	ktime_t timeout = ktime_add_us(ktime_get(), timeout_us); \
	might_sleep_if(timeout_us); \
	for (;;) { \
		fh2m_hal_reg_read32(vga->dev->parent, vga->chip.reg_module, entity, &val); \
		if (cond) \
			break; \
		if (timeout_us && ktime_compare(ktime_get(), timeout) > 0) { \
			fh2m_hal_reg_read32(vga->dev->parent, vga->chip.reg_module, entity, &val); \
			break; \
		} \
		if (sleep_us) { \
			fh2m_inno_usleep_range(DIV_ROUND_UP(sleep_us, 4), sleep_us); \
		} \
	} \
	(cond) ? 0 : -ETIMEDOUT;  \
})

extern int g1p_soc_vga_chip_init(struct vga_chip_t *chip,
	inno_dev *dev, unsigned int vga_id, inno_drm_device *drm_dev);
extern void g1p_soc_vga_chip_fini(struct vga_chip_t *chip);
extern int g0m_soc_vga_chip_init(struct vga_chip_t *chip,
	inno_dev *dev, unsigned int vga_id, inno_drm_device *drm_dev);
extern void g0m_soc_vga_chip_fini(struct vga_chip_t *chip);

#endif//__INNODPU_VGA_H

