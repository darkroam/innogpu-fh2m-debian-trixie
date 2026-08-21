/*************************************************************************/ /*!
@File			innodpu_panel_pwr.c
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

#include <linux/debugfs.h>
#include <linux/mutex.h>
#include <video/display_timing.h>
#include <video/of_display_timing.h>
#include <video/videomode.h>
#include <drm/drm_edid.h>
#include "innodpu_panel_pwr.h"
#include "innodpu_panel_backlight.h"
#include "innodpu_common.h"
#include "innodpu_connector.h"
#include "inno_lock.h"
#include "inno_timer.h"
#include "inno_debug.h"

struct panel_dev {
	struct inno_panel base;
	const struct panel_desc *desc;

	/* __maybe_unused */
	struct dentry *debugfs;

	struct mutex gpio_mutex;

	const struct edp_panel_entry *detected_panel;
	enum reg_entity pwr_mode_reg;  /* input or ouput mode */
	enum reg_entity pwr_multi_reg; /* input or ouput mode */
	enum reg_entity pwr_output_reg; /* ouput reg */
	enum reg_module pwr_reg_type;

	enum reg_entity bl_en_mode_reg;  /* input or ouput mode */
	enum reg_entity bl_en_multi_reg; /* multi mode */
	enum reg_entity bl_en_output_reg; /* ouput reg */
	enum reg_module bl_en_reg_type;

	u32  pwr_mask;         /* mask */
	u32  pwr_gpio_index;       /* index */
	u32  bl_en_mask;       /* mask */
	u32  bl_gpio_index;

	inno_dev *parent;

	bool is_R1;
};

/**
 * struct panel_desc - Describes a simple panel.
 */
struct panel_desc {
	/**
	 * @modes: Pointer to array of fixed modes appropriate for this panel.
	 *
	 * If only one mode then this can just be the address of the mode.
	 * NOTE: cannot be used with "timings" and also if this is specified
	 * then you cannot override the mode in the device tree.
	 */
	const struct drm_display_mode *modes;

	/** @num_modes: Number of elements in modes array. */
	unsigned int num_modes;

	/**
	 * @timings: Pointer to array of display timings
	 *
	 * NOTE: cannot be used with "modes" and also these will be used to
	 * validate a device tree override if one is present.
	 */
	const struct display_timing *timings;

	/** @num_timings: Number of elements in timings array. */
	unsigned int num_timings;

	/** @bpc: Bits per color. */
	unsigned int bpc;

	/** @size: Structure containing the physical size of this panel. */
	struct {
		/**
		 * @size.width: Width (in mm) of the active display area.
		 */
		unsigned int width;

		/**
		 * @size.height: Height (in mm) of the active display area.
		 */
		unsigned int height;
	} size;

};

static inline struct panel_dev *to_panel_dev(struct inno_panel *panel)
{
	return container_of(panel, struct panel_dev, base);
}

static int inline panel_reg_write32(struct inno_panel *panel, enum reg_module reg_type,
								 enum reg_entity entity, uint32_t val)
{
	struct panel_dev *p = to_panel_dev(panel);

	if (!p) {
		return -1;
	}

	if (reg_type != REG_M_BL_GPIO) {
		return -1;
	}

	WARN_ON(!p->parent);
	fh2m_hal_reg_write32(p->parent, reg_type, entity, val);

	return 0;
}

static int inline panel_reg_read32(struct inno_panel *panel, enum reg_module reg_type,
								 enum reg_entity entity, uint32_t *val)
{
	struct panel_dev *p = to_panel_dev(panel);

	if (!p) {
		return -1;
	}

	if (reg_type != REG_M_BL_GPIO) {
		return -1;
	}

	WARN_ON(!p->parent);
	fh2m_hal_reg_read32(p->parent, reg_type, entity, val);

	return 0;
}

static int inline panel_reg_update_bits(struct inno_panel *panel, enum reg_module reg_type,
								 enum reg_entity entity, uint32_t mask, uint32_t val)
{
	struct panel_dev *p = to_panel_dev(panel);
	u32 value = 0;
	u32 tmp   = 0;

	if (!p) {
		return -1;
	}

	panel_reg_read32(panel, reg_type, entity, &value);
	tmp = value & ~mask;
	tmp |= val & mask;
	panel_reg_write32(panel, reg_type, entity, tmp);

	return 0;
}

static bool is_panel_pwr_high(struct inno_panel *panel)
{
	struct panel_dev *p = to_panel_dev(panel);
	u32 value = 0;

	if (!p) {
		return false;
	}

	fh2m_inno_mutex_lock(&p->gpio_mutex);
	panel_reg_read32(panel, p->pwr_reg_type, p->pwr_output_reg, &value);
	fh2m_inno_mutex_unlock(&p->gpio_mutex);

	return (value & p->pwr_mask) ? true : false;
}

