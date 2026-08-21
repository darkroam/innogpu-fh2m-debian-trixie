/*************************************************************************/ /*!
@File			innodpu_hdmi_debugfs.c
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
#include "innodpu_hdmi.h"
#include "innodpu_hdmi_debugfs.h"

#if defined(CONFIG_DEBUG_FS)
static int inno_hdmi_edidparse_show(struct seq_file *seq, void *d)
{
	struct hdmi_chip_t *chip = NULL;

	INNODPU_WARN_RETURN_CHECK(seq, -EINVAL);

	chip = (struct hdmi_chip_t *)seq->private;
	INNODPU_WARN_RETURN_CHECK(chip, -EINVAL);

	if (chip->connector_detect(chip) != connector_status_connected) {
		fh2m_inno_seq_printf(seq, "\n%s\n", "HDMI connector is not plug in.");
		return 0;
	}

	fh2m_inno_seq_printf(seq, "\n%s\n", "innosilicon hdmi edid info:");

	if (chip->hdmi_edid_parse)
		chip->hdmi_edid_parse(seq, chip);

	return 0;
}

static int inno_hdmi_edidparse_open(struct inode *inode, struct file *file)
{
	struct hdmi_chip_t *chip = NULL;

	INNODPU_WARN_RETURN_CHECK(inode, -EINVAL);
	INNODPU_WARN_RETURN_CHECK(file, -EINVAL);

	chip = (struct hdmi_chip_t *)inode->i_private;
	INNODPU_WARN_RETURN_CHECK(chip, -EINVAL);

	single_open(file, inno_hdmi_edidparse_show, chip);

	return 0;
}

static const struct file_operations inno_hdmi_edidparse_fops = {
	.owner   = THIS_MODULE,
	.open    = inno_hdmi_edidparse_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

static int inno_hdmi_edidparse_create(struct dentry *entry, struct hdmi_debugfs_item_t *edid_info)
{
	struct dentry *ent = NULL;
	struct hdmi_chip_t *chip = NULL;
	char name[16] = {0};

	INNODPU_WARN_RETURN_CHECK(entry, -EINVAL);
	INNODPU_WARN_RETURN_CHECK(edid_info, -EINVAL);

	chip = edid_info->chip;
	INNODPU_WARN_RETURN_CHECK(chip, -EINVAL);

	snprintf(name, sizeof(name), "%s", (char *)edid_info->data);

	ent = debugfs_create_file(name, S_IRUGO | S_IWUSR, entry, chip,
				 &inno_hdmi_edidparse_fops);

	if (!ent) {
		fh2m_innodpu_err(chip->dev, "%s create custom debufs(%s) failed.\n",
				chip->name, name);
		return -ENOENT;
	}

	edid_info->ent = ent;

	return 0;
}

static int inno_hdmi_hw_func_test(struct seq_file *seq, void *data)
{
	struct hdmi_chip_t *chip = NULL;
	uint32_t success_num = 0;
	uint32_t fail_num = 0;
	uint32_t hw_self_test_num = 5;
	struct hdmi_device_t *inno_hdmi = NULL;
	int connect_status = 0;
	inno_edid *edid = NULL;
	int ret = 0;

	INNODPU_WARN_RETURN_CHECK(seq, -EINVAL);

	chip = (struct hdmi_chip_t *)seq->private;
	INNODPU_WARN_RETURN_CHECK(chip, -EINVAL);

	inno_hdmi = container_of(chip, struct hdmi_device_t, chip);

	fh2m_inno_seq_printf(seq, "==%s self test result:==\n", inno_hdmi->name);
	fh2m_inno_seq_printf(seq, "\tType of self-test   :%s\n",
			(chip->hw_self_test_mode == INNO_HW_SELF_TEST_EDID) ? "EDID reading" : "unsupport");
	fh2m_inno_seq_printf(seq, "\tNumber of self-tests:%d\n", hw_self_test_num);

#if ((DRM_VERSION >= KERNEL_VERSION(4, 12, 0)))
	if (inno_hdmi->connector.helper_private->detect_ctx)
		connect_status = inno_hdmi->connector.helper_private->detect_ctx(&inno_hdmi->connector, NULL, 0);
#else
	if (inno_hdmi->connector.funcs->detect)
		connect_status = inno_hdmi->connector.funcs->detect(&inno_hdmi->connector, 0);
#endif

	if (connect_status != connector_status_connected) {
		fh2m_inno_seq_printf(seq, "\t%s disconnect, end hardwre function self-test\n", inno_hdmi->name);
		return 0;
	}

	if (chip->hal_edid_mode != EDID_AUTO_READ) {
		fh2m_inno_seq_printf(seq, "\tedid read from custom, parsing failed\n");
		return 0;
	}

	while (hw_self_test_num--) {
		switch (chip->hw_self_test_mode) {
		case INNO_HW_SELF_TEST_EDID:
			if (inno_hdmi->chip.connector_get_edid) {
				edid = inno_hdmi->chip.connector_get_edid(&inno_hdmi->connector, &inno_hdmi->chip);
				if (edid && fh2m_inno_drm_edid_header_is_valid(edid) == 8) {
					ret = 0;
				} else {
					ret = -EFAULT;
				}
			} else {
				fh2m_inno_seq_printf(seq, "\t%s Unsupported hardware platform!\n", inno_hdmi->name);
				return 0;
			}
		break;
		default:
			ret = -EINVAL;
			fh2m_innodpu_err(chip->dev, "%s:unsupport hw self-test mode:%d\n", chip->hw_self_test_mode);
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

static int inno_hdmi_hw_func_open(struct inode *inode, struct file *file)
{
	struct hdmi_chip_t *chip = NULL;

	INNODPU_WARN_RETURN_CHECK(inode, -EINVAL);
	INNODPU_WARN_RETURN_CHECK(file, -EINVAL);

	chip = (struct hdmi_chip_t *)inode->i_private;
	INNODPU_WARN_RETURN_CHECK(chip, -EINVAL);

	single_open(file, inno_hdmi_hw_func_test, chip);

	return 0;
}

static ssize_t inno_hdmi_hw_func_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
	struct seq_file *m = NULL;
	struct hdmi_chip_t *chip = NULL;
	char kbuf[16] = {0};

	INNODPU_WARN_RETURN_CHECK(file, -EINVAL);
	INNODPU_WARN_RETURN_CHECK(buf, -EINVAL);

	m = file->private_data;
	INNODPU_WARN_RETURN_CHECK(m, -EINVAL);

	chip = m->private;
	INNODPU_WARN_RETURN_CHECK(chip, -EINVAL);

	if (size > sizeof(kbuf) - 1)
		return -EINVAL;

	if (fh2m_inno_copy_from_user(kbuf, buf, size))
		return -EFAULT;

	kbuf[size] = '\0';
	kbuf[0] = fh2m_inno_simple_strtoul(kbuf, NULL, 10);

	chip->hw_self_test_mode = kbuf[0];
	hdmi_info(chip->dev, "hardware self-test mode:%d\n", chip->hw_self_test_mode);

	return size;
}

static const struct file_operations inno_hdmi_hw_test_fops = {
	.owner   = THIS_MODULE,
	.open    = inno_hdmi_hw_func_open,
	.read    = seq_read,
	.write   = inno_hdmi_hw_func_write,
	.llseek  = seq_lseek,
	.release = single_release,
};

static int inno_hdmi_hw_test_create(struct dentry *entry, struct hdmi_debugfs_item_t *hw_test)
{
	struct   dentry *ent = NULL;
	struct hdmi_chip_t *chip = NULL;
	char name[64] = {0};

	INNODPU_WARN_RETURN_CHECK(entry, -EINVAL);
	INNODPU_WARN_RETURN_CHECK(hw_test, -EINVAL);
	INNODPU_WARN_RETURN_CHECK(hw_test->chip, -EINVAL);

	chip = hw_test->chip;
	snprintf(name, sizeof(name), "%s", (char *)hw_test->data);

	ent = debugfs_create_file(name, S_IRUGO | S_IWUSR, entry, chip,
				 &inno_hdmi_hw_test_fops);
	if (!ent) {
		fh2m_innodpu_err(chip->dev, "%s create custom debufs(%s) failed.\n",
			chip->name, name);
		return -ENOENT;
	}

	hw_test->ent = ent;

	return 0;
}

static int inno_hdmi_bisttest_show(struct seq_file *seq, void *d)
{
	struct hdmi_chip_t *chip = NULL;
	uint32_t val = 0;

	INNODPU_WARN_RETURN_CHECK(seq, -EINVAL);

	chip = (struct hdmi_chip_t *)seq->private;
	INNODPU_WARN_RETURN_CHECK(chip, -EINVAL);

	fh2m_inno_seq_printf(seq, "==cur status==\n");
	fh2m_inno_seq_printf(seq, "test_mode: %s\n", chip->test_mode?"true":"false");

	fh2m_hal_reg_read32(chip->parent, chip->reg_module, REG_ENTITY0201, &val);
	fh2m_inno_seq_printf(seq, "bist regval: 0x%08x\n", val);

	fh2m_inno_seq_printf(seq, "\n==help==\n"\
			"write 1, will set test_mode to true, and set bist to special color bar(0x40).\n"\
			"write 0, will set test_mode to false, and set bist register default(0x50).\n");

	return 0;
}

static int inno_hdmi_bisttest_open(struct inode *inode, struct file *file)
{
	struct hdmi_chip_t *chip = NULL;

	INNODPU_WARN_RETURN_CHECK(inode, -EINVAL);
	INNODPU_WARN_RETURN_CHECK(file, -EINVAL);

	chip = (struct hdmi_chip_t *)inode->i_private;
	INNODPU_WARN_RETURN_CHECK(chip, -EINVAL);

	single_open(file, inno_hdmi_bisttest_show, chip);

	return 0;
}

static ssize_t inno_hdmi_bisttest_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
	struct seq_file *m = NULL;
	struct hdmi_chip_t *chip = NULL;
	char kbuf[16] = {0};

	INNODPU_WARN_RETURN_CHECK(file, -EINVAL);
	INNODPU_WARN_RETURN_CHECK(file->private_data, -EINVAL);

	m = file->private_data;
	chip = m->private;
	INNODPU_WARN_RETURN_CHECK(chip, -EINVAL);

	if (size > sizeof(kbuf) - 1)
		return -EINVAL;

	if (fh2m_inno_copy_from_user(kbuf, buf, size))
		return -EFAULT;

	kbuf[size] = '\0';
	kbuf[0] = fh2m_inno_simple_strtoul(kbuf, NULL, 10);
	if (kbuf[0] == 1) {
		hdmi_info(chip->dev, "bist_enable\n");
		chip->test_mode = 0x1;
		fh2m_hal_reg_write32(chip->parent, chip->reg_module, REG_ENTITY0201, 0x40);
	} else if (kbuf[0] == 0) {
		hdmi_info(chip->dev, "bist_disable\n");
		chip->test_mode = 0x0;
		fh2m_hal_reg_write32(chip->parent, chip->reg_module, REG_ENTITY0201, 0x50);
	} else
		return -EINVAL;

	return size;
}

static const struct file_operations inno_hdmi_bisttest_fops = {
	.owner   = THIS_MODULE,
	.open    = inno_hdmi_bisttest_open,
	.read    = seq_read,
	.write   = inno_hdmi_bisttest_write,
	.llseek  = seq_lseek,
	.release = single_release,
};


static int inno_hdmi_bisttest_create(struct dentry *entry, struct hdmi_debugfs_item_t *bisttest)
{
	struct   dentry *ent = NULL;
	struct hdmi_chip_t *chip = NULL;
	char name[64] = {0};

	INNODPU_WARN_RETURN_CHECK(entry, -EINVAL);
	INNODPU_WARN_RETURN_CHECK(bisttest, -EINVAL);

	chip = bisttest->chip;
	INNODPU_WARN_RETURN_CHECK(chip, -EINVAL);

	snprintf(name, sizeof(name), "%s", (char *)bisttest->data);

	ent = debugfs_create_file(name, S_IRUGO | S_IWUSR, entry, chip,
				 &inno_hdmi_bisttest_fops);
	if (!ent) {
		fh2m_innodpu_err(chip->dev, "%s create custom debufs(%s) failed.\n",
			chip->name, name);
		return -ENOENT;
	}

	bisttest->ent = ent;

	return 0;
}

static int inno_hdmi_status_show(struct seq_file *seq, void *d)
{
	struct hdmi_chip_t *chip = NULL;
	struct hdmi_device_t *inno_hdmi = NULL;
	int i = 0, tmp = 0;
	uint32_t vic = 0;
	uint32_t rx_rdata[2] = {0};
	const struct drm_display_mode *mode = NULL;
	const struct drm_display_mode *rmode = NULL;
	const struct drm_display_mode *current_mode = NULL;
	const struct drm_display_mode *native_mode = NULL;
	const struct drm_display_mode *adjusted_mode = NULL;
	const struct drm_display_mode *preferred_mode = NULL;
	size_t size = 0;
	char buf[MONITOR_NAME_LEN] = {0};
	char *dpms_string[] = {"on", "standby", "suspend", "off", "invalid"};
	char *edid_string[] = {"EDID_AUTO_READ", "EDID_STR_PUSH", "EDID_USER_DEFINE", "EDID_ZOOM_ENABLE", "UNKOWN"};

	chip = (struct hdmi_chip_t *)seq->private;
	INNODPU_WARN_RETURN_CHECK(chip, -EINVAL);

	inno_hdmi = container_of(chip, struct hdmi_device_t, chip);
	INNODPU_WARN_RETURN_CHECK(inno_hdmi, -EINVAL);

	fh2m_inno_seq_printf(seq, "HDMI-%d STATUS\n", chip->id);
	fh2m_inno_seq_printf(seq, "max width:%d, max height:%d\n", chip->max_width, chip->max_height);

	fh2m_inno_seq_printf(seq, "possible_crtc: %#x\n", chip->possible_crtc);
	fh2m_inno_seq_printf(seq, "replace_timing: %d\n", chip->replace_timing);

	if (inno_hdmi->connector.state && inno_hdmi->connector.state->crtc)
		fh2m_inno_seq_printf(seq, "video_sel: %s match %s\n",
				inno_hdmi->name, inno_hdmi->connector.state->crtc->name);
	else
		fh2m_inno_seq_printf(seq, "video_sel: %s match %s\n", inno_hdmi->name, "unknow");

	if (chip->hpd_status_detect) {
		tmp = chip->hpd_status_detect(chip);
		fh2m_inno_seq_printf(seq, "hpd_status: %s\n", tmp?"connected":"disconnected");
	} else
		fh2m_inno_seq_printf(seq, "hpd_status: %s\n", "unknown");

	tmp = inno_hdmi->connector.dpms;
	switch (tmp) {
		case 0:
		case 1:
		case 2:
		case 3:
			break;
		default:
			tmp = 4;
			break;
	}
	fh2m_inno_seq_printf(seq, "dpms_value: %s\n", dpms_string[tmp]);

	if (chip->hdmi_prepll_islock) {
		tmp = chip->hdmi_prepll_islock(chip);
		fh2m_inno_seq_printf(seq, "prepll_status: %s\n", tmp?"lock":"unlock");
	}

	if (chip->hdmi_postpll_islock) {
		tmp = chip->hdmi_postpll_islock(chip);
		fh2m_inno_seq_printf(seq, "postpll_status: %s\n", tmp?"lock":"unlock");
	}

	if (fh2m_inno_connector_is_support_scdc(&inno_hdmi->connector)) {
#if (DRM_VERSION >= KERNEL_VERSION(4, 12, 0))
		fh2m_inno_seq_printf(seq, "scdc: %s\n", "support scdc");
#else
		fh2m_inno_seq_printf(seq, "scdc: %s\n", "support scdc[Force by old kernel]");
#endif

		if (chip->hdmi_read_rx_scramb) {
			chip->hdmi_read_rx_scramb(chip, rx_rdata);
			fh2m_inno_seq_printf(seq, "rx_scramb_status: 20H:0x%x, 21H:0x%x\n", \
					rx_rdata[0], rx_rdata[1]);
		} else {
			fh2m_inno_seq_printf(seq, "rx_scramb_status: unsupport read\n");
		}
	} else
		fh2m_inno_seq_printf(seq, "scdc: %s\n", "not support scdc");

	if (chip->hdmi_read_tx_scramb) {
		tmp = chip->hdmi_read_tx_scramb(chip);
		fh2m_inno_seq_printf(seq, "tx_scramb_status: 0x%x\n", tmp);
	} else {
		fh2m_inno_seq_printf(seq, "tx_scramb_status: unsupport read\n");
	}

	if (chip->max_pclk_rx > 0)
		fh2m_inno_seq_printf(seq, "max clock from edid: %dkHz\n", chip->max_pclk_rx);

	fh2m_inno_seq_printf(seq, "mode:\n");
	if (inno_hdmi->connector.state && inno_hdmi->connector.state->crtc) {
		if (inno_hdmi->connector.state->crtc) {
			current_mode = &inno_hdmi->connector.state->crtc->mode;
			if (inno_hdmi->connector.state->crtc->state) {
				adjusted_mode = &inno_hdmi->connector.state->crtc->state->adjusted_mode;
			}
		}
	}

	if (is_native_mode_valid(&inno_hdmi->native_mode)) {
		native_mode = &inno_hdmi->native_mode;
	}

	list_for_each_entry(mode, &inno_hdmi->connector.modes, head) {
		vic = fh2m_inno_drm_match_cea_mode(mode);
		fh2m_inno_seq_printf(seq, "    VIC:%d\t"DRM_MODE_FMT, vic, DRM_MODE_ARG(mode));

		if (current_mode && innodpu_modes_equal(mode, current_mode))
			fh2m_inno_seq_printf(seq, "  (current)");

		if (mode->clock > chip->max_pclk_rx) {
			if (chip->max_pclk_rx > 0)
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

		if (inno_hdmi->chip.replace_timing) {
			rmode = innodpu_modes_match_replace_table(mode, NULL, NULL);
			if (!rmode)
				goto next;

			if(is_special_mode(rmode)) {
				fh2m_inno_seq_printf(seq, "  (warnning: unsupport!!!)");
			} else {
				vic = fh2m_inno_drm_match_cea_mode(rmode);
				if (!drm_mode_equal(mode, rmode))
					fh2m_inno_seq_printf(seq, "\n    => :%d\t"DRM_MODE_FMT, vic, DRM_MODE_ARG(rmode));
			}
		}

		next:
		fh2m_inno_seq_printf(seq, "\n");
	}

	fh2m_inno_seq_printf(seq, "logo status: %s\n",
					innodpu_modes_equal(adjusted_mode, preferred_mode) ? "show" : "maybe show");
	tmp = chip->hal_edid_mode;
	switch (tmp) {
		case EDID_AUTO_READ:
			i = 0;
			break;
		case EDID_STR_PUSH:
			i = 1;
			break;
		case EDID_USER_DEFINE:
			i = 2;
			break;
		case EDID_ZOOM_ENABLE:
			i = 3;
			break;
		default:
			i = 4;
			break;
	}
	fh2m_inno_seq_printf(seq, "edid_src: %s\n", edid_string[i]);

	if ((inno_hdmi->connector.status == 1) && tmp != EDID_STR_PUSH) {
		drm_edid_get_monitor_name((struct edid *)chip->edid_buf, buf, INNO_ARRAY_SIZE(buf));
		fh2m_inno_seq_printf(seq, "monitor: %s\n", buf);
	} else {
		fh2m_inno_seq_printf(seq, "monitor: Unknown\n");
	}

	if (inno_hdmi->connector.edid_blob_ptr)
		size = inno_hdmi->connector.edid_blob_ptr->length;
	fh2m_inno_seq_printf(seq, "edid_value: ");
	for (i = 0; i < size; i++) {
		if (((i % 16) == 0))
			fh2m_inno_seq_printf(seq, "\n    ");
		fh2m_inno_seq_printf(seq, "%.2x", chip->edid_buf[i]);
	}
	fh2m_inno_seq_printf(seq, "\n");

	return 0;
}

static int inno_hdmi_status_open(struct inode *inode, struct file *file)
{
	struct hdmi_chip_t *chip = NULL;

	INNODPU_WARN_RETURN_CHECK(inode, -EINVAL);
	INNODPU_WARN_RETURN_CHECK(file, -EINVAL);

	chip = (struct hdmi_chip_t *)inode->i_private;
	INNODPU_WARN_RETURN_CHECK(chip, -EINVAL);

	single_open(file, inno_hdmi_status_show, chip);

	return 0;
}

static const struct file_operations inno_hdmi_status_fops = {
	.owner   = THIS_MODULE,
	.open    = inno_hdmi_status_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

static int inno_hdmi_status_create(struct dentry *entry, struct hdmi_debugfs_item_t *hdmi_status)
{
	struct   dentry *ent;
	struct hdmi_chip_t *chip;
	char name[64];

	if (!hdmi_status || !hdmi_status->chip)
		return -EINVAL;

	chip = hdmi_status->chip;
	snprintf(name, sizeof(name), "%s", (char *)hdmi_status->data);

	ent = debugfs_create_file(name, S_IRUGO | S_IWUSR, entry, chip,
				 &inno_hdmi_status_fops);
	if (!ent) {
		fh2m_innodpu_err(chip->dev, "%s create custom debufs(%s) failed.\n",
				chip->name, name);
		return -ENOENT;
	}

	hdmi_status->ent = ent;

	return 0;
}

int inno_hdmi_custom_debugfs_create(struct dentry *entry, struct hdmi_chip_t *chip)
{
	int ret = 0;
	struct hdmi_debugfs *custom;
	struct hdmi_debugfs_item_t *item;
	struct list_head *pos = NULL, *q = NULL;

	if (!entry || !chip)
		return -EINVAL;

	/* custom edid_info */
	custom = &chip->hdmi_ext->custom_edidparse;
	custom->debugfs_root = entry;

	mutex_lock(&custom->debugfs_lock);
	list_for_each_safe(pos, q, &custom->debugfs_list) {
		item = list_entry(pos, struct hdmi_debugfs_item_t, list);

		if (!item || !item->chip || !item->data)
			break;

		if ((item->chip != chip) ||
			(item->status != HDMI_DEBUGS_STATUS_UNINITED))
			continue;

		ret = inno_hdmi_edidparse_create(entry, item);

		if (!ret) {
			item->status = HDMI_DEBUGS_STATUS_INITED;
		} else {
			item->status = HDMI_DEBUGS_STATUS_FAILED;
		}
	}
	mutex_unlock(&custom->debugfs_lock);

	/* custom bisttest */
	custom = &chip->hdmi_ext->custom_bisttest;
	custom->debugfs_root = entry;

	mutex_lock(&custom->debugfs_lock);
	list_for_each_safe(pos, q, &custom->debugfs_list) {
		item = list_entry(pos, struct hdmi_debugfs_item_t, list);

		if (!item || !item->chip)
			break;

		if ((item->chip != chip) ||
			(item->status != HDMI_DEBUGS_STATUS_UNINITED))
			continue;

		ret = inno_hdmi_bisttest_create(entry, item);

		if (!ret) {
			item->status = HDMI_DEBUGS_STATUS_INITED;
		} else {
			item->status = HDMI_DEBUGS_STATUS_FAILED;
		}
	}
	mutex_unlock(&custom->debugfs_lock);

	/* custom inno_status */
	custom = &chip->hdmi_ext->custom_hdmi_status;
	custom->debugfs_root = entry;
	mutex_lock(&custom->debugfs_lock);
	list_for_each_safe(pos, q, &custom->debugfs_list) {
		item = list_entry(pos, struct hdmi_debugfs_item_t, list);

		if (!item || !item->chip || !item->data)
			break;

		if ((item->chip != chip) ||
				(item->status != HDMI_DEBUGS_STATUS_UNINITED))
				continue;

		ret = inno_hdmi_status_create(entry, item);

		if (!ret) {
			item->status = HDMI_DEBUGS_STATUS_INITED;
		} else {
			item->status = HDMI_DEBUGS_STATUS_FAILED;
		}
	}
	mutex_unlock(&custom->debugfs_lock);

	/* custom hw_self_test */
	custom = &chip->hdmi_ext->custom_hw_self_test;
	custom->debugfs_root = entry;
	mutex_lock(&custom->debugfs_lock);
	list_for_each_safe(pos, q, &custom->debugfs_list) {
		item = list_entry(pos, struct hdmi_debugfs_item_t, list);

		if (!item || !item->chip)
			break;

		if ((item->chip != chip) ||
			(item->status != HDMI_DEBUGS_STATUS_UNINITED))
			continue;

		ret = inno_hdmi_hw_test_create(entry, item);

		if (!ret) {
			item->status = HDMI_DEBUGS_STATUS_INITED;
		} else {
			item->status = HDMI_DEBUGS_STATUS_FAILED;
		}
	}
	mutex_unlock(&custom->debugfs_lock);

	return 0;
}

static void inno_hdmi_custom_debugfs_helper_remove(struct hdmi_debugfs *custom)
{
	struct list_head *pos = NULL, *q = NULL;
	struct hdmi_debugfs_item_t *item;

	mutex_lock(&custom->debugfs_lock);
	list_for_each_safe(pos, q, &custom->debugfs_list) {
		item = list_entry(pos, struct hdmi_debugfs_item_t, list);
		if (item->ent && (HDMI_DEBUGS_STATUS_INITED == item->status))
			debugfs_remove(item->ent);
		kfree(item);
	}
	mutex_unlock(&custom->debugfs_lock);
}

void inno_hdmi_custom_debugfs_remove(struct dentry *entry, struct hdmi_chip_t *chip)
{
	struct hdmi_debugfs *custom;

	if (!entry || !chip)
		return;

	custom = &chip->hdmi_ext->custom_bisttest;
	inno_hdmi_custom_debugfs_helper_remove(custom);

	custom = &chip->hdmi_ext->custom_hw_self_test;
	inno_hdmi_custom_debugfs_helper_remove(custom);
}
#endif
