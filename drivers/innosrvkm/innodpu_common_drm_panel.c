/*************************************************************************/ /*!
@File			innodpu_common_drm_panel.c
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

#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/module.h>
#include "innodpu_common.h"
#include "innodpu_compatibility.h"
#include "innodpu_common_drm_panel.h"
#include "innodpu_panel_backlight.h"

static DEFINE_MUTEX(panel_lock);
static LIST_HEAD(panel_list);

/**
 * DOC: drm panel
 *
 * The DRM panel helpers allow drivers to register panel objects with a
 * central registry and provide functions to retrieve those panels in display
 * drivers.
 *
 * For easy integration into drivers using the &drm_bridge infrastructure please
 * take look at inno_panel_bridge_add() and devm_drm_panel_bridge_add().
 */

/**
 * inno_panel_init - initialize a panel
 * @panel: DRM panel
 * @dev: parent device of the panel
 * @funcs: panel operations
 * @connector_type: the connector type (DRM_MODE_CONNECTOR_*) corresponding to
 *	the panel interface
 *
 * Initialize the panel structure for subsequent registration with
 * inno_panel_add().
 */
void inno_panel_init(struct inno_panel *panel, struct device *dev,
		    const struct inno_panel_funcs *funcs, int connector_type)
{
	INIT_LIST_HEAD(&panel->list);
	panel->dev = dev;
	panel->funcs = funcs;
	panel->connector_type = connector_type;
}

/**
 * inno_panel_add - add a panel to the global registry
 * @panel: panel to add
 *
 * Add a panel to the global registry so that it can be looked up by display
 * drivers.
 */
void inno_panel_add(struct inno_panel *panel)
{
	mutex_lock(&panel_lock);
	list_add_tail(&panel->list, &panel_list);
	mutex_unlock(&panel_lock);
}

/**
 * inno_panel_remove - remove a panel from the global registry
 * @panel: DRM panel
 *
 * Removes a panel from the global registry.
 */
void inno_panel_remove(struct inno_panel *panel)
{
	mutex_lock(&panel_lock);
	list_del_init(&panel->list);
	mutex_unlock(&panel_lock);
}

/**
 * inno_panel_prepare - power on a panel
 * @panel: DRM panel
 *
 * Calling this function will enable power and deassert any reset signals to
 * the panel. After this has completed it is possible to communicate with any
 * integrated circuitry via a command bus.
 *
 * Return: 0 on success or a negative error code on failure.
 */
int inno_panel_prepare(struct inno_panel *panel)
{
	int ret;

	if (!panel)
		return -EOPNOTSUPP;

	/* turn on edp power */
	if (panel->funcs && panel->funcs->prepare) {
		ret = panel->funcs->prepare(panel);
	}

	return 0;
}

/**
 * inno_panel_unprepare - power off a panel
 * @panel: DRM panel
 *
 * Calling this function will completely power off a panel (assert the panel's
 * reset, turn off power supplies, ...). After this function has completed, it
 * is usually no longer possible to communicate with the panel until another
 * call to inno_panel_prepare().
 *
 * Return: 0 on success or a negative error code on failure.
 */
int inno_panel_unprepare(struct inno_panel *panel)
{
	int ret;

	if (!panel)
		return -EOPNOTSUPP;

	/* turn off edp power */
	if (panel->funcs && panel->funcs->unprepare) {
		ret = panel->funcs->unprepare(panel);
	}

	return 0;
}

/**
 * inno_panel_enable - enable a panel
 * @panel: DRM panel
 *
 * Calling this function will cause the panel display drivers to be turned on
 * and the backlight to be enabled. Content will be visible on screen after
 * this call completes.
 *
 * Return: 0 on success or a negative error code on failure.
 */
int inno_panel_enable(struct inno_panel *panel)
{
	int ret;

	if (!panel)
		return -EOPNOTSUPP;

	ret = backlight_enable(panel->backlight);
	if (ret < 0)
		DRM_DEBUG_KMS("failed to enable backlight: %d\n",
			     ret);

	if (panel->funcs && panel->funcs->enable) {
		ret = panel->funcs->enable(panel);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/**
 * inno_panel_disable - disable a panel
 * @panel: DRM panel
 *
 * This will typically turn off the panel's backlight or disable the display
 * drivers. For smart panels it should still be possible to communicate with
 * the integrated circuitry via any command bus after this call.
 *
 * Return: 0 on success or a negative error code on failure.
 */
int inno_panel_disable(struct inno_panel *panel)
{
	int ret;

	if (!panel)
		return -EOPNOTSUPP;

	if (panel->funcs && panel->funcs->disable) {
		ret = panel->funcs->disable(panel);
		if (ret < 0)
			return ret;
	}

	ret = backlight_disable(panel->backlight);
	if (ret < 0)
		DRM_DEBUG_KMS("failed to disable backlight: %d\n",
			     ret);

	return 0;
}

/**
 * inno_panel_get_modes - probe the available display modes of a panel
 * @panel: DRM panel
 * @connector: DRM connector
 *
 * The modes probed from the panel are automatically added to the connector
 * that the panel is attached to.
 *
 * Return: The number of modes available from the panel on success or a
 * negative error code on failure.
 */
int inno_panel_get_modes(struct inno_panel *panel,
			struct drm_connector *connector)
{
	if (!panel)
		return -EOPNOTSUPP;

	if (panel->funcs && panel->funcs->get_modes)
		return panel->funcs->get_modes(panel, connector);

	return -EOPNOTSUPP;
}

/**
 * @inno_panel_get_timings:
 *
 * Copy display timings into the provided array and return
 * the number of display timings available.
 *
 * This function is optional.
 */
int inno_panel_get_timings(struct inno_panel *panel, unsigned int num_timings,
		   struct display_timing *timings)
{
	if (!panel)
		return -EOPNOTSUPP;

	if (panel->funcs && panel->funcs->get_timings)
		return panel->funcs->get_timings(panel, num_timings, timings);

	return -EOPNOTSUPP;
}
