#ifndef __DRM_PANEL_BACKLIHGT_H__
#define __DRM_PANEL_BACKLIHGT_H__

#include <linux/version.h>
#include <linux/backlight.h>
#include "inno_drm_version.h"
#if (DRM_VERSION >= KERNEL_VERSION(5, 19, 0))
#include <drm/display/drm_dp_helper.h>
#else
#include <drm/drm_dp_helper.h>
#endif
#include "innodpu_common_drm_panel.h"
#include "innodpu_dp_common.h"

int inno_panel_backlight_init(struct inno_panel *panel, inno_dev *pdev,
							  struct drm_dp_aux *aux, u8 backlight_mode);
#if (DRM_VERSION < KERNEL_VERSION(4, 13, 0))
# define DP_EDP_PWMGEN_BIT_COUNT_MASK       (0x1f << 0)
#endif

#if (DRM_VERSION < KERNEL_VERSION(4, 17, 0))
/**
 * backlight_enable - Enable backlight
 * @bd: the backlight device to enable
 */
static inline int backlight_enable(struct backlight_device *bd)
{
	if (!bd)
		return 0;

	bd->props.power = FB_BLANK_UNBLANK;
	bd->props.fb_blank = FB_BLANK_UNBLANK;
	bd->props.state &= ~BL_CORE_FBBLANK;

	return backlight_update_status(bd);
}

/**
 * backlight_disable - Disable backlight
 * @bd: the backlight device to disable
 */
static inline int backlight_disable(struct backlight_device *bd)
{
	if (!bd)
		return 0;

	bd->props.power = FB_BLANK_POWERDOWN;
	bd->props.fb_blank = FB_BLANK_POWERDOWN;
	bd->props.state |= BL_CORE_FBBLANK;

	return backlight_update_status(bd);
}
#endif

#endif
