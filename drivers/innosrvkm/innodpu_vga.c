#include "../innogpu/compat_kernel6.h"
/*************************************************************************/ /*!
@File			innodpu_vga.c
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

#include <linux/platform_device.h>
#include "pdp0_crtc.h"
#include "innodpu_vga.h"
#include "innodpu_connector.h"
#include "innodpu_vga_debugfs.h"
#include "innopmbus_drv.h"

static bool s_vga_ddcci = true;
module_param(s_vga_ddcci, bool, 0600);
MODULE_PARM_DESC(s_vga_ddcci, "vga ddcci-pmbus (default: true)");

static int s_vga_monitor = 0x15;
module_param(s_vga_monitor, int, 0600);
MODULE_PARM_DESC(s_vga_monitor,
		"\n\t\tBit(1-0) ddc channels selection\n"
		"\t\t\t(0x0) ddc channels none\n"
		"\t\t\t(0x1) ddc channels use pmbus access enable\n"
		"\t\t\t(0x2) ddc channels use GPIOs to emulate i2c enable\n"
		"\t\t\t(0x3) ddc channels use tne native ddcci enable\n"
		"\t\t Bit(3-2) connection status detection selection\n"
		"\t\t\t(0x0) connection status detection using cable detect\n"
		"\t\t\t(0x1) connection status detection using ddc channel\n"
		"\t\t Bit(5-4) vga autoset set enable\n"
		"\t\t\t(0x0) vga auto setup disable\n"
		"\t\t\t(0x1) vga auto setup enable\n"
		"\t\t Bit(7-6) colorbar test enable\n"
		"\t\t\t(0x0) color bar disable\n"
		"\t\t\t(0x1) color bar enable\n"
		"\t\t Bit(9-8) vga auto setup every mode\n"
		"\t\t\t(0x0) false\n"
		"\t\t\t(0x1) en\n");

#define VGA_MON_EN(mon_item, mode) \
	(((s_vga_monitor >> mon_item * 2) & VGA_MONITOR_MASK) == mode)

#define VGA_MON_CLEAR(mon_item) \
	(s_vga_monitor &= ~(VGA_MONITOR_MASK << (2 * mon_item)))

#define VGA_MON_SET(mon_item, mode) \
	s_vga_monitor |= (mode << (2 * mon_item))

static RAW_NOTIFIER_HEAD(vga_notifier);
void vga_notifier_register(struct notifier_block *nb)
{
	raw_notifier_chain_register(&vga_notifier, nb);
}
void vga_notifier_unregister(struct notifier_block *nb)
{
	raw_notifier_chain_unregister(&vga_notifier, nb);
}

#if ((DRM_VERSION >= KERNEL_VERSION(4, 13, 0)))
static enum drm_mode_status inno_vga_encoder_mode_valid(
		struct drm_encoder *encoder, const struct drm_display_mode *mode)
{
	struct vga_device_t *inno_vga = to_inno_vga(encoder);

	fh2m_innodpu_info(inno_vga->dev, DPU_UT_VGA,
		"%s encoder mode valid resolution: %dx%d@%d",
		inno_vga->name, mode->hdisplay, mode->vdisplay, drm_mode_vrefresh(mode));

	if (inno_vga->chip.encoder_mode_valid)
		return inno_vga->chip.encoder_mode_valid(&inno_vga->chip, mode);

	return MODE_OK;
}
#endif

static bool inno_vga_encoder_mode_fixup(struct drm_encoder *encoder,
		const struct drm_display_mode *mode, struct drm_display_mode *adjusted_mode)
{
	struct vga_device_t *inno_vga
		= container_of(encoder, struct vga_device_t, encoder);

	if (is_native_mode_valid(&inno_vga->native_mode) && is_virtual_mode(&inno_vga->native_mode, adjusted_mode)) {
		drm_mode_copy(adjusted_mode, &inno_vga->native_mode);
	}
	return true;
}

static void inno_vga_auto_setup_mode_enable(struct vga_device_t *inno_vga,
		struct drm_display_mode *adjusted_mode)
{
	struct drm_display_mode *mode = NULL;
	struct drm_display_mode *autoset_mode = NULL;

	if (!inno_vga || !adjusted_mode)
		return;

	fh2m_inno_mutex_lock(inno_vga->auto_setup_mutex);

	inno_vga->auto_setup_enable = true;

	if (VGA_MON_EN(vga_setup_every_mode, VGA_SETUP_EVERY_MODE_DISABLE)) {
		list_for_each_entry(mode, &inno_vga->auto_setup_list, head) {
			if (mode && drm_mode_equal(mode, adjusted_mode)) {
				inno_vga->auto_setup_enable = false;
				break;
			} else {
				inno_vga->auto_setup_enable = true;
			}
		}
	}

	fh2m_innodpu_info(inno_vga->dev, DPU_UT_VGA,
			"AutoSetupEnable:%d", inno_vga->auto_setup_enable);

	if (inno_vga->auto_setup_enable) {
		autoset_mode = drm_mode_duplicate(inno_vga->connector.dev, adjusted_mode);
		if (autoset_mode)
			list_add_tail(&autoset_mode->head, &inno_vga->auto_setup_list);
	}

	fh2m_inno_mutex_unlock(inno_vga->auto_setup_mutex);
}

static void inno_vga_auto_setup_mode_clear(struct vga_device_t *inno_vga)
{
	struct drm_display_mode *mode = NULL;
	struct drm_display_mode *mode1 = NULL;

	if (!inno_vga)
		return;

	fh2m_innodpu_info(inno_vga->dev, DPU_UT_VGA, "AutoSetupModeClear");

	fh2m_inno_mutex_lock(inno_vga->auto_setup_mutex);

	list_for_each_entry_safe(mode, mode1, &inno_vga->auto_setup_list, head) {
		list_del(&mode->head);
		drm_mode_destroy(inno_vga->drm_dev, mode);
	}

	inno_vga->auto_setup_enable = false;

	fh2m_inno_mutex_unlock(inno_vga->auto_setup_mutex);
}

static int inno_vga_set_vcp_feature(struct vga_device_t *vga, int vcp_code, int value)
{
	struct i2c_msg msg;
	char buf[7] = {0x51, 0x84, 0x03,};
	int i = 0;

	if (!vga->ddc)
		return -EFAULT;

	fh2m_innodpu_info(vga->dev, DPU_UT_VGA,
			"set vcp-%d value-%d\n", vcp_code, value);

	buf[3] = vcp_code;
	buf[4] = (value >> 8) & 0xff;
	buf[5] = value & 0xff;
	buf[6] = DDC_CI_ADDR * 2;

	for (i = 0; i < (INNO_ARRAY_SIZE(buf) - 1); i++)
		buf[6] ^= buf[i];

	msg.addr = DDC_CI_ADDR;
	msg.flags = 0;
	msg.len = INNO_ARRAY_SIZE(buf);
	msg.buf = buf;
	if (vga->ddc) {
		if (i2c_transfer(vga->ddc, &msg, 1) != 1)
			return -EFAULT;
	}

	return 0;
}

int inno_vga_auto_setup_ctrl(struct vga_device_t *vga, bool enable)
{
	fh2m_innodpu_info(vga->dev, DPU_UT_VGA,
			"auto_setup_ctrl:%s\n", enable ? "on" : "off");

	return inno_vga_set_vcp_feature(vga, 0xa2, enable ? 0x2 : 0x1);
}

int inno_vga_auto_setup(struct vga_device_t *vga, bool active)
{
	fh2m_innodpu_info(vga->dev, DPU_UT_VGA,
			"auto_setup:%s\n", active ? "active" : "not active");

	return inno_vga_set_vcp_feature(vga, 0x1e, active ? 0x1 : 0x0);
}

static void inno_vga_hpd_poll_work(struct work_struct *work)
{
	struct vga_device_t *inno_vga = container_of(to_delayed_work(work),
			     struct vga_device_t, hpd_poll_work);
	struct drm_connector *connector = &inno_vga->connector;
	struct drm_device *dev = inno_vga->drm_dev;
	enum drm_connector_status old_status;
	bool changed = false;

	if (!inno_vga->hpd_poll_en)
		return;

	mutex_lock(&dev->mode_config.mutex);
	old_status = connector->status;

#if (DRM_VERSION >= KERNEL_VERSION(4, 13, 0))
	connector->status = drm_helper_probe_detect(connector, NULL, false);
#else
	connector->status = connector->funcs->detect(connector, false);
#endif
	if (old_status != connector->status) {
		changed = true;
		fh2m_innodpu_info(inno_vga->dev, DPU_UT_VGA, "[CONNECTOR:%d:%s] status updated from %s to %s\n",
				connector->base.id,
				connector->name,
				drm_get_connector_status_name(old_status),
				drm_get_connector_status_name(connector->status));
	}
	mutex_unlock(&dev->mode_config.mutex);

	if (changed) {
		drm_kms_helper_hotplug_event(dev);
	}

	schedule_delayed_work(&inno_vga->hpd_poll_work, msecs_to_jiffies(1000));
}

static void inno_vga_auto_setup_execute(struct work_struct *work)
{
	struct delayed_work *auto_setup_work = to_delayed_work(work);
	struct vga_device_t *inno_vga = to_inno_vga(auto_setup_work);

	if (!inno_vga)
		return;

	if (inno_vga->auto_setup_enable) {
		fh2m_inno_mutex_lock(inno_vga->adapt_lock);
		atomic_set(&inno_vga->adapt_cnt, 1);

		/* show white edge here */
		raw_notifier_call_chain(&vga_notifier, true, &inno_vga->current_mode);
		inno_vga_auto_setup_ctrl(inno_vga, true);
		fh2m_inno_usleep_range(50000, 60000);
		inno_vga_auto_setup(inno_vga, true);

		/* delay 5 seconds to wait vga auto adapt finished */
		inno_wait_event_interruptible_timeout(inno_vga->adapt_wait,
			atomic_read(&inno_vga->adapt_cnt) == 0, msecs_to_jiffies(5000));

		/* close white edge here */
		raw_notifier_call_chain(&vga_notifier, false, &inno_vga->current_mode);
		fh2m_inno_mutex_unlock(inno_vga->adapt_lock);
	}
}

