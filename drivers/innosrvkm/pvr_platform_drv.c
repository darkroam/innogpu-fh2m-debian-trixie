
#include <linux/version.h>
#include "inno_drm_version.h"
#if (DRM_VERSION >= KERNEL_VERSION(5, 5, 0))
#include <drm/drm_drv.h>
#include <drm/drm_print.h>
#include <linux/mod_devicetable.h>
#include <linux/dma-mapping.h>
#include <linux/of.h>
#include <linux/slab.h>
#else
#include <drm/drmP.h>
#endif

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>
#include <linux/notifier.h>

#include "module_common.h"
#include "pvr_drv.h"
#include "pvrmodule.h"
#include "sysinfo.h"
#include "pvrsrv_error.h"
#include "pvrsrv.h"

#include "pvr_sync.h"
#include "pvr_sync_api.h"

#include <linux/component.h>
#include "innogpu_drm.h"


#if defined(SUPPORT_DISPLAY_CLASS)
/* Display class interface */
#include "kerneldisplay.h"
INNO_EXT_SYM(fh2m_DCRegisterDevice);
INNO_EXT_SYM(fh2m_DCUnregisterDevice);
INNO_EXT_SYM(fh2m_DCDisplayConfigurationRetired);
INNO_EXT_SYM(fh2m_DCDisplayHasPendingCommand);
INNO_EXT_SYM(fh2m_DCImportBufferAcquire);
INNO_EXT_SYM(fh2m_DCImportBufferRelease);

/* Physmem interface (required by LMA DC drivers) */
#include "physheap.h"
INNO_EXT_SYM(fh2m_PhysHeapAcquireByUsage);
INNO_EXT_SYM(fh2m_PhysHeapRelease);
INNO_EXT_SYM(fh2m_PhysHeapGetType);
INNO_EXT_SYM(fh2m_PhysHeapGetCpuPAddr);
INNO_EXT_SYM(fh2m_PhysHeapGetSize);
INNO_EXT_SYM(fh2m_PhysHeapCpuPAddrToDevPAddr);

INNO_EXT_SYM(fh2m_PVRSRVGetDriverStatus);
INNO_EXT_SYM(fh2m_PVRSRVSystemInstallDeviceLISR);
INNO_EXT_SYM(fh2m_PVRSRVSystemUninstallDeviceLISR);

#include "pvr_notifier.h"
INNO_EXT_SYM(fh2m_PVRSRVCheckStatus);

#include "pvr_debug.h"
INNO_EXT_SYM(fh2m_PVRSRVGetErrorString);
INNO_EXT_SYM(fh2m_PVRSRVGetDeviceInstance);
#endif /* defined(SUPPORT_DISPLAY_CLASS) */

#if defined(SUPPORT_RGX)
#include "rgxapi_km.h"
#if defined(SUPPORT_SHARED_SLC)
INNO_EXT_SYM(fh2m_RGXInitSLC);
#endif
INNO_EXT_SYM(fh2m_RGXHWPerfConnect);
INNO_EXT_SYM(fh2m_RGXHWPerfDisconnect);
INNO_EXT_SYM(fh2m_RGXHWPerfControl);
#if defined(HWPERF_PACKET_V2C_SIG)
INNO_EXT_SYM(fh2m_RGXHWPerfConfigureCounters);
#else
INNO_EXT_SYM(fh2m_RGXHWPerfConfigMuxCounters);
INNO_EXT_SYM(fh2m_RGXHWPerfConfigureAndEnableCustomCounters);
#endif
INNO_EXT_SYM(fh2m_RGXHWPerfDisableCounters);
INNO_EXT_SYM(fh2m_RGXHWPerfAcquireEvents);
INNO_EXT_SYM(fh2m_RGXHWPerfReleaseEvents);
INNO_EXT_SYM(fh2m_RGXHWPerfConvertCRTimeStamp);
#if defined(SUPPORT_KERNEL_HWPERF_TEST)
INNO_EXT_SYM(fh2m_OSAddTimer);
INNO_EXT_SYM(fh2m_OSEnableTimer);
INNO_EXT_SYM(fh2m_OSDisableTimer);
INNO_EXT_SYM(fh2m_OSRemoveTimer);
#endif
#endif

