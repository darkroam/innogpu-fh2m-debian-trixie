#include "innodpu_common.h"
#include "hal_interface.h"
#include "innodpu_compatibility.h"
#include "inno_debug.h"

#if (DRM_VERSION >= KERNEL_VERSION(5, 19, 0))
#include <drm/display/drm_dp_helper.h>
#else
#include <drm/drm_dp_helper.h>
#endif

#include "innodpu_hdmi.h"
#include "innodpu_hdmi_common.h"
#include "innodpu_hdmi_debugfs.h"
#include "innodpu_connector.h"

int g0_soc_hdmi_debugfs_edidparse_init(struct hdmi_chip_t *chip)
{
	struct hdmi_debugfs_item_t *edid_info;

	INIT_LIST_HEAD(&chip->hdmi_ext->custom_edidparse.debugfs_list);
	mutex_init(&chip->hdmi_ext->custom_edidparse.debugfs_lock);

	edid_info = (struct hdmi_debugfs_item_t *)kmalloc(
							sizeof(struct hdmi_debugfs_item_t), fh2m_hal_get_inno_gfp_kernel());
	edid_info->chip = chip;
	edid_info->ent = NULL;
	edid_info->status = HDMI_DEBUGS_STATUS_UNINITED;
	edid_info->data = "edid_info";

	mutex_lock(&chip->hdmi_ext->custom_edidparse.debugfs_lock);
	list_add(&edid_info->list, &chip->hdmi_ext->custom_edidparse.debugfs_list);
	mutex_unlock(&chip->hdmi_ext->custom_edidparse.debugfs_lock);

	return 0;
}

int g0_soc_hdmi_debugfs_bisttest_init(struct hdmi_chip_t *chip)
{
	struct hdmi_debugfs_item_t *bisttest;

	INIT_LIST_HEAD(&chip->hdmi_ext->custom_bisttest.debugfs_list);
	mutex_init(&chip->hdmi_ext->custom_bisttest.debugfs_lock);

	bisttest = (struct hdmi_debugfs_item_t *)kmalloc(
							sizeof(struct hdmi_debugfs_item_t), fh2m_hal_get_inno_gfp_kernel());
	bisttest->chip = chip;
	bisttest->ent = NULL;
	bisttest->status = HDMI_DEBUGS_STATUS_UNINITED;
	bisttest->data = "bisttest";

	mutex_lock(&chip->hdmi_ext->custom_bisttest.debugfs_lock);
	list_add(&bisttest->list, &chip->hdmi_ext->custom_bisttest.debugfs_list);
	mutex_unlock(&chip->hdmi_ext->custom_bisttest.debugfs_lock);

	return 0;
}

int g0_soc_hdmi_debugfs_hdmi_status_init(struct hdmi_chip_t *chip)
{
	struct hdmi_debugfs_item_t *hdmi_status;

	INIT_LIST_HEAD(&chip->hdmi_ext->custom_hdmi_status.debugfs_list);
	mutex_init(&chip->hdmi_ext->custom_hdmi_status.debugfs_lock);

	hdmi_status = (struct hdmi_debugfs_item_t *)kmalloc(
							sizeof(struct hdmi_debugfs_item_t), fh2m_hal_get_inno_gfp_kernel());
	hdmi_status->chip = chip;
	hdmi_status->ent = NULL;
	hdmi_status->status = HDMI_DEBUGS_STATUS_UNINITED;
	hdmi_status->data = "inno_status";

	mutex_lock(&chip->hdmi_ext->custom_hdmi_status.debugfs_lock);
	list_add(&hdmi_status->list, &chip->hdmi_ext->custom_hdmi_status.debugfs_list);
	mutex_unlock(&chip->hdmi_ext->custom_hdmi_status.debugfs_lock);

	return 0;
}