static void inno_vga_encoder_mode_set(struct drm_encoder *encoder,
		struct drm_display_mode *mode, struct drm_display_mode *adjusted_mode)
{
	struct vga_device_t *inno_vga = to_inno_vga(encoder);
	int dpu_id = 0;

	if (!encoder || !adjusted_mode)
		return;

	dpu_id = innodpu_get_dpuid_bycrtc(encoder->crtc);

	if (inno_vga->chip.encoder_modeset) {
		fh2m_innodpu_info(inno_vga->dev, DPU_UT_VGA,"%s modeset: "DRM_MODE_FMT "\n",
			inno_vga->name, DRM_MODE_ARG(adjusted_mode));
		inno_vga->chip.encoder_modeset(&inno_vga->chip, dpu_id, inno_vga->chip.test_mode, adjusted_mode);
		drm_mode_copy(&inno_vga->current_mode,  adjusted_mode);
	}

	inno_vga_auto_setup_mode_enable(inno_vga, adjusted_mode);
}

static void inno_vga_encoder_mode_disable(struct drm_encoder *encoder)
{
	struct vga_device_t *inno_vga = to_inno_vga(encoder);

	if (!encoder)
		return;

	if (inno_vga->chip.encoder_disable)
		inno_vga->chip.encoder_disable(&inno_vga->chip);

	atomic_set(&inno_vga->adapt_cnt, 0);
	fh2m_inno_wake_up_interruptible(inno_vga->adapt_wait);
	cancel_delayed_work_sync(&inno_vga->auto_setup_work);
}

static void inno_vga_encoder_mode_enable(struct drm_encoder *encoder)
{
	struct vga_device_t *inno_vga = to_inno_vga(encoder);

	if (!encoder)
		return;

	if (inno_vga->chip.encoder_enable)
		inno_vga->chip.encoder_enable(&inno_vga->chip, encoder->crtc);

	if (VGA_MON_EN(vga_setup, VGA_SETUP_EN))
		queue_delayed_work(inno_vga->auto_setup_wq, &inno_vga->auto_setup_work, msecs_to_jiffies(3000));
}

static int inno_vga_encoder_destroy(struct drm_encoder *encoder)
{
	BUG_ON(encoder);

	/*
	 * Called  drm_encoder_cleanup
	 */
	if (encoder->funcs && encoder->funcs->destroy)
		encoder->funcs->destroy(encoder);

	return 0;
}

static const struct drm_encoder_funcs s_inno_vga_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

static const struct drm_encoder_helper_funcs s_inno_vga_encoder_helper_funcs = {
	.mode_fixup = inno_vga_encoder_mode_fixup,
#if ((DRM_VERSION >= KERNEL_VERSION(4, 13, 0)))
	.mode_valid = inno_vga_encoder_mode_valid,
#endif
	.mode_set = inno_vga_encoder_mode_set,
	.disable = inno_vga_encoder_mode_disable,
	.enable = inno_vga_encoder_mode_enable,
};

static int inno_vga_encoder_create(struct drm_device *drm_dev,
		struct drm_encoder *encoder, unsigned int possible_crtc)
{
	int retcode = 0;

	drm_encoder_helper_add(encoder, &s_inno_vga_encoder_helper_funcs);
	retcode = drm_encoder_init(drm_dev, encoder,
		&s_inno_vga_encoder_funcs, DRM_MODE_ENCODER_DAC, NULL);

	encoder->possible_crtcs = possible_crtc;

	return retcode;
}

static void inno_vga_connector_destroy_func(struct drm_connector *connector)
{
	if (!connector)
		return;

	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
}