#include "inno_misc.h"
#include "inno_srvkm.h"
#include "process_stats.h"
#include "rgxinit.h"
INNO_EXT_SYM(fh2m_inno_pmr_cpumapcount_dec);
INNO_EXT_SYM(fh2m_inno_pmr_unref);
INNO_EXT_SYM(fh2m_inno_pmr_ref);
INNO_EXT_SYM(fh2m_inno_pmr_lock_sys_phys_addr);
INNO_EXT_SYM(fh2m_inno_pmr_unlock_sys_phys_addr);
INNO_EXT_SYM(fh2m_inno_stats_remove_mem_alloc_record);
INNO_EXT_SYM(fh2m_inno_stats_decr_mem_alloc_stat);
INNO_EXT_SYM(fh2m_inno_pmr_write_bytes);
INNO_EXT_SYM(fh2m_inno_pmr_read_bytes);
INNO_EXT_SYM(fh2m_PVRSRVGetPVRSRVData);
INNO_EXT_SYM(fh2m_PVRSRVGetDevicePowerState);
INNO_EXT_SYM(fh2m_GetMemInfoByPID);
INNO_EXT_SYM(fh2m_RGXDevBVNCString);


#if defined(__arm64__) || defined(__aarch64__) || defined(PVRSRV_DEVMEM_TEST_SAFE_MEMSETCPY)
#include "osfunc_common.h"

#if defined(__GNUC__)
/* Workarounds for assumptions made that memory will not be mapped uncached
 * in kernel or user address spaces on arm64 platforms (or other testing).
 * */

INNO_EXT_SYM(fh2m_DeviceMemSet);
INNO_EXT_SYM(fh2m_DeviceMemCopy);

#else /* defined __GNUC__ */

INNO_EXT_SYM(fh2m_DeviceMemSetBytes);
INNO_EXT_SYM(fh2m_DeviceMemCopyBytes);

#endif /* defined __GNUC__ */

#endif

#include "syscommon.h"
#include "osfunc.h"
INNO_EXT_SYM(fh2m_gpu_extern_vram_flag);
INNO_EXT_SYM(fh2m_OSMemoryBarrier);

unsigned int gpu_cleanup_retry_cnt = 2000;
MODULE_PARM_DESC(gpu_cleanup_retry_cnt, "GPU cleanup work retry count (default: 2000)");
module_param_named(gpu_cleanup_retry_cnt, gpu_cleanup_retry_cnt, uint, 0600);

unsigned int gpu_cleanup_retry_delay_time = 1000;
MODULE_PARM_DESC(gpu_cleanup_retry_delay_time , "When gpu retries more than half the cnt, the delay time (default: 1000 ms)");
module_param_named(gpu_cleanup_retry_delay_time , gpu_cleanup_retry_delay_time , uint, 0600);

/* This header must always be included last */
#include "kernel_compatibility.h"

/* Values used to configure the PVRSRV_DEVICE_INIT_MODE tunable (Linux-only) */
#define PVRSRV_LINUX_DEV_INIT_ON_PROBE   1
#define PVRSRV_LINUX_DEV_INIT_ON_OPEN    2
#define PVRSRV_LINUX_DEV_INIT_ON_CONNECT 3

static int innogpu_pvr_bind(struct device *dev,
		struct device *master, void *data);
static void innogpu_pvr_unbind(struct device *dev,
		struct device *master, void *data);

static DEFINE_MUTEX(g_device_mutex);

static int pvr_power_callback(struct notifier_block *pvr_power_notif, unsigned long event, void *ddev)
{
	struct pvr_drm_private *priv;

	priv = container_of(pvr_power_notif, struct pvr_drm_private, pvr_power_notifier);

	if (SYS_POWER_OFF == event)
	{
		priv->pvr_power_status = SYS_POWER_OFF;
	}
	else if (SYS_RESTART == event)
	{
		priv->pvr_power_status = SYS_RESTART;
	}

	return NOTIFY_DONE;
}

