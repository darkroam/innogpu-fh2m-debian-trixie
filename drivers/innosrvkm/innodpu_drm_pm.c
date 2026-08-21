/*************************************************************************/ /*!
@File			innodpu_drm_pm.c
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
#include "innodpu_drm_pm.h"
#include "inno_timer.h"
#include "inno_misc.h"
#include "innogpu_drm.h"

static int innodpu_get_logo_status(struct drm_device *drm_dev,
		struct innodpu_drm_private *dev_priv)
{
	if (!drm_dev || !dev_priv)
		return -EFAULT;

	return dev_priv->logo_execute_status;
}

static void innodpu_set_logo_status(struct drm_device *drm_dev,
		struct innodpu_drm_private *dev_priv, int status)
{
	if (!drm_dev || !dev_priv)
		return;

	dev_priv->logo_execute_status = status;
}

static void innodpu_logo_timedelay_dis(struct drm_device *drm_dev,
		struct innodpu_drm_private *dev_priv)
{
	u64 extime_us = s_logo_extime * 1000 * 1000;
	inno_ktime time = 0;
	long long sleep_us = 0;

	if (!drm_dev || !dev_priv)
		return;

#ifdef PM_VT_SWITCH_DISABLE
	if (dev_priv->logo_end_ktime > 0) {
		time = fh2m_inno_ktime_get();
		sleep_us = fh2m_inno_ktime_to_us(dev_priv->logo_end_ktime) - fh2m_inno_ktime_to_us(time);

		inno_drm_info(drm_dev->dev, "sleep us:%lld logo_end_ktime:%lld time_cur:%lld\n",
					  sleep_us, dev_priv->logo_end_ktime, time);

		dev_priv->logo_end_ktime = 0;
		if (sleep_us > 0 && sleep_us <= extime_us) {
			fh2m_inno_usleep_range(sleep_us, sleep_us + 100);
		} else {
			inno_drm_info(drm_dev->dev, "no need to hibernation\n");
		}

		innodpu_set_logo_status(drm_dev, dev_priv, LOGO_SWITCH_COMPLETE);
	}
#endif

}

void innodpu_poll_logo_execute(struct drm_device *drm_dev,
		struct innodpu_drm_private *dev_priv, int nr)
{
	u64 extime_us = s_logo_extime * 1000 * 1000;

	if (!drm_dev || !dev_priv)
		return;

	if (fh2m_inno_strncmp(current->comm, "plymouthd", 9) == 0 && s_logo_extime > 0) {

		mutex_lock(&dev_priv->logo_lock);

		switch (nr) {
		case DRM_IOCTL_NR(DRM_IOCTL_MODE_SETCRTC):
		{
			dev_priv->logo_end_ktime = fh2m_inno_ktime_add_us(fh2m_inno_ktime_get(), extime_us);
			inno_drm_info(drm_dev->dev, "setcrtc:logo_end_ktime:%lld\n", dev_priv->logo_end_ktime);
			break;
		}
		case DRM_IOCTL_NR(DRM_IOCTL_DROP_MASTER):
		{
			inno_drm_info(drm_dev->dev, "drop_master\n");
			if (innodpu_get_logo_status(drm_dev, dev_priv) == LOGO_SWITCH_POWERUP) {
				inno_drm_info(drm_dev->dev, "time delayed display(logo-powerup)\n");
				innodpu_logo_timedelay_dis(drm_dev, dev_priv);
			}
			break;
		}
		default:
		break;
		}

		mutex_unlock(&dev_priv->logo_lock);
	}
}

static int innodpu_drm_hibernation(struct device *dev)
{
	struct drm_device *drm_dev = dev_get_drvdata(dev);
	struct innodpu_drm_private *dev_priv = NULL;
	struct inno_fbdev *fbdev = NULL;

	if (drm_dev)
		dev_priv = innogpu_drm_to_display_private(drm_dev);

	if (!dev_priv) {
		inno_drm_err(dev, "dev priv is NULL\n");
		return -EINVAL;
	}
	fbdev = dev_priv->fbdev;

	/*
	 * BUG 10367:Lcd light failure under low battery
	 * 	When the power level is lower than %1,the system will enter s4 hibernation,
	 * and after a4 executes swapwrite, it will trigger s3 again, resulting in s3 waking up
	 * with the saved display state of undisplayed.*/
	if (drm_dev->switch_power_state == DRM_SWITCH_POWER_OFF &&
		dev_priv->pm_state) {
		fh2m_innodpu_err(dev, "drm has performed a screen shutdown\n");
		return 0;
	}

	inno_drm_info(dev, "%s\n", __func__);

	drm_kms_helper_poll_disable(drm_dev);
	drm_fb_helper_set_suspend_unlocked(&fbdev->helper, true);

	dev_priv->pm_state = drm_atomic_helper_suspend(drm_dev);
	if (IS_ERR_OR_NULL(dev_priv->pm_state)) {
		drm_fb_helper_set_suspend_unlocked(&fbdev->helper, false);
		drm_kms_helper_poll_enable(drm_dev);
		inno_drm_err(dev, "dpu_pm_suspend Empty pointer2");
		return PTR_ERR(dev_priv->pm_state);
	}

	drm_dev->switch_power_state = DRM_SWITCH_POWER_OFF;

	return 0;
}