static s32 inno_vga_get_edid_block(void *data, u8 * buf, u32 block, size_t len)
{
	struct vga_chip_t *chip = (struct vga_chip_t*)data;

	if (len > INNOVGA_EDID_BUF_LEN / 2) {
		return -EINVAL;
	}

	if (block % 2 == 0) {
		fh2m_inno_memcpy(buf, chip->edid_buf, len);
	} else {
		fh2m_inno_memcpy(buf, chip->edid_buf + INNOVGA_EDID_BUF_LEN / 2, len);
	}

	return 0;
}

static int inno_vga_get_modes_strpush(struct drm_connector *connector)
{
	int ret = 0;
	struct vga_device_t *vga_dev = to_inno_vga(connector);
	struct vga_chip_t *chip = &vga_dev->chip;

	ret = fh2m_hal_vga_edid_data(fh2m_inno_dev_get_parent(chip->dev),
			vga_dev->vga_id, chip->edid_buf);
	if (ret) {
		fh2m_innodpu_err(chip->dev, "strpush edid error, ret:%d\n", ret);
		return -EFAULT;
	}

	chip->modes = innodpu_str_push_edid(chip->edid_buf, connector);
	fh2m_innodpu_info(vga_dev->dev, DPU_UT_VGA, "get edid from strpush ok\n");
	return ret;
}

static int inno_vga_get_edid_user(struct drm_connector *connector)
{
	int ret = 0;
	int i = 0;
	struct vga_device_t *vga_dev = to_inno_vga(connector);
	struct vga_chip_t *chip = &vga_dev->chip;

	ret = fh2m_hal_vga_edid_data(fh2m_inno_dev_get_parent(chip->dev),
			vga_dev->vga_id, chip->edid_buf);
	if (ret) {
		fh2m_innodpu_err(chip->dev, "get edid from user error, ret:%d\n", ret);
		return -EFAULT;
	}

	for (i = 0; i <= chip->edid_buf[0x7e] && i <= 1; i++) {
		if (!drm_edid_block_valid(chip->edid_buf + i * INNOVGA_EDID_BUF_LEN / 2, i, false, NULL)) {
			ret = -EFAULT;
			break;
		}
	}
	fh2m_innodpu_info(vga_dev->dev, DPU_UT_VGA, "get edid from user ok\n");
	return ret;
}

static int inno_vga_read_edid_block(struct vga_device_t *vga, u8 *buf, unsigned int block, size_t len)
{
	int ret = 0;
	int retry = 3;

	do {
		if (vga->ddc) {
			fh2m_innodpu_info(vga->dev, DPU_UT_VGA, "Read edid via ddc[block:%d]\n", block);
			ret = fh2m_inno_drm_do_probe_ddc_edid(vga->ddc, buf, block, len);
		} else if (vga->chip.connector_get_edid) {
			fh2m_innodpu_info(vga->dev, DPU_UT_VGA, "Read edid via edid hardware\n");
			ret = vga->chip.connector_get_edid(&vga->chip);
			fh2m_inno_memcpy(buf, vga->chip.edid_buf, EDID_LENGTH);
		} else {
			fh2m_innodpu_err(vga->dev, "Doesn't support edid reading\n");
			return -EFAULT;
		}

		if (ret == 0 && drm_edid_block_valid(buf, block, false, NULL))
			return 0;

	} while (retry--);

	return -EFAULT;
}

static int inno_vga_read_edid(struct vga_device_t *vga)
{
	u8 buf[EDID_LENGTH] = {0,};
	int block_cnt = 0, block = 0;

#define VGA_EDID_BLOCK (INNOVGA_EDID_BUF_LEN / EDID_LENGTH)

	fh2m_inno_memset(vga->chip.edid_buf, 0, sizeof(vga->chip.edid_buf));

	do {
		if (inno_vga_read_edid_block(vga, buf, block, EDID_LENGTH))
			return -EFAULT;

		if (!vga->ddc)
			return 0;

		if (block == 0) {
			/* edid extension */
			block_cnt = buf[0x7e] + 1;

			/* In the standard EDID for VGA, usually 128 bytes, but in some cases
			 * there may be extended EDID data structures that may exceed 128 bytes.
			 */
			if (block_cnt > VGA_EDID_BLOCK) {
				fh2m_innodpu_err(vga->dev, "vga edid max block is %d", VGA_EDID_BLOCK);
				block_cnt = VGA_EDID_BLOCK;
			}
		}

		fh2m_inno_memcpy(vga->chip.edid_buf + block * EDID_LENGTH, buf, EDID_LENGTH);
	} while (++block < block_cnt);

	return 0;
}

static int inno_vga_get_edid(struct drm_connector *connector)
{
	int ret = 0;
	struct vga_device_t *vga_dev =  to_inno_vga(connector);
	struct vga_chip_t *chip = &vga_dev->chip;

	fh2m_inno_memset(chip->edid_buf, 0, sizeof(chip->edid_buf));

	switch (chip->hal_edid_mode) {
	case EDID_AUTO_READ:
		ret = inno_vga_read_edid(vga_dev);
	break;
	case EDID_STR_PUSH:
		ret = inno_vga_get_modes_strpush(connector);
	break;
	case EDID_USER_DEFINE:
		ret = inno_vga_get_edid_user(connector);
	break;
	default:
		fh2m_innodpu_err(vga_dev->dev, "get_edid failed, invalid hal_edid_mode,"
				"read edid from monitor\n");
		ret = -EFAULT;
	}
	fh2m_innodpu_info(vga_dev->dev, DPU_UT_VGA,  "hal_edid_mode:%d ret = %d\n", chip->hal_edid_mode, ret);

	if (ret == 0 && chip->hal_edid_mode != EDID_STR_PUSH) {
		vga_dev->chip.vga_edid = drm_do_get_edid(connector, inno_vga_get_edid_block, &vga_dev->chip);
		chip->max_pclk_rx = innodpu_conn_get_monitor_max_clk((u8 *)chip->vga_edid);
		if (chip->max_pclk_rx <= 0) {
			chip->max_pclk_rx = 175000;
			fh2m_innodpu_info(vga_dev->dev, DPU_UT_VGA, "get vga max clock err, set it to 175MHz\n");
		}
	}
	if (ret || (chip->hal_edid_mode != EDID_STR_PUSH && vga_dev->chip.vga_edid == NULL)) {
		if (vga_dev->chip.vga_edid) {
			fh2m_inno_kfree(vga_dev->chip.vga_edid);
			vga_dev->chip.vga_edid = NULL;
		}

		if (connector->force != DRM_FORCE_ON) {
			if (connector->status == inno_connector_status_connected)
				fh2m_innodpu_err(vga_dev->dev, "[%s] Edid Invalid, may be poor contact, please re-plug\n", connector->name);
		}

		drm_connector_update_edid_property(connector, NULL);
		vga_dev->chip.modes = innodpu_add_modes_without_edid(connector, NULL);

	} else if (chip->hal_edid_mode != EDID_STR_PUSH) {
		drm_connector_update_edid_property(connector, vga_dev->chip.vga_edid);
		vga_dev->chip.modes = drm_add_edid_modes(connector, vga_dev->chip.vga_edid);
	}

	return ret;
}