extern struct platform_driver g_innogpu_pvrsrvkm_driver;
static const struct component_ops s_innogpu_pvr_ops = {
	.bind = innogpu_pvr_bind,
	.unbind = innogpu_pvr_unbind,
};

static int innogpu_pvr_drm_load(struct device *dev, unsigned long flags)
{
	struct platform_device *pdev = fh2m_inno_to_platform_device(dev);
	plat_data_t *pdata = fh2m_inno_platform_get_data(pdev);
	struct drm_device *drm_dev = NULL;
	struct pvr_drm_private *priv = NULL;
	enum PVRSRV_ERROR_TAG srv_err;
	int err, deviceId;

#if defined(NO_HARDWARE)
	drm_dev = fh2m_inno_platform_get_drvdata(pdev);
	pdata->pdev_rsrc = NULL;
#else
	drm_dev = pdata->pdev_rsrc->ddev[pdata->dev_idx];
#endif
	if (drm_dev == NULL)
		DRM_ERROR("%s %d drm_dev is NULL\n", __func__, __LINE__);

	priv = innogpu_drm_to_pvr_private(drm_dev);
	BUG_ON(!priv);
	DRM_DEBUG_DRIVER("device %p\n", drm_dev->dev);

	/*
	dev_set_drvdata(drm_dev->dev, drm_dev); // dropped, drm_dev->dev is pcie device, foribidden to use.
	*/

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 12, 0))
	/*
	 * Older kernels do not have render drm_minor member in drm_device,
	 * so we fallback to primary node for device identification
	 */
	deviceId = drm_dev->primary->index;
#else
	if (drm_dev->render)
		deviceId = drm_dev->render->index;
	else /* when render node is NULL, fallback to primary node */
		deviceId = drm_dev->primary->index;
#endif

	/* dropped
	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		err = -ENOMEM;
		goto err_exit;
	}
	drm_dev->dev_private = priv;

	// TBD ???
	if (!drm_dev->dev->dma_parms)
		drm_dev->dev->dma_parms = &priv->dma_parms;
	dma_set_max_seg_size(drm_dev->dev, DMA_BIT_MASK(32));
	*/

	mutex_lock(&g_device_mutex);

	srv_err = PVRSRVCommonDeviceCreate(dev, deviceId, &priv->dev_node);
	if (srv_err != PVRSRV_OK) {
		DRM_ERROR("failed to create device node for device %p (%s)\n",
			  drm_dev->dev, fh2m_PVRSRVGetErrorString(srv_err));
		if (srv_err == PVRSRV_ERROR_PROBE_DEFER)
			err = -EPROBE_DEFER;
		else
			err = -ENODEV;
		goto err_unset_dma_parms;
	}

	srv_err = INNOPVRSRVCommonDeviceCreate(priv->dev_node);
	if (srv_err != PVRSRV_OK) {
		DRM_ERROR("INNOPVRSRVCommonDeviceCreate initialisation failed : %s\n", fh2m_PVRSRVGetErrorString(srv_err));
		err = -EINVAL;
		goto err_device_destroy;
	}

	err = PVRSRVDeviceInit(priv->dev_node);
	if (err) {
		DRM_ERROR("device %p initialisation failed (err=%d)\n",
			  drm_dev->dev, err);
		goto err_inno_device_destroy;
	}
	/*
	drm_mode_config_init(drm_dev); // dropped
	*/

#if (PVRSRV_DEVICE_INIT_MODE == PVRSRV_LINUX_DEV_INIT_ON_PROBE)
	srv_err = PVRSRVCommonDeviceInitialise(priv->dev_node);
	if (srv_err != PVRSRV_OK) {
		err = -ENODEV;
		DRM_ERROR("device %p initialisation failed (err=%d)\n",
			  drm_dev->dev, err);
		goto err_device_deinit;
	}
