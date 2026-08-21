/*************************************************************************/ /*!
@File			innodpu_audio_api.c
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

#include "innodpu_audio_api.h"
#include "innodpu_hdmi.h"
#include "innodpu_hdmi_audio_api.h"
#include "innodpu_dp.h"
#include "innodpu_dp_audio_api.h"
#include "inno_debug.h"

#define is_valid_module_support(_x)  (\
		((_x) == REG_M_HDMI)  || \
		((_x) == REG_M_HDMI1) || \
		((_x) == REG_M_DP))


static struct platform_device* innodpu_audio_get_handler(struct dev_rsrc *pdev_rsrc, enum reg_module reg_m)
{
	struct platform_device* pdev = NULL;

	switch (reg_m) {
#if (HAL_MAX_HDMI_NUMS > 0)
	case REG_M_HDMI:
		pdev  = pdev_rsrc->hdmi_dev[0];
		break;
#endif
#if (HAL_MAX_HDMI_NUMS > 1)
	case REG_M_HDMI1:
		pdev  = pdev_rsrc->hdmi_dev[1];
		break;
#endif
#if (HAL_MAX_DP_NUMS > 0)
	case REG_M_DP:
		pdev  = pdev_rsrc->dp_dev[0];
		break;
#endif
	default:
		inno_error("%s: Not support reg_m(%d)\n", __func__, reg_m);
		break;
	}

	return pdev;
}

int fh2m_innodpu_audio_is_support(struct dev_rsrc *pdev_rsrc, enum reg_module reg_m)
{
	int ret = 0;
	struct platform_device* pdev = NULL;

	if (!is_valid_module_support(reg_m) || !pdev_rsrc)
		return -EINVAL;

	pdev = innodpu_audio_get_handler(pdev_rsrc, reg_m);
	if (!pdev) {
		inno_error("%s: Not found pdev handler.\n", __func__);
		return -EINVAL;
	}

	switch (reg_m) {
	case REG_M_HDMI:
	case REG_M_HDMI1:
		ret = fh2m_innodpu_hdmi_audio_is_support(pdev);
		break;
	case REG_M_DP:
		ret = innodpu_dp_audio_is_support(pdev);
		break;
	default:
		return -EINVAL;
	}

	return ret;
}
INNO_EXT_SYM(fh2m_innodpu_audio_is_support);

int fh2m_innodpu_audio_query_display_connected(struct dev_rsrc *pdev_rsrc, enum reg_module reg_m)
{
	int ret = 0;
	struct platform_device* pdev = NULL;

	if (!is_valid_module_support(reg_m) || !pdev_rsrc)
		return -EINVAL;

	pdev = innodpu_audio_get_handler(pdev_rsrc, reg_m);
	if (!pdev) {
		inno_error("%s: Not found pdev handler.\n", __func__);
		return -EINVAL;
	}

	switch (reg_m) {
	case REG_M_HDMI:
	case REG_M_HDMI1:
		ret = fh2m_innodpu_hdmi_audio_query_display_connected(pdev);
		break;
	case REG_M_DP:
		ret = innodpu_dp_audio_query_display_connected(pdev);
		break;
	default:
		return -EINVAL;
	}

	return ret;
}
INNO_EXT_SYM(fh2m_innodpu_audio_query_display_connected);

int fh2m_innodpu_audio_query_display_status(struct dev_rsrc *pdev_rsrc, enum reg_module reg_m)
{
	int ret = 0;
	struct platform_device* pdev = NULL;

	if (!is_valid_module_support(reg_m) || !pdev_rsrc)
		return -EINVAL;

	pdev = innodpu_audio_get_handler(pdev_rsrc, reg_m);
	if (!pdev) {
		inno_error("%s: Not found pdev handler.\n", __func__);
		return -EINVAL;
	}

	switch (reg_m) {
	case REG_M_HDMI:
	case REG_M_HDMI1:
		ret = fh2m_innodpu_hdmi_audio_query_display_status(pdev);
		break;
	case REG_M_DP:
		ret = innodpu_dp_audio_query_display_status(pdev);
		break;
	default:
		return -EINVAL;
	}

	return ret;
}
INNO_EXT_SYM(fh2m_innodpu_audio_query_display_status);

int fh2m_innodpu_audio_get_eld(struct dev_rsrc *pdev_rsrc, enum reg_module reg_m, char *buf, int buf_size)
{
	int ret = 0;
	struct platform_device* pdev = NULL;

	if (!is_valid_module_support(reg_m) || !pdev_rsrc || !buf || buf_size<0)
		return -EINVAL;

	pdev = innodpu_audio_get_handler(pdev_rsrc, reg_m);
	if (!pdev) {
		inno_error("%s: Not found pdev handler.\n", __func__);
		return -EINVAL;
	}

	switch (reg_m) {
	case REG_M_HDMI:
	case REG_M_HDMI1:
		ret = innodpu_hdmi_audio_get_eld(pdev, buf, buf_size);
		break;
	case REG_M_DP:
		ret = innodpu_dp_audio_get_eld(pdev, buf, buf_size);
		break;
	default:
		return -EINVAL;
	}

	return ret;
}
INNO_EXT_SYM(fh2m_innodpu_audio_get_eld);

bool fh2m_innodpu_sound_support(struct dev_rsrc *pdev_rsrc)
{
	struct platform_device* pdev = NULL;
	struct drm_device *dev = NULL;
	struct drm_connector *connector = NULL;
	struct drm_property_blob *edid_blob_ptr = NULL;
	struct drm_connector_list_iter conn_iter;
	enum drm_connector_status status;
	unsigned char *edid;
	size_t size = 0;
	bool enabled = false;
	bool ret = false;

	if (!pdev_rsrc || !pdev_rsrc->drm_dev[0])
		return false;

	pdev = pdev_rsrc->drm_dev[0];
	dev = fh2m_inno_platform_get_drvdata(pdev);
	if (!dev)
		return false;

	mutex_lock(&dev->mode_config.mutex);
	drm_connector_list_iter_begin(dev, &conn_iter);
	drm_for_each_connector_iter(connector, &conn_iter) {
		enabled = READ_ONCE(connector->encoder);
		status = READ_ONCE(connector->status);

		edid_blob_ptr = connector->edid_blob_ptr;
		if (status == connector_status_connected &&
			enabled && edid_blob_ptr &&
			edid_blob_ptr->data) {
			edid = connector->edid_blob_ptr->data;
			size = connector->edid_blob_ptr->length;
			if ((size > (INNO_EDID_BUF_LEN / 2)) &&
				drm_detect_monitor_audio((struct edid *)edid)) {
				ret = true;
				break;
			}
		}
	}
	drm_connector_list_iter_end(&conn_iter);
	mutex_unlock(&dev->mode_config.mutex);

	return ret;
}
INNO_EXT_SYM(fh2m_innodpu_sound_support);