static int innodpu_drm_wakeup(struct device *dev)
{
	struct drm_device *drm_dev = dev_get_drvdata(dev);
	struct innodpu_drm_private *dev_priv = NULL;
	struct inno_fbdev *fbdev = NULL;
	int ret = 0;

	if (drm_dev)
		dev_priv = innogpu_drm_to_display_private(drm_dev);
	else
		return -EINVAL;

	if (!dev_priv) {
		inno_drm_err(drm_dev->dev, "dev priv is NULL");
		return -EINVAL;
	}
	fbdev = dev_priv->fbdev;

	if (drm_dev->switch_power_state == DRM_SWITCH_POWER_ON &&
		!dev_priv->pm_state) {
		fh2m_innodpu_err(dev, "drm has performed an open screen operaion\n");
		return 0;
	}

	inno_drm_info(dev, "drm wakeup...\n", __func__);

	drm_dev->switch_power_state = DRM_SWITCH_POWER_ON;

	if (!IS_ERR_OR_NULL(dev_priv->pm_state)) {
		ret = drm_atomic_helper_resume(drm_dev, dev_priv->pm_state);
		if (ret) {
			inno_drm_err(dev, "innodpu_drm wake failed");
		}
	}
	dev_priv->pm_state = NULL;

	drm_fb_helper_set_suspend_unlocked(&fbdev->helper, false);
	drm_kms_helper_poll_enable(drm_dev);

	return ret;
}

static void innodpu_drm_gem_backup(struct device *dev)
{
	struct drm_device *drm_dev = dev_get_drvdata(dev);
	struct innodpu_drm_private *dev_priv = NULL;

	if (drm_dev)
		dev_priv = innogpu_drm_to_display_private(drm_dev);
	else
		return;

	if (!dev_priv) {
		fh2m_innodpu_err(drm_dev->dev, "dev priv is NULL\n");
		return;
	}

	if (dev_priv->has_inv_mem)
		innodpu_gem_backup(dev_priv->invisible_mem_manager);
	innodpu_gem_backup(dev_priv->visible_mem_manager);

	if (dev_priv->has_shared_mem) {
		if (dev_priv->shared_vram_info.current_user)
			innodpu_gem_backup(dev_priv->shared_vram_info.current_user->mem_manager);
	}
}

static void innodpu_drm_gem_recover(struct device *dev)
{
	struct drm_device *drm_dev = dev_get_drvdata(dev);
	struct innodpu_drm_private *dev_priv = NULL;

	if (drm_dev)
		dev_priv = innogpu_drm_to_display_private(drm_dev);
	else
		return;

	if (!dev_priv) {
		fh2m_innodpu_err(drm_dev->dev, "dev priv is NULL\n");
		return;
	}

	if (dev_priv->has_inv_mem)
		innodpu_gem_recover(dev_priv->invisible_mem_manager);
	innodpu_gem_recover(dev_priv->visible_mem_manager);
	innodpu_gem_zero_vram_recover(drm_dev, dev_priv->zero_gem);

	if (dev_priv->has_shared_mem) {
		if (dev_priv->shared_vram_info.current_user)
			innodpu_gem_recover(dev_priv->shared_vram_info.current_user->mem_manager);
	}
}

static int innodpu_drm_suspend(struct device *dev)
{
	int ret = 0;
	struct drm_device *drm_dev = dev_get_drvdata(dev);
	struct drm_crtc *crtc = NULL;

	inno_drm_info(dev, "drm suspend...");

	drm_for_each_crtc(crtc, drm_dev) {
		innodpu_pdp0_backup(crtc);
	}

	ret = innodpu_drm_hibernation(dev);
	innodpu_drm_gem_backup(dev);

	return ret;
}

static int innodpu_drm_resume(struct device *dev)
{
	int ret = 0;
	struct drm_device *drm_dev = dev_get_drvdata(dev);
	struct drm_crtc *crtc = NULL;

	inno_drm_info(dev, "drm resume...");

	innodpu_drm_gem_recover(dev);
	ret = innodpu_drm_wakeup(dev);

	drm_for_each_crtc(crtc, drm_dev) {
		innodpu_pdp0_wakeup(crtc);
	}

	return ret;
}

/*
 *
 */