#endif

	mutex_unlock(&g_device_mutex);

	priv->pvr_power_status = 0;
	priv->pvr_power_notifier.notifier_call = pvr_power_callback;
	err = register_reboot_notifier(&priv->pvr_power_notifier);
	if (err) {
		DRM_ERROR("device %p pvr_power_notifier error \n", drm_dev->dev);
		goto err_reboot_register_fail;
	}

	return 0;

err_reboot_register_fail:
	mutex_lock(&g_device_mutex);
#if (PVRSRV_DEVICE_INIT_MODE == PVRSRV_LINUX_DEV_INIT_ON_PROBE)
err_device_deinit:
	/*
	drm_mode_config_cleanup(drm_dev); // dropped
	*/
	PVRSRVDeviceDeinit(priv->dev_node);
#endif
err_inno_device_destroy:
	INNOPVRSRVCommonDeviceDestroy(priv->dev_node);
err_device_destroy:
	PVRSRVCommonDeviceDestroy(priv->dev_node);
err_unset_dma_parms:
	mutex_unlock(&g_device_mutex);
	/* dropped
	if (drm_dev->dev->dma_parms == &priv->dma_parms)
		drm_dev->dev->dma_parms = NULL;
	kfree(priv);

err_exit:
	*/
	return err;
}


static int innogpu_pvr_drm_unload(struct drm_device *drm_dev)
{
	struct pvr_drm_private *priv = innogpu_drm_to_pvr_private(drm_dev);

	BUG_ON(!priv);

	DRM_DEBUG_DRIVER("device %p\n", drm_dev->dev);

	unregister_reboot_notifier(&priv->pvr_power_notifier);

	/*
	drm_mode_config_cleanup(drm_dev); // dropped
	*/

	PVRSRVDeviceDeinit(priv->dev_node);

	mutex_lock(&g_device_mutex); // TBD???
	INNOPVRSRVCommonDeviceDestroy(priv->dev_node);
	PVRSRVCommonDeviceDestroy(priv->dev_node);
	mutex_unlock(&g_device_mutex);

	/* dropped
	if (drm_dev->dev->dma_parms == &priv->dma_parms)
		drm_dev->dev->dma_parms = NULL;

	kfree(priv);
	drm_dev->dev_private = NULL;
	*/
	return 0;
}

/**
 * innogpu_pvr_bind - innosilicon pvr-driver initialization function
 * sync from function pvr_probe
 * @dev:
 * parent(dev->parent) is &pci_dev.dev
 * @master: component master
 * @data: point of struct drm_device
 *
 * This function initializes the innosilicon pvr device
 *
 * Returns:
 * 0 if it is OK, errno otherwise.
 */
static int innogpu_pvr_bind(struct device *dev,
		struct device *master, void *data)
{
	struct drm_device *drm_dev = (struct drm_device *)data;
	struct platform_device *pdev = fh2m_inno_to_platform_device(dev);
#if defined(NO_HARDWARE)
#else
	plat_data_t *pdata = fh2m_inno_platform_get_data(pdev);
	pdata->pdev_rsrc->ddev[pdata->dev_idx] = drm_dev;
	if (pdata->pdev_rsrc->pdev == NULL) {
		DRM_ERROR("pdev_rsrc->pdev pci_device is NULL");
	}
#endif
	fh2m_inno_platform_set_drvdata(pdev, drm_dev);
	/*
	drm_dev->platformdev = pdev; // dropped
	*/
	BUG_ON(!data);
	return innogpu_pvr_drm_load(dev, 0);
}

/**
 *innogpu_pvr_unbind - innosilicon pvr-driver deinitialization function
 * sync from function pvr_remove
 *@dev:
 *parent(dev->parent) is &pci_dev.dev
 *@master: component master
 *@data: point of struct drm_device
 *
 */
 static void innogpu_pvr_unbind(struct device *dev,
		struct device *master, void *data)
{
	struct platform_device *pdev = fh2m_inno_to_platform_device(dev);
	struct drm_device *drm_dev = (struct drm_device *)data;