static void panel_bl_en_gpio_init(struct inno_panel *panel)
{
	struct panel_dev *p = to_panel_dev(panel);
	u32 multi_value = 0;
	u32 mode_value = 0;

	if (!p) {
		return ;
	}

	fh2m_inno_mutex_lock(&p->gpio_mutex);

	panel_reg_read32(panel, p->bl_en_reg_type, p->bl_en_multi_reg, &multi_value);
	panel_reg_read32(panel, p->bl_en_reg_type, p->bl_en_mode_reg, &mode_value);

	if (multi_value == 0x2 && (mode_value & p->bl_en_mask)) {
		DRM_DEBUG_KMS("Skiping panel already bl_en reg init config\n");
		goto end;
	}

	/* 1.default gpio high */
	panel_reg_update_bits(panel, p->bl_en_reg_type, p->bl_en_output_reg,
						  p->bl_en_mask, 0x1 << p->bl_gpio_index);
	/* 2.set backlight gpio multi is gpio */
	panel_reg_write32(panel, p->bl_en_reg_type, p->bl_en_multi_reg, 0x2);
	/* 3.set backlight gpio mode  is ouput */
	panel_reg_update_bits(panel, p->bl_en_reg_type, p->bl_en_mode_reg,
						  p->bl_en_mask, 0x1 << p->bl_gpio_index);

end:
	fh2m_inno_mutex_unlock(&p->gpio_mutex);
}

static void panel_pwr_gpio_init(struct inno_panel *panel)
{
	struct panel_dev *p = to_panel_dev(panel);
	u32 multi_value = 0;
	u32 mode_value = 0;

	if (!p) {
		return ;
	}

	fh2m_inno_mutex_lock(&p->gpio_mutex);

	panel_reg_read32(panel, p->pwr_reg_type, p->pwr_multi_reg, &multi_value);
	panel_reg_read32(panel, p->pwr_reg_type, p->pwr_mode_reg, &mode_value);

	if (multi_value == 0x2 && (mode_value & p->pwr_mask)) {
		DRM_DEBUG_KMS("Skiping panel already pwr reg init config\n");
		goto end;
	}

	/* 1.default gpio high */
	panel_reg_update_bits(panel, p->pwr_reg_type, p->pwr_output_reg,
								p->pwr_mask, 0x1 << p->pwr_gpio_index);
	/* 2.set backlight gpio multi is gpio */
	panel_reg_write32(panel, p->pwr_reg_type, p->pwr_multi_reg, 0x2);
	/* 3.set backlight gpio mode  is ouput */
	panel_reg_update_bits(panel, p->pwr_reg_type, p->pwr_mode_reg,
								p->pwr_mask, 0x1 << p->pwr_gpio_index);

	/* gpio init msleep 200 wait hpd and aux ready */
	if (p->is_R1) {
		fh2m_inno_udelay(200000);
	} else {
		fh2m_inno_msleep(200);
	}

end:
	fh2m_inno_mutex_unlock(&p->gpio_mutex);
}

static void panel_set_pwr_state(struct inno_panel *panel, bool state)
{
	struct panel_dev *p = to_panel_dev(panel);

	if (!p) {
		return ;
	}

	panel_pwr_gpio_init(&p->base);
	if (state) {
		if (!is_panel_pwr_high(panel)) {

			fh2m_inno_mutex_lock(&p->gpio_mutex);
			panel_reg_update_bits(&p->base, p->pwr_reg_type, p->pwr_output_reg,
								  p->pwr_mask, 0x1 << p->pwr_gpio_index);
			fh2m_inno_mutex_unlock(&p->gpio_mutex);

			/* msleep 200 wait hpd and aux ready */
			if (p->is_R1) {
				fh2m_inno_udelay(200000);
			} else {
				fh2m_inno_msleep(200);
			}
		} else {
			DRM_DEBUG_KMS("Skiping panel already pwr reg config(current high)\n");
		}
	} else {
		if (is_panel_pwr_high(panel)) {
			/* turn off panel pwr */
			if (p->is_R1) {
				fh2m_inno_udelay(500);
			} else {
				usleep_range(500, 500);
			}
			mb();
			fh2m_inno_mutex_lock(&p->gpio_mutex);
			panel_reg_update_bits(&p->base, p->pwr_reg_type, p->pwr_output_reg,
								  p->pwr_mask, 0x0 << p->pwr_gpio_index);
			fh2m_inno_mutex_unlock(&p->gpio_mutex);
			/* pwr down,then msleep */
			mb();
			if (p->is_R1) {
				fh2m_inno_udelay(500000);
			} else {
				fh2m_inno_msleep(500);
			}
		} else {
			DRM_DEBUG_KMS("Skiping panel already pwr reg config(current low)\n");
		}
	}

	DRM_DEBUG_KMS("panel pwr state is %s\n", state ? "ON" : "OFF");
}

