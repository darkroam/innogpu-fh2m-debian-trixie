#ifndef __DRM_PANEL_EDP_H__
#define __DRM_PANEL_EDP_H__

#include "inno_drm_version.h"
#if (DRM_VERSION >= KERNEL_VERSION(5, 19, 0))
#include <drm/display/drm_dp_helper.h>
#else
#include <drm/drm_dp_helper.h>
#endif
#if (DRM_VERSION >= KERNEL_VERSION(5, 5, 0))
#include <drm/drm_device.h>
#else
#include <drm/drmP.h>
#endif
#include "hal_interface.h"
#include "innodpu_common.h"
#include "innodpu_compatibility.h"
#include "innodpu_dp_common.h"
#include "innodpu_common_drm_panel.h"

struct inno_panel* panel_pwr_create(struct drm_device *drm_dev,
										  inno_dev *dev,
										  enum reg_module reg_module,
										  struct drm_dp_aux *aux,
										  struct drm_connector *connector);
void panel_pwr_destory(struct inno_panel *panel);
void panel_set_bl_en_state(struct inno_panel *panel, bool state);

#endif
