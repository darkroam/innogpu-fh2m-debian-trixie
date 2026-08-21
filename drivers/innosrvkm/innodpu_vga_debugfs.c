#include "innodpu_connector.h"
#include "innodpu_vga.h"
#include "innodpu_vga_debugfs.h"

#if defined(CONFIG_DEBUG_FS)
static int vga_edid_info(struct seq_file *seq, void *data)
{
	struct drm_connector *connector = seq->private;
	struct vga_device_t *vga_dev = to_inno_vga(connector);

	if (vga_dev->chip.connector_detect(&vga_dev->chip) != connector_status_connected) {
		fh2m_inno_seq_printf(seq, "\n%s\n", "VGA connector is not plug in.");
		return 0;
	}

	fh2m_inno_seq_printf(seq, "\n%s\n", "innosilicon vga edid info:");
	if (vga_dev->chip.vga_edid_parse)
		vga_dev->chip.vga_edid_parse(seq, &vga_dev->chip);

	return 0;
}

static int vga_status_info(struct seq_file *seq, void *data)
{
	struct drm_connector *connector = seq->private;
	struct vga_device_t *vga_dev = to_inno_vga(connector);
	int connector_status = 0;
	struct drm_display_mode *display_mode = NULL;
	struct drm_display_mode *native_mode = NULL;
	const struct drm_display_mode *adjusted_mode = NULL;
	const struct drm_display_mode *preferred_mode = NULL;
	struct drm_display_mode *mode = NULL;
	char buf[MONITOR_NAME_LEN] = {0};
	unsigned char *edid = NULL;
	size_t size = 0;
	int i = 0;

	mutex_lock(&vga_dev->drm_dev->mode_config.mutex);

#if ((DRM_VERSION >= KERNEL_VERSION(4, 12, 0)))
	if (connector->helper_private->detect_ctx)
		connector_status = connector->helper_private->detect_ctx(connector, NULL, 0);
#else
	if (connector->funcs->detect)
		connector_status = connector->funcs->detect(connector, 0);
#endif

	fh2m_inno_seq_printf(seq, "%s status info\n", vga_dev->name);
	fh2m_inno_seq_printf(seq, "hpd_status:\t%s\n",
		(connector_status == connector_status_connected) ? "connected" : "disconnected");

	if (connector_status != connector_status_connected) {
		fh2m_inno_seq_printf(seq, "\t %s disconnect, end dump status info\n", vga_dev->name);
		goto unlock;
	}

	if (connector->state && connector->state->crtc) {
		display_mode = &connector->state->crtc->mode;
		if (connector->state->crtc->state) {
			adjusted_mode = &connector->state->crtc->state->adjusted_mode;
		}
	}

	if (vga_dev->chip.max_pclk_rx > 0)
		fh2m_inno_seq_printf(seq, "max clock from edid: %dkHz\n", vga_dev->chip.max_pclk_rx);

	fh2m_inno_seq_printf(seq, "mode: \n");

	if (is_native_mode_valid(&vga_dev->native_mode)) {
		native_mode = &vga_dev->native_mode;
	}

	list_for_each_entry(mode, &connector->modes, head) {
		fh2m_inno_seq_printf(seq, "    "DRM_MODE_FMT, DRM_MODE_ARG(mode));

		if (display_mode && innodpu_modes_equal(mode, display_mode)) {
			fh2m_inno_seq_printf(seq, "  (current)");
		}

		if (mode->clock > vga_dev->chip.max_pclk_rx) {
			if (vga_dev->chip.max_pclk_rx)
				fh2m_inno_seq_printf(seq, "  (greater than max clock!)");
			else
				fh2m_inno_seq_printf(seq, "  (no edid mode!)");
		}

		if (native_mode) {
			if (innodpu_modes_equal(mode, native_mode)) {
				fh2m_inno_seq_printf(seq, "  (native)");
			}
			if (is_virtual_mode(native_mode, mode)) {
				fh2m_inno_seq_printf(seq, "  (virtual)");
			}
		}

		if (mode->type & DRM_MODE_TYPE_PREFERRED) {
			preferred_mode = mode;
			fh2m_inno_seq_printf(seq, "  (preferred)");
		}

		fh2m_inno_seq_printf(seq, "\n");
	}

	fh2m_inno_seq_printf(seq, "logo status: %s\n",
					innodpu_modes_equal(adjusted_mode, preferred_mode) ? "show" : "maybe show");

	fh2m_inno_seq_printf(seq, "possible_crtc: %#x\n", vga_dev->chip.possible_crtc);

	fh2m_inno_seq_printf(seq, "dpms:	%s\n", (connector->dpms == DRM_MODE_DPMS_ON) ? "on":
		((connector->dpms == DRM_MODE_DPMS_STANDBY) ? "Standby" :
		((connector->dpms == DRM_MODE_DPMS_SUSPEND) ? "Suspend" : "Off")));

	fh2m_inno_seq_printf(seq, "max width: %d, max height: %d\n",
		vga_dev->chip.max_width, vga_dev->chip.max_height);

	fh2m_inno_seq_printf(seq, "edid src: %s\n", (vga_dev->chip.hal_edid_mode == EDID_AUTO_READ) ? "EDID_AUTO_READ" :
		((vga_dev->chip.hal_edid_mode == EDID_STR_PUSH) ? "EDID_STR_PUSH" : "EDID_USER_DEFINE"));

	if ((vga_dev->connector.status == 1) && vga_dev->chip.hal_edid_mode != EDID_STR_PUSH) {
		drm_edid_get_monitor_name((struct edid *)vga_dev->chip.edid_buf, buf, INNO_ARRAY_SIZE(buf));
		fh2m_inno_seq_printf(seq, "monitor: %s\n", buf);
	} else {
		fh2m_inno_seq_printf(seq, "monitor: Unknown\n");
	}

	fh2m_inno_seq_printf(seq, "edid value:\n");
	if (connector->edid_blob_ptr) {
		edid = connector->edid_blob_ptr->data;
		size = connector->edid_blob_ptr->length;
	}
	if (!edid) {
		fh2m_inno_seq_printf(seq, "edid NULL!\n");
	} else {
		for (i = 0; i < size; i++) {
			if ((i % 16) == 0)
				fh2m_inno_seq_printf(seq, "\n\t");
			fh2m_inno_seq_printf(seq, "%.2x", vga_dev->chip.edid_buf[i]);
		}
		fh2m_inno_seq_printf(seq, "\n");
	}

	if (connector->state && connector->state->crtc) {
		fh2m_inno_seq_printf(seq, "video_sel: %s match %s\n", vga_dev->name, connector->state->crtc->name);
	} else {
		fh2m_inno_seq_printf(seq, "video_sel: %s match %s\n", vga_dev->name, "unkonwn");
	}

unlock:
	mutex_unlock(&vga_dev->drm_dev->mode_config.mutex);

	return 0;
}