static bool is_panel_bl_en_high(struct inno_panel *panel)
{
	struct panel_dev *p = to_panel_dev(panel);
	u32 value = 0;

	if (!p) {
		return -1;
	}

	fh2m_inno_mutex_lock(&p->gpio_mutex);
	panel_reg_read32(panel, p->bl_en_reg_type, p->bl_en_output_reg, &value);
	fh2m_inno_mutex_unlock(&p->gpio_mutex);

	return (value & p->bl_en_mask) ? true : false;
}

void panel_set_bl_en_state(struct inno_panel *panel, bool state)
{
	struct panel_dev *p = to_panel_dev(panel);

	if (!p) {
		return ;
	}

	panel_bl_en_gpio_init(&p->base);
	if (state) {
		if (!is_panel_bl_en_high(panel)) {
			/* turn on panel pwr */
			fh2m_inno_mutex_lock(&p->gpio_mutex);

			panel_reg_update_bits(panel, p->bl_en_reg_type, p->bl_en_output_reg,
								  p->bl_en_mask, 0x1 << p->bl_gpio_index);
			fh2m_inno_mutex_unlock(&p->gpio_mutex);
		} else {
			DRM_DEBUG_KMS("Skiping panel already bl_en reg config(current high)\n");
		}
	} else {
		if (is_panel_bl_en_high(panel)) {
			/* turn off panel pwr */
			fh2m_inno_mutex_lock(&p->gpio_mutex);
			panel_reg_update_bits(panel, p->bl_en_reg_type, p->bl_en_output_reg,
								  p->bl_en_mask, 0x0 << p->bl_gpio_index);
			fh2m_inno_mutex_unlock(&p->gpio_mutex);
		} else {
			DRM_DEBUG_KMS("Skiping panel already bl_en reg config(current low)\n");
		}
	}

	DRM_DEBUG_KMS("panel bl_en state is %s\n", state ? "ON" : "OFF");
}

static int panel_pwr_unprepare(struct inno_panel *panel)
{
	/* turn off panel pwr */
	panel_set_pwr_state(panel, false);

	return 0;
}

static int panel_pwr_prepare(struct inno_panel *panel)
{
	/* turn on panel pwr */
	panel_set_pwr_state(panel, true);

	return 0;
}

static int panel_bl_en_enable(struct inno_panel *panel)
{
	/* turn on panel bl_en */
	panel_set_bl_en_state(panel, true);

	return 0;
}

static int panel_bl_en_disable(struct inno_panel *panel)
{
	/* turn off panel bl_en */
	panel_set_bl_en_state(panel, false);

	return 0;
}

static int panel_get_non_edid_modes(struct panel_dev *panel,
					struct drm_connector *connector)
{
	return 0;
}

static int panel_get_modes(struct inno_panel *panel,
			       struct drm_connector *connector)
{
	struct panel_dev *p = to_panel_dev(panel);
	int num = 0;

	if (!p->desc)
		return num;
	/*
	 * Add hard-coded panel modes. Don't call this if there are no timings
	 * and no modes (the generic panel-panel case) because it will clobber
	 * the display_info that was already set by drm_add_edid_modes().
	 */
	if (p->desc->num_timings || p->desc->num_modes)
		num += panel_get_non_edid_modes(p, connector);
	else if (!num)
		dev_warn(p->base.dev, "No display modes\n");

	return num;
}

static int panel_get_timings(struct inno_panel *panel,
				 unsigned int num_timings,
				 struct display_timing *timings)
{
	struct panel_dev *p = to_panel_dev(panel);
	unsigned int i;

	if (!p->desc)
		return 0;

	if (p->desc->num_timings < num_timings)
		num_timings = p->desc->num_timings;

	if (timings)
		for (i = 0; i < num_timings; i++)
			timings[i] = p->desc->timings[i];

	return p->desc->num_timings;
}

#if 0
static int detected_panel_show(struct seq_file *s, void *data)
{
	struct inno_panel *panel = s->private;
	struct panel_dev *p = to_panel_dev(panel);

	if (IS_ERR(p->detected_panel))
		seq_puts(s, "UNKNOWN\n");
	else if (!p->detected_panel)
		seq_puts(s, "HARDCODED\n");
	else
		seq_printf(s, "panel_debugfs\n");

	return 0;
}

DEFINE_SHOW_ATTRIBUTE(detected_panel);