static bool inno_vga_native_mode_filter(struct drm_connector *connector,
	struct drm_display_mode *mode, bool scaling_filter)
{
	struct vga_device_t *inno_vga
		= container_of(connector, struct vga_device_t, connector);
#if ((DRM_VERSION >= KERNEL_VERSION(4, 13, 0)))
	struct drm_encoder *encoder = &inno_vga->encoder;
#endif
	int ret = 0;

	if (mode && inno_vga->chip.max_pclk_rx > 0 &&
		mode->clock > inno_vga->chip.max_pclk_rx)
		return true;

	if (connector->helper_private && connector->helper_private->mode_valid) {
		ret = connector->helper_private->mode_valid(&inno_vga->connector, mode);
		if (ret != MODE_OK) {
			return true;
		}
	}

#if ((DRM_VERSION >= KERNEL_VERSION(4, 13, 0)))
	if (encoder->helper_private && encoder->helper_private->mode_valid) {
		ret = encoder->helper_private->mode_valid(&inno_vga->encoder, mode);
		if (ret != MODE_OK) {
			return true;
		}
	}
#endif

	return false;
}

static void inno_vga_add_common_mode(struct drm_connector *connector)
{
	struct vga_device_t *inno_vga = to_inno_vga(connector);
	struct drm_display_mode *native_mode = NULL;
	struct drm_display_mode *mode1 = NULL;
	struct drm_display_mode *mode = NULL;
	char mode_name[DRM_DISPLAY_MODE_LEN];
	int hdisplay, vdisplay;

	/* get native mode */
	native_mode = innodpu_get_native_mode(connector, inno_vga_native_mode_filter);

	/*
	 * In order to solve the problem of offset-screen under small VGA resolution,
	 * we try to convert the mode with hdisplay less than 1600 to
	 * virtual resolution.
	 * */
	if (native_mode) {
		list_for_each_entry_safe(mode, mode1, &connector->probed_modes, head) {
			if (!mode || innodpu_modes_equal(mode, native_mode)) {
				continue;
			}
			if (mode->hdisplay <= 1600 &&
					mode->hdisplay <= native_mode->hdisplay &&
					mode->vdisplay <= native_mode->vdisplay) {
				hdisplay = mode->hdisplay;
				vdisplay = mode->vdisplay;
				drm_mode_copy(mode, native_mode);
				snprintf(mode_name, sizeof(mode_name), "%dx%d", hdisplay, vdisplay);
				strncpy(mode->name, mode_name, DRM_DISPLAY_MODE_LEN);
				mode->hdisplay = hdisplay;
				mode->vdisplay = vdisplay;
				mode->type &= ~DRM_MODE_TYPE_PREFERRED;
			}
		}

		/* add common mode */
		fh2m_innodpu_info(inno_vga->dev, DPU_UT_VGA, "%s native mode: "DRM_MODE_FMT ", status:%d\n",
			inno_vga->name, DRM_MODE_ARG(native_mode), native_mode->status);
		fh2m_inno_memcpy(&inno_vga->native_mode, native_mode, sizeof(inno_vga->native_mode));
		drm_mode_set_crtcinfo(&inno_vga->native_mode, CRTC_INTERLACE_HALVE_V);
		inno_vga->native_mode.status = MODE_OK;
		inno_vga->chip.modes += innodpu_connector_add_common_modes(connector, &inno_vga->native_mode, true);
	} else {
		fh2m_inno_memset(&inno_vga->native_mode, 0, sizeof(inno_vga->native_mode));
	}
}

/**
 * inno_vga_connector_helper_get_modes - Get vga supports mode;
 *
 * Returns:
 * Number of supported modes
 */
static int inno_vga_connector_helper_get_modes(
		struct drm_connector *connector)
{
	struct vga_device_t *inno_vga = to_inno_vga(connector);
	struct edid *edid = inno_vga->chip.vga_edid;

	if (drm_edid_is_valid(edid)) {
		drm_connector_update_edid_property(connector, edid);
		inno_vga->chip.modes = drm_add_edid_modes(connector, edid);
	} else {
		inno_vga_get_edid(connector);
	}

	drm_mode_sort(&connector->probed_modes);

	// fixup 1366x768 and more modes for /dev/fb0
	innodpu_modes_fixup_preferred_nonaligned_modes(connector);

	inno_vga_add_common_mode(connector);

	return inno_vga->chip.modes;
}

static int innovga_display_ddc_probe(struct i2c_adapter *ddc)
{
	u8 out = 0x0;
	u8 buf[8];
	int ret;
	struct i2c_msg msgs[] = {
		{
			.addr = DDC_ADDR,
			.flags = 0,
			.len = 1,
			.buf = &out,
		},
		{
			.addr = DDC_ADDR,
			.flags = I2C_M_RD,
			.len = 8,
			.buf = buf,
		}
	};

	if (!ddc) {
		return -EFAULT;
	}

	ret = i2c_transfer(ddc, msgs, 2);

	if (ret != 2)
		/* Couldn't find an accessible DDC on this connector */
		return ret;
	/* Probe also for valid EDID header
	 * EDID header starts with:
	 * 0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00.
	 * Only the first 6 bytes must be valid as
	 * drm_edid_block_valid() can fix the last 2 bytes */
	if (drm_edid_header_is_valid(buf) < 6) {
		/* Couldn't find an accessible EDID on this
		 * connector */
		return -EFAULT;
	}

	return 0;
}

static void inno_vga_hpdout_clear(struct vga_device_t *inno_vga)
{
	if (inno_vga->chip.vga_edid) {
		kfree(inno_vga->chip.vga_edid);
		inno_vga->chip.vga_edid = NULL;
	}

	inno_vga->chip.max_pclk_rx = 0;

	if (!list_empty(&inno_vga->auto_setup_list))
		inno_vga_auto_setup_mode_clear(inno_vga);
}

/**
 * inno_vga_connector_detect_ctx - check vga hpd status
 *
 * Returns:
 * drm_connector_status. (connector_status_connected or connector_status_disconnected)
 */
static int inno_vga_connector_detect_ctx(struct drm_connector *connector,
		struct drm_modeset_acquire_ctx *ctx, bool force)
{
	int ret = 0;
	int con = connector_status_disconnected;
	struct vga_device_t *inno_vga = to_inno_vga(connector);

	if (VGA_MON_EN(con_detect_mode, CON_DETECT_DDC) && inno_vga->ddc) {
		fh2m_innodpu_info(inno_vga->dev, DPU_UT_VGA, "connector detect ddc\n");
		ret = innovga_display_ddc_probe(inno_vga->ddc);
		if (ret == 0)
			con = connector_status_connected;
	}