int g0_soc_hdmi_debugfs_hw_test_init(struct hdmi_chip_t *chip)
{
	struct hdmi_debugfs_item_t *hw_test;

	INIT_LIST_HEAD(&chip->hdmi_ext->custom_hw_self_test.debugfs_list);
	mutex_init(&chip->hdmi_ext->custom_hw_self_test.debugfs_lock);

	hw_test = (struct hdmi_debugfs_item_t *)kmalloc(
					sizeof(struct hdmi_debugfs_item_t), fh2m_hal_get_inno_gfp_kernel());
	hw_test->chip = chip;
	hw_test->ent = NULL;
	hw_test->status = HDMI_DEBUGS_STATUS_UNINITED;
	hw_test->data = "hw_self_test";

	mutex_lock(&chip->hdmi_ext->custom_hw_self_test.debugfs_lock);
	list_add(&hw_test->list, &chip->hdmi_ext->custom_hw_self_test.debugfs_list);
	mutex_unlock(&chip->hdmi_ext->custom_hw_self_test.debugfs_lock);

	return 0;
}

int g1p_soc_hdmi_debugfs_edidparse_init(struct hdmi_chip_t *chip)
{
	struct hdmi_debugfs_item_t *edid_info;

	INIT_LIST_HEAD(&chip->hdmi_ext->custom_edidparse.debugfs_list);
	mutex_init(&chip->hdmi_ext->custom_edidparse.debugfs_lock);

	edid_info = (struct hdmi_debugfs_item_t *)kmalloc(
							sizeof(struct hdmi_debugfs_item_t), fh2m_hal_get_inno_gfp_kernel());
	edid_info->chip = chip;
	edid_info->ent = NULL;
	edid_info->status = HDMI_DEBUGS_STATUS_UNINITED;
	edid_info->data = "edid_info";

	mutex_lock(&chip->hdmi_ext->custom_edidparse.debugfs_lock);
	list_add(&edid_info->list, &chip->hdmi_ext->custom_edidparse.debugfs_list);
	mutex_unlock(&chip->hdmi_ext->custom_edidparse.debugfs_lock);

	return 0;
}

int g1p_soc_hdmi_debugfs_bisttest_init(struct hdmi_chip_t *chip)
{
	struct hdmi_debugfs_item_t *bisttest;

	INIT_LIST_HEAD(&chip->hdmi_ext->custom_bisttest.debugfs_list);
	mutex_init(&chip->hdmi_ext->custom_bisttest.debugfs_lock);

	bisttest = (struct hdmi_debugfs_item_t *)kmalloc(
							sizeof(struct hdmi_debugfs_item_t), fh2m_hal_get_inno_gfp_kernel());
	bisttest->chip = chip;
	bisttest->ent = NULL;
	bisttest->status = HDMI_DEBUGS_STATUS_UNINITED;
	bisttest->data = "bisttest";

	mutex_lock(&chip->hdmi_ext->custom_bisttest.debugfs_lock);
	list_add(&bisttest->list, &chip->hdmi_ext->custom_bisttest.debugfs_list);
	mutex_unlock(&chip->hdmi_ext->custom_bisttest.debugfs_lock);

	return 0;
}

int g1p_soc_hdmi_debugfs_hw_test_init(struct hdmi_chip_t *chip)
{
	struct hdmi_debugfs_item_t *hw_test;

	INIT_LIST_HEAD(&chip->hdmi_ext->custom_hw_self_test.debugfs_list);
	mutex_init(&chip->hdmi_ext->custom_hw_self_test.debugfs_lock);

	hw_test = (struct hdmi_debugfs_item_t *)kmalloc(
			sizeof(struct hdmi_debugfs_item_t), fh2m_hal_get_inno_gfp_kernel());
	hw_test->chip = chip;
	hw_test->ent = NULL;
	hw_test->status = HDMI_DEBUGS_STATUS_UNINITED;
	hw_test->data = "hw_self_test";

	mutex_lock(&chip->hdmi_ext->custom_hw_self_test.debugfs_lock);
	list_add(&hw_test->list, &chip->hdmi_ext->custom_hw_self_test.debugfs_list);
	mutex_unlock(&chip->hdmi_ext->custom_hw_self_test.debugfs_lock);

	return 0;
}