/* TODO not used */
static void panel_debugfs_init(struct inno_panel *panel, struct dentry *root)
{
	struct panel_dev *p = to_panel_dev(panel);

	p->debugfs = debugfs_create_file("detected_panel", 0600, root, panel, &detected_panel_fops);
}

/* TODO not used */
static void panel_debugfs_fini(struct inno_panel *panel)
{
	struct panel_dev *p = to_panel_dev(panel);

	debugfs_remove(p->debugfs);
	p->debugfs = NULL;
}
#endif

static const struct inno_panel_funcs panel_pwr_funcs = {
	.unprepare = panel_pwr_unprepare,
	.prepare = panel_pwr_prepare,
	.enable  = panel_bl_en_enable,
	.disable = panel_bl_en_disable,
	.get_modes = panel_get_modes,
	.get_timings = panel_get_timings,
	/* .debugfs_init = panel_debugfs_init, */
};

static __maybe_unused struct mutex* panel_get_gpio_mutex(struct inno_panel *panel)
{
	struct panel_dev *p = to_panel_dev(panel);

	return &p->gpio_mutex;
}

struct inno_panel* panel_pwr_create(struct drm_device *drm_dev,
										  inno_dev *dev,
										  enum reg_module reg_module,
										  struct drm_dp_aux *aux,
										  struct drm_connector *connector)
{
	chip_type_e plat;
	inno_dev *pdev = NULL;
	struct panel_dev *panel;
	int err;
	u8 backlight_mode = 0;

	if (!dev)
		return NULL;

	if (!drm_dev || !drm_dev->dev) {
		dev_warn(dev, "invalid drm_dev\n");
		return NULL;
	}

	pdev = fh2m_inno_dev_get_parent(dev);
	if (!pdev) {
		dev_warn(drm_dev->dev, "invalid parent dev\n");
		return NULL;
	}

	panel = devm_kzalloc(drm_dev->dev, sizeof(*panel), fh2m_hal_get_inno_gfp_kernel());
	if (!panel) {
		dev_warn(drm_dev->dev, "panel no memory\n");
		return NULL;
	}

	inno_panel_init(&panel->base, drm_dev->dev, &panel_pwr_funcs, connector->connector_type);

	fh2m_inno_mutex_init(&panel->gpio_mutex);

	panel->parent = pdev;
	plat = fh2m_hal_get_chiptype(panel->parent);
	switch(plat) {
	case CHIP_G1_SOC:
	case CHIP_G1P_SOC:
		dev_warn(drm_dev->dev, "current only support g0m/g0c platform.\n");
		return NULL;
	case CHIP_G0_SOC:
	case CHIP_G0M_SOC:
		panel->pwr_gpio_index = 0; /* pwr by hw */
		panel->pwr_mask = 0x1 << panel->pwr_gpio_index; /* mask */
		panel->pwr_mode_reg   = REG_ENTITY0000;     /* input or ouput mode */
		panel->pwr_multi_reg  = REG_ENTITY0001;     /* multi mode */
		panel->pwr_output_reg = REG_ENTITY0002;     /* ouput reg */
		panel->pwr_reg_type   = REG_M_BL_GPIO;     /* reg type */

		panel->bl_gpio_index  = 1; /* bl_en by hw */
		panel->bl_en_mask = 0x1 << panel->bl_gpio_index;    /* mask */
		panel->bl_en_mode_reg = REG_ENTITY0000;       /* input or ouput mode */
		panel->bl_en_multi_reg  = REG_ENTITY0001;     /* multi mode */
		panel->bl_en_output_reg = REG_ENTITY0002;     /* ouput reg */
		panel->bl_en_reg_type   = REG_M_BL_GPIO;     /* reg type */
		break;
	default:
		dev_warn(drm_dev->dev, "does not currently support platform.\n");
		return NULL;
	}

	{ // special handling for R1
		struct hw_board_info board = {"R1", "*"};
		panel->is_R1 = innodpu_is_odm_pcb_match(dev, &board);
	}

	backlight_mode = innodpu_get_connector_backlight_mode(dev, reg_module);
	err = inno_panel_backlight_init(&panel->base, panel->parent, aux, backlight_mode);
	if (err) {
		dev_warn(drm_dev->dev, "panel has not backlight device\n");
	}

	inno_panel_add(&panel->base);

	return &panel->base;
}

void panel_pwr_destory(struct inno_panel *panel)
{
	struct panel_dev *p = to_panel_dev(panel);

	if (!p)
		return ;

	/* backlight destroy by devres */
	inno_panel_remove(&p->base);
	inno_panel_disable(&p->base);
	inno_panel_unprepare(&p->base);
}