	/* If the return value is -ETIMEDOUT, the pmbus bus is abnormal,
	 * SCL is high, SDA is low, pmbus can not be rwepairer by itself,
	 * you can solve the pmbus abnormality by pulling up the SDA through the
	 * DDC channel that comes with the vga
	 * */
	if (con != connector_status_connected &&
		(VGA_MON_EN(con_detect_mode, CON_DETECT_CABLE) || ret == -ETIMEDOUT)) {
		if (inno_vga->chip.connector_detect) {
			fh2m_innodpu_info(inno_vga->dev, DPU_UT_VGA, "connector detect cable\n");
			con = inno_vga->chip.connector_detect(&inno_vga->chip);
		}
	}

	if (con != connector_status_connected)
		inno_vga_hpdout_clear(inno_vga);

	fh2m_innodpu_info(inno_vga->dev, DPU_UT_VGA, "connector_status: %s\n",
			(con == connector_status_connected) ? "connect" : "disconnect");

	return con;
}

static enum drm_connector_status inno_vga_detect(struct drm_connector *connector, bool force)
{
    return inno_vga_connector_detect_ctx(connector, NULL, force);
}

static enum drm_mode_status inno_vga_connector_helper_mode_valid(
		struct drm_connector *connector, struct drm_display_mode *mode)
{
	int status = MODE_OK;
	struct vga_device_t *inno_vga = to_inno_vga(connector);

	if (mode->flags & DRM_MODE_FLAG_DBLSCAN) {
		fh2m_innodpu_info(inno_vga->dev, DPU_UT_VGA, "[Connector:%d] Doublescan mode rejected.\n", connector->base.id);
		return MODE_NO_DBLESCAN;
	}

	if (mode->flags & DRM_MODE_FLAG_INTERLACE) {
		fh2m_innodpu_info(inno_vga->dev, DPU_UT_VGA, "[Connector:%d] Interlace mode rejected.\n", connector->base.id);
		return MODE_NO_INTERLACE;
	}

	if (drm_mode_vrefresh(mode) > INNOVGA_DEFAULT_FRAME_RATE) {
		fh2m_innodpu_info(inno_vga->dev, DPU_UT_VGA,
			"[Connector:%d] %s:vfresh must be smaller than 60 !\n",
			connector->base.id, mode->name);
		return MODE_BAD_HVALUE;
	}

	if (inno_vga->chip.connector_mode_valid) {
		status = inno_vga->chip.connector_mode_valid(&inno_vga->chip, mode);
		if (status != MODE_OK)
			return status;
	}

	return MODE_OK;
}

static s32 _i2c_smbus_xfer(struct i2c_adapter *adapter, u16 addr, unsigned short flags,
char read_write, u8 command, int protocol, union i2c_smbus_data *data)
{
	struct inno_pmbus_dev *pmbus = i2c_get_adapdata(adapter);
	s32 ret = 0;

	if (!adapter)
		return -EINVAL;

	pmbus = i2c_get_adapdata(adapter);
	if (!pmbus) {
		inno_error("%s %d pmbus get failed\n", __func__, __LINE__);
		return -EINVAL;
	}

	mutex_lock(&pmbus->chip_rs_lock);
	pmbus->chip_need_rs = 1;
	ret = i2c_smbus_xfer(adapter, addr, flags, read_write, command, protocol, data);
	mutex_unlock(&pmbus->chip_rs_lock);

	return ret;
}

static int innovga_i2c_xfer(struct i2c_adapter *ddc, struct i2c_msg *msgs,
			int num)
{
	int ret = 0;
	u8 request = 0;
	u8 command = 0;
	u8 word_offset = 0;
	unsigned int i, j;
	unsigned transfer_size;
	union i2c_smbus_data data;
	unsigned int slave_addr = 0;
	struct vga_device_t *inno_vga = ddc->algo_data;

	transfer_size = I2C_SMBUS_BLOCK_MAX;

	for (i = 0; i < num; i++) {
		request = (msgs[i].flags & I2C_M_RD) ? I2C_SMBUS_READ : I2C_SMBUS_WRITE;
		slave_addr = msgs[i].addr;

		/* The VGA display interface usually only uses 0x50 0x37,
		 * for 0x30 segment pointers and other accesses are nack and
		 * more time consuming! */
		if (slave_addr != DDC_EDID_ADDR && slave_addr != DDC_CI_ADDR)
			continue;

		for (j = 0; j < msgs[i].len; j += request ? data.block[0] : (data.block[0] + 1)) {
			memset(data.block, 0, sizeof(data.block));
			if (request == I2C_SMBUS_WRITE) {
				data.block[0] = min(transfer_size, msgs[i].len - j - 1);
				command = word_offset = msgs[i].buf[j];
				if (data.block[0] >= 1)
					memcpy(&data.block[1], msgs[i].buf + j + 1, data.block[0]);
			} else {
				data.block[0] = min(transfer_size, msgs[i].len - j);
				command = j + word_offset;
			}

			/*The delayed operation during DDC/CI communication is handled by the application layer,
			 * and the driver does not need to delay*/
			/*if (slave_addr == DDCCI_ADDR && request == I2C_SMBUS_READ)
				fh2m_inno_usleep_range(50000, 60000);*/

			fh2m_innodpu_info(inno_vga->dev, DPU_UT_VGA,
					"%s slave_addr:%#x command:%#.2x data.block[0]:%d msgs.len:%d\n",
					request ? "read" : "write", msgs[i].addr,
					command, data.block[0], msgs[i].len);

			ret = _i2c_smbus_xfer(inno_vga->pmbus_adapter, slave_addr,
						0x0, request, command,
						I2C_SMBUS_I2C_BLOCK_DATA, &data);
			if (ret != 0)
				return ret;

			if (request == I2C_SMBUS_READ)
				memcpy(msgs[i].buf + j, &data.block[1], data.block[0]);

			if (data.block[0] && (fh2m_hal_get_s_dpu_debug() & DPU_UT_VGA)) {
				fh2m_inno_print_hex_dump(KERN_NOTICE, "\t", 0, 16, 1,
					(request == I2C_SMBUS_READ) ? &msgs[i].buf[j] : &data.block[1], data.block[0], false);
			}
		}
	}

	return num;
}

static u32 innovga_i2c_functionality(struct i2c_adapter *ddc)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL |
	       I2C_FUNC_SMBUS_READ_BLOCK_DATA |
	       I2C_FUNC_SMBUS_BLOCK_PROC_CALL |
	       I2C_FUNC_10BIT_ADDR;
}

static const struct i2c_algorithm innovga_i2c_algo = {
	.functionality = innovga_i2c_functionality,
	.master_xfer = innovga_i2c_xfer,
};