static int vga_connector_edid_info_open(struct inode *inode, struct file *file)
{
	struct drm_connector *dev = inode->i_private;

	return single_open(file, vga_edid_info, dev);
}

static int vga_connector_status_open(struct inode *inode, struct file *file)
{
	struct drm_connector *dev = inode->i_private;

	return single_open(file, vga_status_info, dev);
}

static int vga_auto_calibration_show(struct seq_file *m, void *data)
{
	struct drm_connector *connector = m->private;
	struct vga_device_t *vga_dev = to_inno_vga(connector);
	u8 buf[3];

	fh2m_inno_seq_printf(m, "==auto calibration==\n");
	fh2m_inno_seq_printf(m, "== help ==\n\t(format: VsyncSignalDelaySelection HsyncSignalDelaySelection\n");

	if (IS_ERR(vga_dev))
		return -EFAULT;

	if (!vga_dev->chip.auto_calibration_get) {
		fh2m_inno_seq_printf(m, "unsupport platform\n");
		return -EINVAL;
	}

	vga_dev->chip.auto_calibration_get(&vga_dev->chip, buf, INNO_ARRAY_SIZE(buf));
	fh2m_inno_seq_printf(m, "\tCurrent config: %#.2x %#.2x\n", buf[0], buf[1]);

	return 0;
}