	BUG_ON(!data);

	innogpu_pvr_drm_unload(drm_dev);
	fh2m_inno_platform_set_drvdata(pdev, NULL);
}

/**
 *innogpu_pvr_unbind - sync from function pvr_init
 *
 */
static int innogpu_pvr_probe(struct platform_device *pdev)
{
	BUG_ON(!pdev);
	DRM_DEBUG_DRIVER("\n");
	return component_add(&pdev->dev, &s_innogpu_pvr_ops);
}
#if 0
static void pvr_devices_unregister(void)
{
#if defined(MODULE) && !defined(PVR_LDM_PLATFORM_PRE_REGISTERED)
	unsigned int i;

	BUG_ON(!pvr_devices);

	for (i = 0; i < pvr_num_devices && pvr_devices[i]; i++)
		platform_device_unregister(pvr_devices[i]);

	kfree(pvr_devices);
	pvr_devices = NULL;
#endif /* defined(MODULE) && !defined(PVR_LDM_PLATFORM_PRE_REGISTERED) */
}
#endif
/**
 *innogpu_pvr_unbind - sync from function pvr_exit
 *
 */
static int innogpu_pvr_remove(struct platform_device *pdev)
{
	BUG_ON(!pdev);
	component_del(&pdev->dev, &s_innogpu_pvr_ops);
	return 0;
}


static void innogpu_pvr_shutdown(struct platform_device *pdev)
{
	struct drm_device *drm_dev = fh2m_inno_platform_get_drvdata(pdev); //innogpu_drm_component_bind  fh2m_inno_platform_set_drvdata(pdev, drm_dev);
	if (!drm_dev) {
		DRM_ERROR("device %p shutdown not bind\n", &pdev->dev);
		return;
	}

	DRM_DEBUG_DRIVER("device %p\n", &pdev->dev);

	PVRSRVDeviceShutdown(drm_dev);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 9, 0))
static const struct of_device_id innogpu_pvr_of_ids[] = {
#if defined(SYS_RGX_OF_COMPATIBLE)
	{ .compatible = SYS_RGX_OF_COMPATIBLE, },
#endif
	{},
};

#if !defined(CHROMIUMOS_KERNEL)
MODULE_DEVICE_TABLE(of, innogpu_pvr_of_ids);
#endif
#endif

static struct platform_device_id innogpu_pvr_platform_ids[] = {
#if defined(SYS_RGX_DEV_NAME)
	{ SYS_RGX_DEV_NAME, 0 },
#endif
	{ }
};

#if !defined(CHROMIUMOS_KERNEL)
MODULE_DEVICE_TABLE(platform, innogpu_pvr_platform_ids);
#endif

struct platform_driver g_innogpu_pvrsrvkm_driver = {
	.driver = {
		.name		= SYS_RGX_DEV_NAME,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 9, 0))
		.of_match_table	= of_match_ptr(innogpu_pvr_of_ids),
#endif
		.pm		= &pvr_pm_ops,
	},
	.id_table		= innogpu_pvr_platform_ids,
	.probe			= innogpu_pvr_probe,
	.remove			= innogpu_pvr_remove,
	.shutdown		= innogpu_pvr_shutdown,
};

MODULE_FIRMWARE("innogpu/innogpu.fw.35.2.1632.23");
MODULE_FIRMWARE("innogpu/innogpu.sh.35.2.1632.23");
MODULE_FIRMWARE("innogpu/innogpu.fw.35.4.1632.23");
MODULE_FIRMWARE("innogpu/innogpu.sh.35.4.1632.23");
MODULE_FIRMWARE("innogpu/innogpu.fw.70.3.2448.1360");
MODULE_FIRMWARE("innogpu/innogpu.sh.70.3.2448.1360");

MODULE_FIRMWARE("innogpu/fh2c.fw");
MODULE_FIRMWARE("innogpu/fh2c.sh");
MODULE_FIRMWARE("innogpu/fh2m.fw");
MODULE_FIRMWARE("innogpu/fh2m.sh");