static int inno_vga_connector_late_register(struct drm_connector *connector)
{
	int ret = 0;
	struct device *adap_dev = NULL;
	struct vga_device_t *inno_vga = to_inno_vga(connector);
	struct i2c_adapter *ddc;

	if (!VGA_MON_EN(ddc_access_mode, DDC_ACCESS_NONE)) {
		ddc = devm_kzalloc(inno_vga->dev, sizeof(struct i2c_adapter), fh2m_hal_get_inno_gfp_kernel());
		if (!ddc) {
			return -ENOMEM;
		}

#if ((DRM_VERSION < KERNEL_VERSION(6, 8, 0)))
		ddc->class = I2C_CLASS_DDC;
#endif
		ddc->owner = THIS_MODULE;
		ddc->dev.parent = inno_vga->dev;
		ddc->algo_data = inno_vga;
		ddc->algo = &innovga_i2c_algo;
		fh2m_inno_strlcpy(ddc->name, dev_name(inno_vga->dev), sizeof(ddc->name));

		ret = i2c_add_adapter(ddc);
		if (ret) {
			fh2m_innodpu_err(inno_vga->dev, "vga-ddc register failed\n");
			goto end;
		}

		inno_vga->ddc = ddc;
		adap_dev = &inno_vga->ddc->dev;
		ret = sysfs_create_link(&connector->kdev->kobj,
					&adap_dev->kobj,
					adap_dev->kobj.name);
		if (ret) {
			fh2m_innodpu_err(inno_vga->dev, "%s sysfs link create failed\n", inno_vga->ddc->name);
		}
	}

#if defined(CONFIG_DEBUG_FS)
	ret = inno_vga_custom_debugfs_create(connector);
	if (ret) {
		fh2m_innodpu_err(inno_vga->dev, "%s custom debugfs create failed-%d\n",
			inno_vga->name, ret);
	}
#endif

end:
	/* start hpd detect */
	if (connector->force != DRM_FORCE_ON) {
		inno_vga->hpd_poll_en = true;
		schedule_delayed_work(&inno_vga->hpd_poll_work, HZ);
	}

	return ret;
}

static void inno_vga_connector_early_unregister(struct drm_connector *connector)
{
	struct vga_device_t *inno_vga = to_inno_vga(connector);

	inno_vga->hpd_poll_en = false;
	cancel_delayed_work_sync(&inno_vga->hpd_poll_work);

	if (inno_vga->ddc) {
		sysfs_remove_link(&connector->kdev->kobj,
				inno_vga->ddc->dev.kobj.name);
		i2c_del_adapter(inno_vga->ddc);
	}
}

static struct drm_connector_helper_funcs s_inno_vga_connector_helper_funcs = {
	.get_modes = inno_vga_connector_helper_get_modes,
	.mode_valid = inno_vga_connector_helper_mode_valid,
#if ((DRM_VERSION >= KERNEL_VERSION(4, 12, 0)))
	.detect_ctx = inno_vga_connector_detect_ctx,
#endif
};

static const struct drm_connector_funcs s_inno_vga_connector_funcs = {
#if ((DRM_VERSION <= KERNEL_VERSION(4, 13, 0)))
	.dpms = drm_atomic_helper_connector_dpms,
#else
	.dpms = drm_helper_connector_dpms,
#endif
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = inno_vga_connector_destroy_func,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
	.late_register = inno_vga_connector_late_register,
	.early_unregister = inno_vga_connector_early_unregister,
	.detect = inno_vga_detect,
};

static int inno_vga_connector_create(struct drm_device *drm_dev,
				struct drm_connector *connector)
{
	int retcode = 0;

	BUG_ON(!drm_dev);
	BUG_ON(!connector);

	drm_connector_helper_add(connector, &s_inno_vga_connector_helper_funcs);

	retcode = drm_connector_init(drm_dev, connector, &s_inno_vga_connector_funcs,
		DRM_MODE_CONNECTOR_VGA);

	connector->polled = DRM_CONNECTOR_POLL_HPD;  // TBD : DRM_CONNECTOR_POLL_CONNECT ?
	connector->dpms = DRM_MODE_DPMS_OFF;
	connector->interlace_allowed = false;
	connector->doublescan_allowed = false;
	connector->display_info.subpixel_order = SubPixelHorizontalRGB;

	return retcode;
}

static int inno_vga_connector_destroy(struct drm_connector *connector)
{
	struct drm_mode_object *conn_obj = &connector->base;

	while (kref_read(&conn_obj->refcount) > 0)
		drm_connector_put(connector); // will be called drm_connector_free
	return 0;
}


static int inno_vga_connector_attach_encoder(
			struct vga_device_t *inno_vga)
{
	int retcode = 0;

	retcode = inno_vga_encoder_create(inno_vga->drm_dev, &inno_vga->encoder,
		inno_vga->chip.possible_crtc);
	if (retcode) {
		fh2m_innodpu_err(inno_vga->dev, "%s encoder create failed-%d\n",
				  inno_vga->name, retcode);
		return retcode;
	}

	retcode = inno_vga_connector_create(inno_vga->drm_dev, &inno_vga->connector);
	if (retcode) {
		fh2m_innodpu_err(inno_vga->dev, "%s connector create failed-%d\n",
			inno_vga->name, retcode);
		goto err_connector_create;
	}

	// only set connector->possible_encoder,
	retcode = drm_connector_attach_encoder(&inno_vga->connector, &inno_vga->encoder);
	if (retcode) {
		fh2m_innodpu_err(inno_vga->dev, "%s connector attach encoder failed-%d\n",
			inno_vga->name, retcode);
		goto error_attach_failed;
	}

	fh2m_innodpu_info(inno_vga->dev, DPU_UT_VGA, "%s attach encoder-%d connector-%d\n",
		inno_vga->name, inno_vga->encoder.base.id, inno_vga->connector.base.id);

	return retcode;

error_attach_failed:
	inno_vga_connector_destroy(&inno_vga->connector);
err_connector_create:
	inno_vga_encoder_destroy(&inno_vga->encoder);

	return retcode;
}
#if 0
void inno_vga_hw_irq_enable(struct vga_device_t *inno_vga, unsigned int flags)
{
	BUG_ON(!inno_vga);
	if (inno_vga->chip.irq_enable)
		return inno_vga->chip.irq_enable(&inno_vga->chip, flags);
}

void inno_vga_hw_irq_disable(struct vga_device_t *inno_vga, unsigned int flags)
{
	BUG_ON(!inno_vga);
	if (inno_vga->chip.irq_disable)
		return inno_vga->chip.irq_disable(&inno_vga->chip, flags);
}
#endif
static int inno_vga_hw_init(struct vga_device_t *inno_vga)
{
	int retcode = 0;

	if (inno_vga->chip.hw_init)
		return inno_vga->chip.hw_init(&inno_vga->chip);

	return retcode;
}

static void inno_vga_hw_fini(struct vga_device_t *inno_vga)
{
	if (inno_vga->chip.hw_fini)
		inno_vga->chip.hw_fini(&inno_vga->chip);
}