int g1p_soc_hdmi_debugfs_hdmi_status_init(struct hdmi_chip_t *chip)
{
	struct hdmi_debugfs_item_t *hdmi_status;

	INIT_LIST_HEAD(&chip->hdmi_ext->custom_hdmi_status.debugfs_list);
	mutex_init(&chip->hdmi_ext->custom_hdmi_status.debugfs_lock);

	hdmi_status = (struct hdmi_debugfs_item_t *)kmalloc(sizeof(struct hdmi_debugfs_item_t), fh2m_hal_get_inno_gfp_kernel());
	hdmi_status->chip = chip;
	hdmi_status->ent = NULL;
	hdmi_status->status = HDMI_DEBUGS_STATUS_UNINITED;
	hdmi_status->data = "inno_status";

	mutex_lock(&chip->hdmi_ext->custom_hdmi_status.debugfs_lock);
	list_add(&hdmi_status->list, &chip->hdmi_ext->custom_hdmi_status.debugfs_list);
	mutex_unlock(&chip->hdmi_ext->custom_hdmi_status.debugfs_lock);

	return 0;
}

int g1_soc_hdmi_debugfs_bisttest_init(struct hdmi_chip_t *chip)
{

	INIT_LIST_HEAD(&chip->hdmi_ext->custom_bisttest.debugfs_list);
	mutex_init(&chip->hdmi_ext->custom_bisttest.debugfs_lock);

	return 0;
}

int g1_soc_hdmi_debugfs_edidparse_init(struct hdmi_chip_t *chip)
{
	INIT_LIST_HEAD(&chip->hdmi_ext->custom_edidparse.debugfs_list);
	mutex_init(&chip->hdmi_ext->custom_edidparse.debugfs_lock);

	return 0;
}

int g1_soc_hdmi_debugfs_hw_test_init(struct hdmi_chip_t *chip)
{
	struct hdmi_debugfs_item_t *hw_test;

	INIT_LIST_HEAD(&chip->hdmi_ext->custom_hw_self_test.debugfs_list);
	mutex_init(&chip->hdmi_ext->custom_hw_self_test.debugfs_lock);

	hw_test = (struct hdmi_debugfs_item_t *)kmalloc(
					sizeof(struct hdmi_debugfs_item_t), fh2m_hal_get_inno_gfp_kernel());
	hw_test->chip = chip;
	hw_test->ent = NULL;
	hw_test->status = HDMI_DEBUGS_STATUS_UNINITED;
	hw_test->data = "hw_self_test";

	mutex_lock(&chip->hdmi_ext->custom_hw_self_test.debugfs_lock);
	list_add(&hw_test->list, &chip->hdmi_ext->custom_hw_self_test.debugfs_list);
	mutex_unlock(&chip->hdmi_ext->custom_hw_self_test.debugfs_lock);

	return 0;
}

int g1_soc_hdmi_debugfs_hdmi_status_init(struct hdmi_chip_t *chip)
{
	INIT_LIST_HEAD(&chip->hdmi_ext->custom_hdmi_status.debugfs_list);
	mutex_init(&chip->hdmi_ext->custom_hdmi_status.debugfs_lock);

	return 0;
}

void hdmi_set_hpg_status(struct hdmi_chip_t *chip, int val)
{
	atomic64_set(&chip->hdmi_ext->hpg_status, val);
}

long long hdmi_get_hpg_status(struct hdmi_chip_t *chip)
{
	return atomic64_read(&chip->hdmi_ext->hpg_status);
}

int hdmi_ext_init(struct hdmi_chip_t *chip)
{
	struct hdmi_ext_t *ext = NULL;

	ext = fh2m_inno_kzalloc_kernel(sizeof(struct hdmi_ext_t));
	if (!ext) {
		fh2m_innodpu_err(chip->dev, "hdmi ext init faild\n");
		return -ENOMEM;
	}

	chip->hdmi_ext = ext;

	return 0;
}

void hdmi_ext_fini(struct hdmi_chip_t *chip)
{
	struct hdmi_ext_t *ext = chip->hdmi_ext;

	if (ext)
		fh2m_inno_kfree(ext);

	chip->hdmi_ext = NULL;
}

