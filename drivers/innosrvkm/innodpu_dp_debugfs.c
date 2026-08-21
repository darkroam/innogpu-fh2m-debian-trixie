/*************************************************************************/ /*!
@File			innodpu_dp_debugfs.c
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
#include "innodpu_connector.h"
#include "innodpu_dp.h"
#include "innodpu_dp_debugfs.h"
#include "innodpu_panel_pwr.h"

static int dp_connector_status_show(struct seq_file *m, void *data)
{
	int i = 0;
	int connect_status = 0;
	char buf[MONITOR_NAME_LEN] = {0};
	struct dp_sprint_file dp_sprint;
	u8 link_status[DP_LINK_STATUS_SIZE];
	const struct drm_display_mode *mode = NULL;
	const struct drm_display_mode *rmode = NULL;
	const struct drm_display_mode *current_mode = NULL;
	const struct drm_display_mode *adjusted_mode = NULL;
	const struct drm_display_mode *native_mode = NULL;
	const struct drm_display_mode *preferred_mode = NULL;
	struct drm_connector *connector = m->private;
	struct dp_device_t *dp_dev = to_dp_device(connector);

	dp_sprint.buf = (void *)__get_free_page(fh2m_hal_get_inno_gfp_kernel());
	dp_sprint.size = PAGE_SIZE;
	dp_sprint.count = 0;

	mutex_lock(&dp_dev->drm_dev->mode_config.mutex);

#if ((DRM_VERSION >= KERNEL_VERSION(4, 12, 0)))
	if (connector->helper_private->detect_ctx)
		connect_status = connector->helper_private->detect_ctx(connector, NULL, 0);
#else
	if (connector->funcs->detect)
		connect_status = connector->funcs->detect(connector, 0);
#endif

	fh2m_inno_seq_printf(m, "%s status info\n", dp_dev->name);
	fh2m_inno_seq_printf(m, "hpd_status:\t %s\n",
			(connect_status == connector_status_connected) ?
			"connected": "disconnected");

	if (connect_status != connector_status_connected) {
		fh2m_inno_seq_printf(m, "\t %s disconnect, end dump status info\n", dp_dev->name);
		goto unlock;
	}

	if (connector->state && connector->state->crtc) {
		current_mode = &connector->state->crtc->mode;
		if (connector->state->crtc->state) {
			adjusted_mode = &connector->state->crtc->state->adjusted_mode;
		}
	}

	if (dp_dev->chip.max_pclk_rx > 0)
		fh2m_inno_seq_printf(m, "max clock from edid: %dkHz\n", dp_dev->chip.max_pclk_rx);

	fh2m_inno_seq_printf(m, "mode: \n");
	if (is_native_mode_valid(&dp_dev->native_mode)) {
		native_mode = &dp_dev->native_mode;
	}
	list_for_each_entry(mode, &connector->modes, head) {
		fh2m_inno_seq_printf(m, "    "DRM_MODE_FMT, DRM_MODE_ARG(mode));

		if (current_mode && innodpu_modes_equal(mode, current_mode)) {
			fh2m_inno_seq_printf(m, "  (current)");
		}

		if (mode->clock > dp_dev->chip.max_pclk_rx) {
			if (dp_dev->chip.max_pclk_rx > 0)
				fh2m_inno_seq_printf(m, "  (greater than max clock!)");
			else
				fh2m_inno_seq_printf(m, "  (no edid mode!)");
		}

		if (native_mode) {
			if (innodpu_modes_equal(mode, native_mode)) {
				fh2m_inno_seq_printf(m, "  (native)");
			}
			if (is_virtual_mode(native_mode, mode)) {
				fh2m_inno_seq_printf(m, "  (virtual)");
			}
		}

		if (mode->type & DRM_MODE_TYPE_PREFERRED) {
			preferred_mode = mode;
			fh2m_inno_seq_printf(m, "  (preferred)");
		}

		if (dp_dev->chip.replace_timing) {
			rmode = innodpu_modes_match_replace_table(mode, NULL, inno_dp_is_skip_replace);
			if (!rmode)
				goto next;

			if(is_special_mode(rmode)) {
				fh2m_inno_seq_printf(m, "  (warnning: unsupport!!!)");
			} else {
				if (!drm_mode_equal(mode, rmode))
					fh2m_inno_seq_printf(m, "\n    =>  "DRM_MODE_FMT, DRM_MODE_ARG(rmode));
			}
		}

		next:
		fh2m_inno_seq_printf(m, "\n");

	}

	fh2m_inno_seq_printf(m, "logo status: %s\n",
					innodpu_modes_equal(adjusted_mode, preferred_mode) ? "show" : "maybe show");
	fh2m_inno_seq_printf(m, "possible_crtc:%#x\n", dp_dev->chip.possible_crtc);
	fh2m_inno_seq_printf(m, "replace_timing: %d\n", dp_dev->chip.replace_timing);
	fh2m_inno_seq_printf(m, "dpms:         %s\n", (connector->dpms == DRM_MODE_DPMS_ON) ? "on" :
			((connector->dpms == DRM_MODE_DPMS_STANDBY) ? "Standby" :
			((connector->dpms == DRM_MODE_DPMS_SUSPEND) ? "Suspend" : "Off")));
	fh2m_inno_seq_printf(m, "dpcd_rev    : %#x\n", dp_dev->dpcd[DP_DPCD_REV]);
	fh2m_inno_seq_printf(m, "enhance_mode: %s\n", dp_dev->chip.enhance_mode ? "support" : "not support");

	fh2m_inno_seq_printf(m, "source status:\n");
	if (dp_dev->chip.dp_source_cfg_show && dp_sprint.buf) {
		dp_dev->chip.dp_source_cfg_show(&dp_dev->chip, &dp_sprint);
		fh2m_inno_seq_printf(m, "%s", dp_sprint.buf);
	}

	fh2m_inno_seq_printf(m, "Training:\n");
	if (!sink_lane_status_get(dp_dev, link_status))
		fh2m_inno_seq_printf(m, "\tsink status: %*ph\n", sizeof(link_status), link_status);

	fh2m_inno_seq_printf(m, "max width:%d max height:%d\n", dp_dev->chip.max_width, dp_dev->chip.max_height);

	fh2m_inno_seq_printf(m, "edid src    : %s\n",
				(dp_dev->chip.hal_edid_mode == EDID_AUTO_READ) ? "EDID_AUTO_READ" :
				( (dp_dev->chip.hal_edid_mode == EDID_STR_PUSH) ? "EDID_STR_PUSH" : "EDID_USER_DEFINE"));

	if (dp_get_edid(&dp_dev->chip)) {
		drm_edid_get_monitor_name(dp_get_edid(&dp_dev->chip), buf, INNO_ARRAY_SIZE(buf));
		fh2m_inno_seq_printf(m, "monitor: %s\n", buf);
	} else {
		fh2m_inno_seq_printf(m, "monitor: Unknown\n");
	}

	fh2m_inno_seq_printf(m, "edid value  :\n");
	if (connector->edid_blob_ptr && connector->edid_blob_ptr->length > 0) {
		for (i = 0; i < connector->edid_blob_ptr->length; i++) {
			if (((i % 16) == 0))
				fh2m_inno_seq_printf(m, "\n\t");
			fh2m_inno_seq_printf(m, "%.2x", dp_dev->chip.edid_buf[i]);
		}
		fh2m_inno_seq_printf(m, "\n");
	}

	if (connector->state && connector->state->crtc) {
		fh2m_inno_seq_printf(m, "video_sel: %s match %s\n", dp_dev->name, connector->state->crtc->name);
	} else {
		fh2m_inno_seq_printf(m, "video_sel: %s match %s\n", dp_dev->name, "unknow");
	}

unlock:
	mutex_unlock(&dp_dev->drm_dev->mode_config.mutex);

	if (dp_sprint.buf)
		free_page((unsigned long)dp_sprint.buf);

	return 0;
}

static int dp_edid_show(struct seq_file *m, void *data)
{
	struct drm_connector *connector = m->private;
	struct dp_device_t *dp_dev = container_of(connector, struct dp_device_t, connector);

	if (dp_dev->chip.connector_detect(&dp_dev->chip) != connector_status_connected) {
		fh2m_inno_seq_printf(m, "\n%s\n", "DP connector is not plug in.");
		return 0;
	}

	fh2m_inno_seq_printf(m, "\n%s\n", "innosilicon dp edid info:");
	if (fh2m_inno_drm_edid_block_checksum(dp_dev->chip.edid_buf)) {
		fh2m_inno_seq_printf(m, "\nblock0 checksum failed.\n");
		return 0;
	} else {
		ParseEdidBlock0(m, dp_dev->chip.edid_buf);
	}

	if (fh2m_inno_drm_edid_block_checksum(dp_dev->chip.edid_buf + EDID_LENGTH)) {
		fh2m_inno_seq_printf(m, "\nblock1 checksum failed.\n");
		return 0;
	} else {
		ParseEdidBlock1(m, dp_dev->chip.edid_buf + EDID_LENGTH);
	}

	return 0;
}

static int dp_connector_status_open(struct inode *inode, struct file *file)
{
	struct drm_connector *dev = inode->i_private;

	return single_open(file, dp_connector_status_show, dev);
}

static int dp_connector_bist_show(struct seq_file *m, void *data)
{
	struct drm_connector *connector = m->private;
	struct dp_device_t *dp_dev = container_of(connector, struct dp_device_t, connector);

	fh2m_inno_seq_printf(m, "==cur status==\n");
	fh2m_inno_seq_printf(m, "test mode: %s\n", dp_dev->chip.bist_enable ? "true" : "false");

	fh2m_inno_seq_printf(m, "\n==help==\n");
	fh2m_inno_seq_printf(m, "write 1, will set test_mode to true.\n");
	fh2m_inno_seq_printf(m, "write 0, will set test_mode to false.\n");

	return 0;
}

static int dp_connector_bist_open(struct inode *inode, struct file *file)
{
	struct drm_connector *dev = inode->i_private;

	return single_open(file, dp_connector_bist_show, dev);
}

static ssize_t dp_connector_bist_write(struct file *file, const char __user *ubuf,
			       size_t len, loff_t *offp)
{
	char buf[16] = {0};
	struct seq_file *m = file->private_data;
	struct drm_connector *connector = m->private;
	struct dp_device_t *dp_dev = container_of(connector, struct dp_device_t, connector);

	if (sizeof(buf) - 1 < len) {
		return -EINVAL;
	}

	if (fh2m_inno_copy_from_user(buf, ubuf, len)) {
		return -EFAULT;
	}

	buf[len] = '\0';
	buf[0] = fh2m_inno_simple_strtoul(buf, NULL, 10);
	if (buf[0] == 1) {
		dp_info(dp_dev->dev, "bist_enable\n");
		dp_dev->chip.bist_enable = true;
	} else {
		dp_info(dp_dev->dev, "bist_disable\n");
		dp_dev->chip.bist_enable = false;
	}

	if (dp_dev->chip.dp_bist_test)
		if (connector->state && connector->state->crtc)
			dp_dev->chip.dp_bist_test(&dp_dev->chip, (inno_drm_display_mode *)&connector->state->crtc->mode);

	return len;
}

static int dp_connector_edid_info_open(struct inode * inode, struct file * file)
{
	struct drm_connector *dev = inode->i_private;

	return single_open(file, dp_edid_show, dev);
}

static int dp_connector_hw_func_show(struct seq_file *seq, void *data)
{
	struct drm_connector *connector = seq->private;
	int connect_status = 0;
	uint32_t hw_self_test_num = 5;
	struct dp_device_t *dp_dev = container_of(connector, struct dp_device_t, connector);
	uint32_t success_num = 0;
	uint32_t fail_num = 0;
	int ret = 0;

	fh2m_inno_seq_printf(seq, "==%s self test result:==\n", dp_dev->name);
	fh2m_inno_seq_printf(seq, "\tType of self-test   :%s\n",
			(dp_dev->chip.hw_self_test_mode == INNO_HW_SELF_TEST_EDID) ? "EDID reading" : "unsupport");
	fh2m_inno_seq_printf(seq, "\tNumber of self-tests:%d\n", hw_self_test_num);

#if ((DRM_VERSION >= KERNEL_VERSION(4, 12, 0)))
	if (connector->helper_private->detect_ctx)
		connect_status = connector->helper_private->detect_ctx(connector, NULL, 0);
#else
	if (connector->funcs->detect)
		connect_status = connector->funcs->detect(connector, 0);
#endif

	if (connect_status != inno_connector_status_connected) {
		fh2m_inno_seq_printf(seq, "\t%s disconnect, end hardwre function self-test\n", dp_dev->name);
		return 0;
	}

	if (dp_dev->chip.hal_edid_mode != EDID_AUTO_READ) {
		fh2m_inno_seq_printf(seq, "\tedid read from custom, parsing failed\n");
		return 0;
	}

	while (hw_self_test_num--) {
		switch (dp_dev->chip.hw_self_test_mode) {
		case INNO_HW_SELF_TEST_EDID:
			ret = inno_dp_get_edid_monitor(connector);
		break;
		default:
			ret = -EINVAL;
			fh2m_innodpu_err(dp_dev->chip.dev, "%s:unsupport hw self-test mode:%d\n", dp_dev->chip.hw_self_test_mode);
		break;
		}
		if (!ret) {
			success_num += 1;
		} else {
			fail_num += 1;
		}
	}

	fh2m_inno_seq_printf(seq, "\tNumber of successful self-tests:%d\n", success_num);
	fh2m_inno_seq_printf(seq, "\tNumber of failed self-tests    :%d\n", fail_num);

	return 0;
}

static ssize_t dp_connector_hw_func_write(struct file *file, const char __user *ubuf,
			       size_t len, loff_t *offp)
{
	char kbuf[16] = {0};
	struct seq_file *m = file->private_data;
	struct drm_connector *connector = m->private;
	struct dp_device_t *dp_dev = container_of(connector, struct dp_device_t, connector);

	if (sizeof(kbuf) - 1 < len) {
		return -EINVAL;
	}

	if (fh2m_inno_copy_from_user(kbuf, ubuf, len)) {
		return -EFAULT;
	}

	kbuf[len] = '\0';
	kbuf[0] = fh2m_inno_simple_strtoul(kbuf, NULL, 10);

	dp_dev->chip.hw_self_test_mode = kbuf[0];
	dp_info(dp_dev->chip.dev, "hardware self-test mode:%d\n", dp_dev->chip.hw_self_test_mode);

	return len;
}

static int dp_connector_hw_func_open(struct inode *inode, struct file *file)
{
	struct drm_connector *dev = inode->i_private;

	return single_open(file, dp_connector_hw_func_show, dev);
}

static int dp_aux_rw_show(struct seq_file *m, void *data)
{
	struct drm_connector *connector = m->private;
	struct dp_device_t *dp_dev = to_dp_device(connector);
	int i = 0, ret = 0, per_size = 0;

	fh2m_inno_seq_printf(m, "==aux rw==\n");
	fh2m_inno_seq_printf(m, "== help ==\n\t(format: dpcd_addr rw(0:w 1:r) len (write data))\n");
	fh2m_inno_seq_printf(m, "DPCD: %#.4x RW:%s LEN:%d byte\n", dp_dev->aux_msg.address,
			dp_dev->aux_msg.request ? "read": "write", dp_dev->aux_msg.size);

	if (IS_ERR(dp_dev->aux_msg.buffer))
		return -EFAULT;

	if (dp_dev->aux_msg.size > INNODP_EDID_BUF_LEN || dp_dev->aux_msg.size <= 0) {
			fh2m_inno_seq_printf(m, "Invalid argument\n");
			return 0;
	}

	for ( i = 0; i < dp_dev->aux_msg.size; i += per_size) {
		per_size = (dp_dev->aux_msg.size - i > DP_AUX_MAX_PAYLOAD_BYTES) ? DP_AUX_MAX_PAYLOAD_BYTES : (dp_dev->aux_msg.size - i);
		if (dp_dev->aux_msg.request)
			ret += drm_dp_dpcd_read(&dp_dev->aux, dp_dev->aux_msg.address + i,
					(u8 *)dp_dev->aux_msg.buffer + i, per_size);
		else
			ret += drm_dp_dpcd_write(&dp_dev->aux, dp_dev->aux_msg.address + i,
					(u8 *)dp_dev->aux_msg.buffer + i, per_size);
	}

	if (ret == dp_dev->aux_msg.size) {
		fh2m_inno_seq_printf(m, "%s (address-value)\n\t", dp_dev->aux_msg.request ? "Read data:" : "Write data");
		for (i = 0; i < dp_dev->aux_msg.size;) {
			fh2m_inno_seq_printf(m, "(%#.2x-%#.2x) ",
					dp_dev->aux_msg.address + i, ((u8 *)dp_dev->aux_msg.buffer)[i]);
			if (++i % 8 == 0)
				fh2m_inno_seq_printf(m, "\n\t");
		}
		fh2m_inno_seq_printf(m, "\n");
	} else {
		fh2m_inno_seq_printf(m, "aux trnasfer failed!\n");
	}

	return 0;
}

static ssize_t dp_connector_aux_rw_write(struct file *file, const char __user *ubuf,
			       size_t len, loff_t *offp)
{
	char kbuf[128] = {0};
	char *sub_ptr = NULL, *sv_ptr = NULL;
	int i = 0;
	struct seq_file *m = file->private_data;
	struct drm_connector *connector = m->private;
	struct dp_device_t *dp_dev = container_of(connector, struct dp_device_t, connector);

	if (sizeof(kbuf) - 1 < len)
		return -EINVAL;

	if (fh2m_inno_copy_from_user(kbuf, ubuf, len))
		return -EFAULT;

	kbuf[len] = '\0';
	sv_ptr = kbuf;

	sub_ptr = strsep(&sv_ptr, " ");
	if (sub_ptr)
		dp_dev->aux_msg.address = fh2m_inno_simple_strtoul(sub_ptr, NULL, 0);
	else
		goto out;

	sub_ptr = strsep(&sv_ptr, " ");
	if (sub_ptr)
		dp_dev->aux_msg.request = fh2m_inno_simple_strtoul(sub_ptr, NULL, 0);
	else
		goto out;

	sub_ptr = strsep(&sv_ptr, " ");
	if (sub_ptr)
		dp_dev->aux_msg.size = fh2m_inno_simple_strtoul(sub_ptr, NULL, 0);
	else
		goto out;

	if (!dp_dev->aux_msg.request) {
		for (i = 0 ; i < INNODP_EDID_BUF_LEN; i++) {
			sub_ptr = strsep(&sv_ptr, " ");
			if (sub_ptr)
				((u8 *)dp_dev->aux_msg.buffer)[i] = fh2m_inno_simple_strtoul(sub_ptr, NULL, 0);
			else
				break;
		}
	}

out:
	fh2m_inno_seq_printf(m, "dpcd_addr:%#.4x\n", dp_dev->aux_msg.address);
	fh2m_inno_seq_printf(m, "request  :%s\n", dp_dev->aux_msg.request ? "read" : "write");
	fh2m_inno_seq_printf(m, "size     :%d\n", dp_dev->aux_msg.size);
	if (!dp_dev->aux_msg.request) {
		for (i = 0 ; i < dp_dev->aux_msg.size; i++) {
			fh2m_inno_seq_printf(m, "%#.2x \n", ((u8 *)dp_dev->aux_msg.buffer)[i]);
			if (++i % 16 == 0)
				fh2m_inno_seq_printf(m, "\n");
		}
		fh2m_inno_seq_printf(m, "\n");
	}

	return len;
}

static int dp_connector_aux_rw_open(struct inode *inode, struct file *file)
{
	struct drm_connector *dev = inode->i_private;

	return single_open(file, dp_aux_rw_show, dev);
}

INNODP_DEBUGFS_ENTRY_RO(status);
INNODP_DEBUGFS_ENTRY_RO(edid_info);
INNODP_DEBUGFS_ENTRY_RW(bist);
INNODP_DEBUGFS_ENTRY_RW(hw_func);
INNODP_DEBUGFS_ENTRY_RW(aux_rw);

int inno_dp_late_register(struct drm_connector *connector)
{
	struct dp_device_t * dp_dev = container_of(connector, struct dp_device_t, connector);
#if defined(CONFIG_DEBUG_FS)
	struct dentry *ent0 = NULL;
	struct drm_minor *minor = connector->dev->primary;
	u8 i = 0;
#endif
	struct device *adap_dev = NULL;
	int ret = 0;
	u8 backlight_mode = 0;

#if defined(CONFIG_DEBUG_FS)
	const char *debug_name[] = {"inno_status",
								"edid_info",
								"bist_info",
								"hw_self_test",
								"dp_aux_transfer"};
	const struct file_operations *debug_fops[] = {
					&innodpu_dp_status_fops,
					&innodpu_dp_edid_info_fops,
					&innodpu_dp_bist_fops,
					&innodpu_dp_hw_func_fops,
					&innodpu_dp_aux_rw_fops};

	if (connector->debugfs_entry) {
		for (i = 0; i < INNO_ARRAY_SIZE(debug_name); i++) {
			ent0 = debugfs_create_file(debug_name[i], S_IRUGO | S_IWUSR,
					connector->debugfs_entry, connector, debug_fops[i]);
			if (!ent0) {
				fh2m_innodpu_err(dp_dev->dev, "create debug node /sys/kernel/debug/dri/%d/%s/%s Error.\n",
						minor->index, connector->name, debug_name[i]);
			}
		}
	}
#endif

	ret = drm_dp_aux_register(&dp_dev->aux);
	if (ret) {
		fh2m_innodpu_err(dp_dev->dev, "drm_dp_aux_register failed\n");
		goto end;
	}

	backlight_mode = innodpu_get_connector_backlight_mode(dp_dev->dev, dp_dev->chip.reg_module);
	if (backlight_mode >= CONNECTOR_BACKLIGHT_PWM0 &&
		backlight_mode <= CONNECTOR_BACKLIGHT_AUX_HDR) {
		dp_dev->panel = panel_pwr_create(connector->dev, dp_dev->dev,
										  dp_dev->chip.reg_module,
										  &dp_dev->aux,
										  connector);
		connector->force = DRM_FORCE_ON;
	} else {
		dp_dev->panel = NULL;
	}
	dp_dev->chip.panel = dp_dev->panel;

	adap_dev = &dp_dev->aux.ddc.dev;
	ret = sysfs_create_link(&connector->kdev->kobj,
				&adap_dev->kobj,
				adap_dev->kobj.name);
	if (ret) {
		fh2m_innodpu_err(dp_dev->dev, "%s sysfs_create_link failed\n", adap_dev->kobj.name);
	}

	{   // special handling for U7
		struct hw_board_info board = {"U7", "*"};
		int status = 0;
		if (connector->connector_type == DRM_MODE_CONNECTOR_eDP
			&& innodpu_is_odm_pcb_match(dp_dev->drm_dev->dev, &board)) {
			/* pwr on edp */
			inno_panel_prepare(dp_dev->panel);
			/* detect edp hpd */
			if (dp_dev->chip.connector_detect) {
				status = dp_dev->chip.connector_detect(&dp_dev->chip);
				if (status == inno_connector_status_connected) {
					dp_dev->connector.force = DRM_FORCE_ON;
				} else {
					dp_dev->connector.force = DRM_FORCE_OFF;
				}
				fh2m_inno_dp_drm_helper_hpd_irq_event(dp_dev->drm_dev);
			}
		}
	}

	inno_dp_check_sink_connection(dp_dev);

	{ // special handling for R1
		struct hw_board_info board = {"R1", "*"};
		dp_dev->is_R1 = innodpu_is_odm_pcb_match(dp_dev->drm_dev->dev, &board);
	}
	if (dp_dev->is_R1) {
		inno_dp_sink_power_ctrl(dp_dev, false);
	}

end:
	return ret;
}

void inno_dp_early_unregister(struct drm_connector *connector)
{
	struct dp_device_t * inno_dp = container_of(connector, struct dp_device_t, connector);

	sysfs_remove_link(&connector->kdev->kobj,
			inno_dp->aux.ddc.dev.kobj.name);

	if (inno_dp->panel) {
		panel_pwr_destory(inno_dp->panel);
	}

	drm_dp_aux_unregister(&inno_dp->aux);
}