static int inno_vga_chip_init(struct vga_device_t *inno_vga)
{
	chip_type_e plat;
	int retcode = 0;

	inno_vga->chip.hal_edid_mode = fh2m_hal_vga_edid_mode(inno_vga->dev->parent, inno_vga->vga_id);
	if (inno_vga->chip.hal_edid_mode < EDID_AUTO_READ) {
		inno_vga->chip.hal_edid_mode = EDID_AUTO_READ;
	}

	vga_info(inno_vga->dev, "hal_vga-%d edid_mode %d\n",
			inno_vga->vga_id, inno_vga->chip.hal_edid_mode);

	plat = fh2m_hal_get_chiptype(inno_vga->dev->parent);
	switch(plat) {
	case CHIP_G1P_SOC:
		vga_info(inno_vga->dev, "%s start init g1p soc.\n", inno_vga->name);
		retcode = g1p_soc_vga_chip_init(&inno_vga->chip,
				inno_vga->dev, inno_vga->vga_id, inno_vga->drm_dev);
		break;
	case CHIP_G0M_SOC:
		vga_info(inno_vga->dev, "%s start init g0m soc.\n", inno_vga->name);
		inno_vga->pmbus_adapter = fh2m_hal_get_pmbus_adapter(inno_vga->dev->parent, 0);
		retcode = g0m_soc_vga_chip_init(&inno_vga->chip,
				inno_vga->dev, inno_vga->vga_id, inno_vga->drm_dev);
		VGA_MON_CLEAR(ddc_access_mode);
		VGA_MON_CLEAR(con_detect_mode);
		VGA_MON_CLEAR(vga_setup);
		if (s_vga_ddcci) {
			VGA_MON_SET(ddc_access_mode, DDC_ACCESS_PMBUS);
			VGA_MON_SET(con_detect_mode, CON_DETECT_DDC);
		}
		if (s_vga_auto_adapt)
			VGA_MON_SET(vga_setup, VGA_SETUP_EN);

		break;
	default:
		fh2m_innodpu_err(inno_vga->dev, "%s does not currently support %d platform.\n",
			inno_vga->name, plat);
		retcode = -EINVAL;
		break;
	}

	return retcode;
}

static void inno_vga_chip_fini(struct vga_device_t *inno_vga)
{
	chip_type_e plat;

	plat = fh2m_hal_get_chiptype(inno_vga->dev->parent);
	switch(plat) {
	case CHIP_G1_SOC:
		g1p_soc_vga_chip_fini(&inno_vga->chip);
		break;
	case CHIP_G0M_SOC:
		g0m_soc_vga_chip_fini(&inno_vga->chip);
		break;
	default:
		fh2m_innodpu_err(inno_vga->dev, "%s does not currently support %d platform.\n",
			inno_vga->name, plat);
		break;
	}

	return;
}


/**
 * inno_vga_bind - innosilicon vga-driver initialization function
 * @dev: vga_device_t_info allocated when inno_vga_device_t_register
 * parent(dev->parent) is &pci_dev.dev
 * @master: component master
 * @data: point of struct drm_device
 *
 * This function initializes the innosilicon vga device
 * 1. alloc and init vga handle
 * 2. vga hardware init
 * 3. vga ddcci supports
 * 4. creation and bonding of HDMI connectors and encoders
 * 5. setup vga irq
 *
 * Returns:
 * 0 if it is OK, errno otherwise.
 */
static int inno_vga_bind(struct device *dev,
					  struct device *master, void *data)
{
	int retcode = 0;
	struct drm_device * drm_dev = data;
	struct vga_device_t *inno_vga = NULL;
	plat_data_t *pdata =  dev_get_platdata(dev);

	vga_info(dev, "start\n");
	BUG_ON(!dev);
	BUG_ON(!data);

	if (!innodpu_detect_is_valid_output(dev, CONNECTOR_M_VGA)) {
		vga_info(dev,  "possible_crtc = 0, do not bind vga\n");
		return retcode;
	}

	inno_vga = devm_kmalloc(dev, sizeof(*inno_vga), fh2m_hal_get_inno_gfp_kernel());
	if (!inno_vga) {
		fh2m_innodpu_err(dev, "Alloc vga-%d handle failed. Short of memory.\n",
			pdata->dev_idx);
		return -ENOMEM;
	}
	memset(inno_vga, 0, sizeof(*inno_vga));
	inno_vga->vga_id = pdata->dev_idx;
	atomic_set(&inno_vga->adapt_cnt, 0);
	inno_vga->dev = get_device(dev);
	inno_vga->drm_dev = drm_dev;
	inno_vga->name = kasprintf(fh2m_hal_get_inno_gfp_kernel(), "inno-vga-%d",
		inno_vga->vga_id);
	if (!inno_vga->name) {
		fh2m_innodpu_err(dev, "Alloc vga-%d name failed. Short of memory.\n",
			pdata->dev_idx);
		retcode = -ENOMEM;
		goto err_out_name;
	}
	dev_set_drvdata(dev, inno_vga);
	vga_info(dev,  "%s start bind.\n", inno_vga->name);

	retcode = inno_vga_chip_init(inno_vga);
	if (retcode) {
		fh2m_innodpu_err(dev, "%s chip Init failed-%d.\n", inno_vga->name, retcode);
		goto err_chip_init;
	}

	retcode = inno_vga_hw_init(inno_vga);
	if (retcode) {
		fh2m_innodpu_err(dev, "%s hw Init failed-%d.\n", inno_vga->name, retcode);
		goto err_hw_init;
	}

	retcode = inno_vga_connector_attach_encoder(inno_vga);
	if (retcode) {
		fh2m_innodpu_err(dev, "%s connector and encoder Init failed-%d.\n",
			inno_vga->name, retcode);
		goto err_attach_encoder;
	}

	inno_vga->auto_setup_wq = fh2m_inno_create_singlethread_wq("vga auto_calib_wq");
	if (!inno_vga->auto_setup_wq) {
		retcode = -1;
		fh2m_innodpu_err(dev, "%s auto_setup_sw failed-%d.\n", inno_vga->name, retcode);
		goto err_attach_encoder;
	}
	INIT_DELAYED_WORK(&inno_vga->auto_setup_work, inno_vga_auto_setup_execute);

	/* hpd period 1s */
	INIT_DELAYED_WORK(&inno_vga->hpd_poll_work, inno_vga_hpd_poll_work);

	INIT_LIST_HEAD(&inno_vga->auto_setup_list);
	inno_vga->auto_setup_mutex = fh2m_inno_mutex_alloc();
	inno_vga->adapt_lock = fh2m_inno_mutex_alloc();
	if (!inno_vga->auto_setup_mutex || !inno_vga->adapt_lock) {
		retcode = -1;
		fh2m_innodpu_err(dev, "mutex alloc failed.\n");
		goto err_attach_encoder;
	}