int g0m_soc_hdmi_debugfs_edidparse_init(struct hdmi_chip_t *chip)
{
	struct hdmi_debugfs_item_t *edid_info;

	INIT_LIST_HEAD(&chip->hdmi_ext->custom_edidparse.debugfs_list);
	mutex_init(&chip->hdmi_ext->custom_edidparse.debugfs_lock);

	edid_info = (struct hdmi_debugfs_item_t *)kmalloc(
							sizeof(struct hdmi_debugfs_item_t), fh2m_hal_get_inno_gfp_kernel());
	edid_info->chip = chip;
	edid_info->ent = NULL;
	edid_info->status = HDMI_DEBUGS_STATUS_UNINITED;
	edid_info->data = "edid_info";

	mutex_lock(&chip->hdmi_ext->custom_edidparse.debugfs_lock);
	list_add(&edid_info->list, &chip->hdmi_ext->custom_edidparse.debugfs_list);
	mutex_unlock(&chip->hdmi_ext->custom_edidparse.debugfs_lock);

	return 0;
}

int g0m_soc_hdmi_debugfs_bisttest_init(struct hdmi_chip_t *chip)
{
	struct hdmi_debugfs_item_t *bisttest;

	INIT_LIST_HEAD(&chip->hdmi_ext->custom_bisttest.debugfs_list);
	mutex_init(&chip->hdmi_ext->custom_bisttest.debugfs_lock);

	bisttest = (struct hdmi_debugfs_item_t *)kmalloc(
							sizeof(struct hdmi_debugfs_item_t), fh2m_hal_get_inno_gfp_kernel());
	bisttest->chip = chip;
	bisttest->ent = NULL;
	bisttest->status = HDMI_DEBUGS_STATUS_UNINITED;
	bisttest->data = "bisttest";

	mutex_lock(&chip->hdmi_ext->custom_bisttest.debugfs_lock);
	list_add(&bisttest->list, &chip->hdmi_ext->custom_bisttest.debugfs_list);
	mutex_unlock(&chip->hdmi_ext->custom_bisttest.debugfs_lock);

	return 0;
}

int g0m_soc_hdmi_debugfs_hdmi_status_init(struct hdmi_chip_t *chip)
{
	struct hdmi_debugfs_item_t *hdmi_status;

	INIT_LIST_HEAD(&chip->hdmi_ext->custom_hdmi_status.debugfs_list);
	mutex_init(&chip->hdmi_ext->custom_hdmi_status.debugfs_lock);

	hdmi_status = (struct hdmi_debugfs_item_t *)kmalloc(
							sizeof(struct hdmi_debugfs_item_t), fh2m_hal_get_inno_gfp_kernel());
	hdmi_status->chip = chip;
	hdmi_status->ent = NULL;
	hdmi_status->status = HDMI_DEBUGS_STATUS_UNINITED;
	hdmi_status->data = "inno_status";

	mutex_lock(&chip->hdmi_ext->custom_hdmi_status.debugfs_lock);
	list_add(&hdmi_status->list, &chip->hdmi_ext->custom_hdmi_status.debugfs_list);
	mutex_unlock(&chip->hdmi_ext->custom_hdmi_status.debugfs_lock);

	return 0;
}

int g0m_soc_hdmi_debugfs_hw_test_init(struct hdmi_chip_t *chip)
{
	struct hdmi_debugfs_item_t *hw_test;

	INIT_LIST_HEAD(&chip->hdmi_ext->custom_hw_self_test.debugfs_list);
	mutex_init(&chip->hdmi_ext->custom_hw_self_test.debugfs_lock);

	hw_test = (struct hdmi_debugfs_item_t *)kmalloc(
					sizeof(struct hdmi_debugfs_item_t), fh2m_hal_get_inno_gfp_kernel());
	hw_test->chip = chip;
	hw_test->ent = NULL;
	hw_test->status = HDMI_DEBUGS_STATUS_UNINITED;
	hw_test->data = "hw_self_test";

	mutex_lock(&chip->hdmi_ext->custom_hw_self_test.debugfs_lock);
	list_add(&hw_test->list, &chip->hdmi_ext->custom_hw_self_test.debugfs_list);
	mutex_unlock(&chip->hdmi_ext->custom_hw_self_test.debugfs_lock);

	return 0;
}

void *inno_hdmi_get_chip_adapater(struct hdmi_chip_t *chip)
{
	struct hdmi_device_t *inno_hdmi
		= container_of(chip, struct hdmi_device_t, chip);

	return &inno_hdmi->i2c->adapter;
}