int innodpu_pm_notifier(struct notifier_block *nb,
		unsigned long val,void *ptr)
{
	struct innodpu_drm_private *dev_priv =
		container_of(nb, struct innodpu_drm_private, pm_nb);
	struct drm_device *drm_dev = dev_priv->drm_dev;
	struct device *dev = dev_priv->dev;
	int ret = 0;

	switch (val) {
	/*	The system is going to restore a hibernation image.  If all goes well,
		the restored image kernel will issue a ``PM_POST_HIBERNATION``
		notification.*/
	case PM_RESTORE_PREPARE:
		inno_drm_info(drm_dev->dev, "PM_RESTORE_PREPARE\n");
		mutex_lock(&dev_priv->logo_lock);
		innodpu_set_logo_status(dev_priv->drm_dev, dev_priv, LOGO_SWITCH_HIBERNATION);
		mutex_unlock(&dev_priv->logo_lock);
		break;
	/* 	The system is going to hibernate, tasks will be frozen immediately.*/
	case PM_HIBERNATION_PREPARE:
		inno_drm_info(drm_dev->dev, "PM_HIBERNATION_PREPARE\n");
		if (dev_priv->drm_dev->switch_power_state == DRM_SWITCH_POWER_ON) {
			ret = innodpu_drm_hibernation(dev);
		}
		break;
	/*	The system memory state has been restored from a hibernation image
		or make hiberantion image save failed */
	case PM_POST_HIBERNATION:
	/*	An error occurred during restore from hibernation.  Device restore
		callbacks have been executed and tasks have been thawed.*/
	case PM_POST_RESTORE:
		inno_drm_info(dev, "PM_POST_HIBERNATION|PM_POST_RESTORE");
		if (dev_priv->drm_dev->switch_power_state == DRM_SWITCH_POWER_OFF)
			ret = innodpu_drm_wakeup(dev);
		break;
	default:
		break;
	}

	dev_priv->pm_event = val;

	return ret;
}

/*
 * called before innodpu_pm_notifier->innodpu_drm_hibernation
 * dev: drm platform device, drvdata is drm_dev
 */
static int innodpu_drm_freeze(struct device *dev)
{
	struct drm_device *drm_dev = dev_get_drvdata(dev);
	struct innodpu_drm_private *dev_priv = NULL;
	int ret = 0;

	if (drm_dev)
		dev_priv = innogpu_drm_to_display_private(drm_dev);
	else
		return -EINVAL;

	if (!dev_priv) {
		inno_drm_err(drm_dev->dev, "dev priv is NULL\n");
		return -EINVAL;
	}

	inno_drm_info(dev, "drm freeze\n");

	mutex_lock(&dev_priv->logo_lock);
	if (innodpu_get_logo_status(drm_dev, dev_priv) == LOGO_SWITCH_HIBERNATION &&
		drm_dev->switch_power_state == DRM_SWITCH_POWER_ON) {
		inno_drm_info(dev, "time delayed display(logo-hibernation)\n");
		innodpu_logo_timedelay_dis(drm_dev, dev_priv);
	}
	mutex_unlock(&dev_priv->logo_lock);

	if (drm_dev->switch_power_state == DRM_SWITCH_POWER_ON)
		ret = innodpu_drm_hibernation(dev);

	innodpu_drm_gem_backup(dev);

	return ret;
}

static int innodpu_drm_thaw(struct device *dev)
{
	struct drm_device *drm_dev = dev_get_drvdata(dev);
	struct innodpu_drm_private *dev_priv = innogpu_drm_to_display_private(drm_dev);

	if (!dev_priv) {
		inno_drm_err(drm_dev->dev, "dev priv is NULL\n");
		return -EINVAL;
	}

	inno_drm_info(dev, "drm thaw\n");

	if (dev_priv->has_inv_mem)
		innodpu_sysmem_thaw_free(dev_priv->invisible_mem_manager);
	innodpu_sysmem_thaw_free(dev_priv->visible_mem_manager);

	if (dev_priv->has_shared_mem) {
		if (dev_priv->shared_vram_info.current_user)
			innodpu_sysmem_thaw_free(dev_priv->shared_vram_info.current_user->mem_manager);
	}

	if (dev_priv->pm_event == PM_RESTORE_PREPARE) {
		inno_drm_warn(dev, "s4 wakeup failure, wakeup display in thaw interface.\n");
		innodpu_drm_wakeup(dev);
	}

	return 0;
}

static int innodpu_drm_poweroff(struct device *dev)
{
	inno_drm_info(dev, "drm poweroff\n");
	return 0;
}

static int innodpu_drm_restore(struct device *dev)
{
	inno_drm_info(dev, "drm restore\n");
	innodpu_drm_gem_recover(dev);
	return innodpu_drm_wakeup(dev);
}

const struct dev_pm_ops innogpu_drm_pm_ops = {
	.suspend = innodpu_drm_suspend,
	.resume = innodpu_drm_resume,
	.freeze = innodpu_drm_freeze,
	.thaw = innodpu_drm_thaw,
	.poweroff = innodpu_drm_poweroff,
	.restore = innodpu_drm_restore,
};