	inno_vga->adapt_wait = fh2m_inno_waitqueue_head_alloc();
	if (!inno_vga->adapt_wait) {
		retcode = -1;
		fh2m_innodpu_err(dev, "vga adapt wait alloc failed\n");
		goto err_attach_encoder;
	}
	fh2m_inno_init_waitqueue_head(inno_vga->adapt_wait);

	vga_info(dev, "end\n");
	return retcode;

err_attach_encoder:
	if (inno_vga->adapt_wait) {
		fh2m_inno_waitqueue_head_free(inno_vga->adapt_wait);
		inno_vga->adapt_wait = NULL;
	}

	if (inno_vga->auto_setup_mutex)
		fh2m_inno_mutex_free(inno_vga->auto_setup_mutex);
	if (inno_vga->adapt_lock)
		fh2m_inno_mutex_free(inno_vga->adapt_lock);
	inno_vga_hw_fini(inno_vga);
err_hw_init:
	inno_vga_chip_fini(inno_vga);
err_chip_init:
	if (inno_vga->name)
		kfree(inno_vga->name);
err_out_name:
	put_device(dev);
	devm_kfree(dev, inno_vga);

	return retcode;
}

/**
* inno_vga_unbind - innosilicon vga-driver initialization function
* @dev: vga_device_t_info allocated when inno_vga_device_t_register
* parent(dev->parent) is &pci_dev.dev
* @master: component master
* @data: point of struct drm_device
*
* This function deinitializes the innosilicon vga device
*
* Once drm_dev_register is called, the connector and encoder
* resources are released by the drm_mode_config_cleanup function!!!
* So we don't need to worry about the release of the connector and encoder here.
*/
static void inno_vga_unbind(struct device *dev,
						  struct device *master, void *data)

{
	struct vga_device_t *inno_vga = NULL;

	vga_info(dev, "start\n");
	BUG_ON(!dev);
	BUG_ON(!data);

	if (!innodpu_detect_is_valid_output(dev, CONNECTOR_M_VGA)) {
		vga_info(dev,  "possible_crtc = 0, do not unbind vga\n");
		return;
	}

	inno_vga = dev_get_drvdata(dev);
	if (!inno_vga) {
		fh2m_innodpu_err(dev, "vga handle is NULL\n");
		return;
	}

	cancel_delayed_work_sync(&inno_vga->hpd_poll_work);
	atomic_set(&inno_vga->adapt_cnt, 0);
	fh2m_inno_wake_up_interruptible(inno_vga->adapt_wait);
	cancel_delayed_work_sync(&inno_vga->auto_setup_work);
	fh2m_inno_destroy_workqueue(inno_vga->auto_setup_wq);

	inno_vga_hw_fini(inno_vga);
	inno_vga_chip_fini(inno_vga);
	kfree(inno_vga->name);

	if (!list_empty(&inno_vga->auto_setup_list))
		inno_vga_auto_setup_mode_clear(inno_vga);
	if (inno_vga->auto_setup_mutex)
		fh2m_inno_mutex_free(inno_vga->auto_setup_mutex);
	if (inno_vga->adapt_lock)
		fh2m_inno_mutex_free(inno_vga->adapt_lock);

	if (inno_vga->adapt_wait) {
		fh2m_inno_waitqueue_head_free(inno_vga->adapt_wait);
		inno_vga->adapt_wait = NULL;
	}

	put_device(dev);
	devm_kfree(dev, inno_vga);
	vga_info(dev, "end\n");
}


static const struct component_ops s_inno_vga_ops = {
	.bind = inno_vga_bind,
	.unbind = inno_vga_unbind,
};

static int inno_vga_probe(struct platform_device *pdev)
{
	BUG_ON(!pdev);
	return component_add(&pdev->dev, &s_inno_vga_ops);
}

static int inno_vga_remove(struct platform_device *pdev)
{
	BUG_ON(!pdev);
	component_del(&pdev->dev, &s_inno_vga_ops);
	return 0;
}

static struct platform_device_id s_inno_vga_device_id_table[] = {
	{.name = INNO_VGA_DEVICE_NAME, .driver_data = 0},
	{},
};
MODULE_DEVICE_TABLE(platform, s_inno_vga_device_id_table);

#ifdef CONFIG_PM_SLEEP
static int inno_vga_suspend(struct device *dev)
{
	struct vga_device_t *inno_vga = dev_get_drvdata(dev);

	vga_info(dev, "suspend\n");

	if (!innodpu_detect_is_valid_output(dev, CONNECTOR_M_VGA)) {
		vga_info(dev,  "possible_crtc = 0, do not suspend vga\n");
		return 0;
	}

	if (inno_vga) {
		inno_vga->hpd_poll_en = false;
		cancel_delayed_work_sync(&inno_vga->hpd_poll_work);
		atomic_set(&inno_vga->adapt_cnt, 0);
		fh2m_inno_wake_up_interruptible(inno_vga->adapt_wait);
		cancel_delayed_work_sync(&inno_vga->auto_setup_work);
		inno_vga_hw_fini(inno_vga);
		if (!list_empty(&inno_vga->auto_setup_list))
			inno_vga_auto_setup_mode_clear(inno_vga);
	}

	return 0;
}

static int inno_vga_resume(struct device *dev)
{
	struct vga_device_t *inno_vga = NULL;

	vga_info(dev, "resume\n");

	if (!innodpu_detect_is_valid_output(dev, CONNECTOR_M_VGA)) {
		vga_info(dev,  "possible_crtc = 0, do not resume vga\n");
		return 0;
	}

	inno_vga = dev_get_drvdata(dev);
	if (inno_vga) {
		inno_vga_hw_init(inno_vga);
	}

	/* start hpd detect */
	if (inno_vga->connector.force != DRM_FORCE_ON) {
		inno_vga->hpd_poll_en = true;
		schedule_delayed_work(&inno_vga->hpd_poll_work, HZ);
	}

	return 0;
}
#endif

static void inno_vga_shutdown(struct platform_device *pdev)
{
	struct vga_device_t *inno_vga = NULL;

	if (!innodpu_detect_is_valid_output(&pdev->dev, CONNECTOR_M_VGA)) {
		vga_info(&pdev->dev, "possible_crtc = 0, do not shutdown vga\n");
		return;
	}

	inno_vga = fh2m_inno_platform_get_drvdata(pdev);
	if (inno_vga) {
		cancel_delayed_work_sync(&inno_vga->hpd_poll_work);
		inno_vga_hw_fini(inno_vga);
	}
}

static const struct dev_pm_ops inno_vga_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(inno_vga_suspend, inno_vga_resume)
};

struct platform_driver g_innogpu_vga_driver = {
	.probe = inno_vga_probe,
	.remove = inno_vga_remove,
	.shutdown = inno_vga_shutdown,
	.driver = {
		.name = INNO_VGA_DEVICE_NAME,
		.pm = &inno_vga_pm_ops,
	},
	.id_table = s_inno_vga_device_id_table,
};