static ssize_t vga_auto_calibration_write(struct file *file, const char __user *ubuf,
			       size_t len, loff_t *offp)
{
	char kbuf[128] = {0};
	u8 cbuf[3] = {0};
	char *sub_ptr = NULL, *sv_ptr = NULL;
	struct seq_file *m = file->private_data;
	struct drm_connector *connector = m->private;
	struct vga_device_t *vga_dev = to_inno_vga(connector);

	if (sizeof(kbuf) - 1 < len)
		return -EINVAL;

	if (fh2m_inno_copy_from_user(kbuf, ubuf, len))
		return -EFAULT;

	kbuf[len] = '\0';
	sv_ptr = kbuf;

	sub_ptr = strsep(&sv_ptr, " ");
	if (sub_ptr)
		cbuf[0] = fh2m_inno_simple_strtoul(sub_ptr, NULL, 0);
	else
		goto out;

	sub_ptr = strsep(&sv_ptr, " ");
	if (sub_ptr)
		cbuf[1] = fh2m_inno_simple_strtoul(sub_ptr, NULL, 0);

out:
	fh2m_inno_seq_printf(m, "(config: VsyncSignalDelaySelection "
			"HsyncSignalDelaySelection:%#.2x %#.2x\n", cbuf[0], cbuf[1]);

	if (!vga_dev->chip.auto_calibration_set) {
		fh2m_inno_seq_printf(m, "unsupport platform\n");
	} else {
		vga_dev->chip.auto_calibration_set(&vga_dev->chip, cbuf, INNO_ARRAY_SIZE(cbuf));
	}

	inno_vga_auto_setup_ctrl(vga_dev, true);
	fh2m_inno_usleep_range(50000, 60000);
	inno_vga_auto_setup(vga_dev, true);

	return len;
}

static int vga_auto_calibration_open(struct inode *inode, struct file *file)
{
	struct drm_connector *dev = inode->i_private;

	return single_open(file, vga_auto_calibration_show, dev);
}

static const struct file_operations innodpu_vga_edid_info_fops = {
	.owner = THIS_MODULE,
	.open = vga_connector_edid_info_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations innodpu_vga_status_fops = {
	.owner = THIS_MODULE,
	.open = vga_connector_status_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations innodpu_vga_auto_calibration_fops = {
	.owner = THIS_MODULE,
	.open = vga_auto_calibration_open,
	.read = seq_read,
	.write = vga_auto_calibration_write,
	.llseek = seq_lseek,
	.release = single_release,
};

int inno_vga_custom_debugfs_create(struct drm_connector *connector)
{
	int i = 0;
	struct dentry *ent0 = NULL;
	int ret = 0;
	struct vga_device_t *vga_dev = container_of(connector, struct vga_device_t, connector);
	struct drm_minor *minor = connector->dev->primary;

	const char *debug_name[] = {
		"edid_info",
		"inno_status",
		"auto_calibration"
	};
	const struct file_operations *debug_fops[] = {
		&innodpu_vga_edid_info_fops,
		&innodpu_vga_status_fops,
		&innodpu_vga_auto_calibration_fops
	};

	if (connector->debugfs_entry) {
		for (i = 0; i < INNO_ARRAY_SIZE(debug_name); i++) {
			ent0 = debugfs_create_file(debug_name[i], S_IRUGO | S_IWUSR,
				connector->debugfs_entry, connector, debug_fops[i]);
			if (!ent0) {
				fh2m_innodpu_err(vga_dev->dev, "create debug node /sys/kernel/debug/dri/%d/%s/%s Error.\n",
					minor->index, connector->name, debug_name[i]);
				ret = -1;
			}
		}
	}

	return ret;
}
#endif
