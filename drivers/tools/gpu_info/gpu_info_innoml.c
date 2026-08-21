#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/ioctl.h>
#include <linux/ctype.h>
#include <linux/ctype.h>
#include <linux/pci.h>

#include "allocmem.h"
#include "img_types.h"
#include "process_stats.h"
#include "rgxinit.h"
#include "gpu_info_innoml.h"
#include "gpu_info_innoml_km.h"
#include "pvrsrv.h"
#include "inno_mm.h"
#include "hal_bmc.h"
#include "inno_pci.h"
#include "inno_uuid.h"
#include "inno_cpumask.h"
#include "rgx_fwif_km.h"
#include "devicemem_utils.h"
#include "debug_common.h"
#include "hal_interface.h"
#include "hal.h"
#include "inno_srvkm.h"

#define INNOGPU_CLASS "innogpu_class"
#define INNOGPU_ML_DEVICE "innogpu_ml_device"
#define CHIP_INVALID_VAL (255)

struct innoml_cdev {
	inno_dev *pdev;
	struct cdev inno_cdev;
	PVRSRV_DEVICE_NODE *psDeviceNodeArray[MAX_GPU_CORES_PER_PCI_DEV];
};

static int g_major;
static struct class *g_innogpu_class;
static struct innoml_cdev g_innogpu_cdev[INNOGPU_MAX_DEV_NUM];

typedef int innogpu_ioctl_t(void *innoml_ctx, void *data);

struct innogpu_ioctl_desc {
	unsigned int cmd;
	innogpu_ioctl_t *func;
};

static int inno_open(struct inode *inode, struct file *file)
{
    PVRSRV_DEVICE_NODE *psDeviceNode = NULL;
    int minor = iminor(inode);
    PVRSRV_DATA *psPVRSRVData = NULL;
	inno_dev *pci_dev;
	inno_pci_dev *pdev;
	struct dev_rsrc *pdev_rsrc = NULL;
	int num = 0;

	if (g_innogpu_cdev[minor].pdev == NULL) {
		innoml_error("minor %d pdev is NULL!\n", minor);
		return -1;
	}

	pdev_rsrc = fh2m_inno_rsrc_devres_find(g_innogpu_cdev[minor].pdev);
    psPVRSRVData = fh2m_PVRSRVGetPVRSRVData();
    for (psDeviceNode = psPVRSRVData->psDeviceNodeList;
                       psDeviceNode != NULL;
                       psDeviceNode = psDeviceNode->psNext)
    {
		pci_dev = fh2m_inno_dev_get_parent(psDeviceNode->psDevConfig->pvOSDevice);
		pdev = fh2m_inno_to_pci_dev(pci_dev);
		if (pdev_rsrc->pdev == pdev) {
			if (num >= MAX_GPU_CORES_PER_PCI_DEV) {
				innoml_error("num:%d exceeds the max value!\n", num);
				return -1;
			}

			g_innogpu_cdev[minor].psDeviceNodeArray[num] = psDeviceNode;
			num++;
		}
    }

	if (g_innogpu_cdev[minor].psDeviceNodeArray[0] == NULL) {
		innoml_error("psDeviceNode is all NULL!\n");
		return -1;
	}

    file->private_data = &g_innogpu_cdev[minor];
    return 0;
}

static int innogpu_get_gpu_status(void *innoml_ctx, void *data)
{
	struct innoml_cdev *innoml_cdev = (struct innoml_cdev*)innoml_ctx;
    PVRSRV_DEVICE_NODE *psDeviceNode = NULL;
    PVRSRV_DEVICE_HEALTH_STATUS eHealthStatus;
	PVRSRV_DEVICE_HEALTH_REASON eHealthReason;
    int devStatus, devReason;
    InnomlGpuStatus *status = (InnomlGpuStatus*)data;
	uint32_t core_nums = fh2m_hal_get_gpu_core_nums();
	int i = 0;

	for (i = 0; i < core_nums; i++) {
		psDeviceNode = innoml_cdev->psDeviceNodeArray[i];
		if (psDeviceNode->pfnUpdateHealthStatus)
		{
			psDeviceNode->pfnUpdateHealthStatus(psDeviceNode, IMG_FALSE);
		}

		eHealthStatus = OSAtomicRead(&psDeviceNode->eHealthStatus);
		eHealthReason = OSAtomicRead(&psDeviceNode->eHealthReason);

		switch (eHealthStatus)
		{
		case PVRSRV_DEVICE_HEALTH_STATUS_OK:  devStatus = DEVICE_OK;  break;
		case PVRSRV_DEVICE_HEALTH_STATUS_NOT_RESPONDING:  devStatus = NOT_RESPONDING;  break;
		case PVRSRV_DEVICE_HEALTH_STATUS_DEAD:  devStatus = DEAD;  break;
		case PVRSRV_DEVICE_HEALTH_STATUS_FAULT:  devStatus = FAULT;  break;
		case PVRSRV_DEVICE_HEALTH_STATUS_UNDEFINED:  devStatus = UNDEFINED;  break;
		default:  devStatus = UNKNOWN;  break;
		}

		switch (eHealthReason)
		{
		case PVRSRV_DEVICE_HEALTH_REASON_NONE:  devReason = DEVICE_NONE;  break;
		case PVRSRV_DEVICE_HEALTH_REASON_ASSERTED:  devReason = DEVICE_ASSERTED;  break;
		case PVRSRV_DEVICE_HEALTH_REASON_POLL_FAILING:  devReason = DEVICE_POLL_FAILING;  break;
		case PVRSRV_DEVICE_HEALTH_REASON_TIMEOUTS:  devReason = DEVICE_TIMEOUTS;  break;
		case PVRSRV_DEVICE_HEALTH_REASON_QUEUE_CORRUPT:  devReason = DEVICE_QUEUE_CORRUPT;  break;
		case PVRSRV_DEVICE_HEALTH_REASON_QUEUE_STALLED:  devReason = DEVICE_QUEUE_STALLED;  break;
		case PVRSRV_DEVICE_HEALTH_REASON_IDLING:  devReason = DEVICE_IDLING;  break;
		case PVRSRV_DEVICE_HEALTH_REASON_RESTARTING:  devReason = DEVICE_RESTARTING;  break;
		case PVRSRV_DEVICE_HEALTH_REASON_MISSING_INTERRUPTS:  devReason = DEVICE_MISSING_INTERRUPTS;  break;
		default:  devReason = DEVICE_UNKNOW_REASON;  break;
		}

		status->gpustatus[i].devStatus = devStatus;
		status->gpustatus[i].devHealthReason = devReason;

		if (PVRSRV_ERROR_LIMIT_REACHED)
			status->gpustatus[i].serverEventCount = IMG_UINT32_MAX;
		else
			status->gpustatus[i].serverEventCount = PVRSRV_KM_ERRORS;

		/* Write other useful stats to aid the test cycle... */
		if (psDeviceNode->pvDevice != NULL)
		{
#ifdef SUPPORT_RGX
			PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;
			const RGXFWIF_HWRINFOBUF *psHWRInfoBuf = psDevInfo->psRGXFWIfHWRInfoBufCtl;
			const RGXFWIF_SYSDATA *psFwSysData = psDevInfo->psRGXFWIfFwSysData;

			/* Calculate the number of HWR events in total across all the DMs... */
			if (psHWRInfoBuf != NULL)
			{
				IMG_UINT32 ui32HWREventCount = 0;
				IMG_UINT32 ui32CRREventCount = 0;
				IMG_UINT32 ui32DMIndex;

				for (ui32DMIndex = 0; ui32DMIndex < RGXFWIF_DM_MAX; ui32DMIndex++)
				{
					ui32HWREventCount += psHWRInfoBuf->aui32HwrDmLockedUpCount[ui32DMIndex];
					ui32CRREventCount += psHWRInfoBuf->aui32HwrDmOverranCount[ui32DMIndex];
				}
				status->gpustatus[i].hwrEventCount = ui32HWREventCount;
				status->gpustatus[i].crrErrCount = ui32CRREventCount;

#ifdef PVRSRV_STALLED_CCB_ACTION
				/* Write the number of Sync Lockup Recovery (SLR) events... */
				status->gpustatus[i].slrErrCount = psDevInfo->psRGXFWIfFwOsData->ui32ForcedUpdatesRequested;
#endif /* PVRSRV_STALLED_CCB_ACTION */
			}

			/* Show error counts */
			status->gpustatus[i].wgpErrCount = psDevInfo->sErrorCounts.ui32WGPErrorCount;
			status->gpustatus[i].trpErrCount = psDevInfo->sErrorCounts.ui32TRPErrorCount;

			/*
			 * Guest drivers do not support the following functionality:
			 *	- Perform actual on-chip fw tracing.
			 *	- Collect actual on-chip GPU utilization stats.
			 *	- Perform actual on-chip GPU power/dvfs management.
			 *	- As a result no more information can be provided.
			 */
			if (!PVRSRV_VZ_MODE_IS(GUEST))
			{
				if (psFwSysData != NULL)
				{
					status->gpustatus[i].fwfErrCount = psFwSysData->ui32FWFaults;
				}

				/* Write the number of APM events... */
				status->gpustatus[i].apmErrCount = psDevInfo->ui32ActivePMReqTotal;
			}
#endif /* SUPPORT_RGX */
		}
	}

    return 0;
}

#if !defined(NO_HARDWARE) && defined(__G0M_SOC__)
extern int prohibit_umd_gtt_alloc;
#endif

static IMG_UINT32 get_numa_node(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	PVRSRV_DEVICE_CONFIG *psDevConfig;
	inno_platform_device *innoDev;
	IMG_UINT32 node_id;

	psDevConfig = psDeviceNode->psDevConfig;
	innoDev = fh2m_inno_to_platform_device(psDevConfig->pvOSDevice);
	node_id = fh2m_inno_platform_get_numa_node(innoDev);

	if (node_id == -1)
		node_id = 0;

	return node_id;
}

static InnoGpuArch get_gpu_arch(struct dev_rsrc *pdev_rsrc)
{
    IMG_UINT32 device_id;
    InnoGpuArch arch;

    device_id = pdev_rsrc->chip.gpu_feature.device_id;
    if (device_id == 0x8810) {
        arch = INNO_G1P;
    } else if (device_id == 0x8800) {
        arch = INNO_G1;
    } else if (device_id == 0x9800) {
        arch = INNO_G0;
    } else if (device_id == 0x9810) {
        arch = INNO_G0M;
    } else {
        arch = INNO_UNKNOW_ARCH;
    }

    return arch;
}

static int innogpu_get_ioctl_version(void *innoml_ctx, void *data)
{
    InnomlIoctlVersion *version = (InnomlIoctlVersion *)data;

    version->major = INNOML_IOCTL_VERSION_MAJOR;
    version->minor = INNOML_IOCTL_VERSION_MINOR;
    version->revision = INNOML_IOCTL_VERSION_REVISION;

    return 0;
}

static int innogpu_get_gpu_info(void *innoml_ctx, void *data)
{
	struct innoml_cdev *innoml_cdev = (struct innoml_cdev*)innoml_ctx;
	struct dev_rsrc *pdev_rsrc = fh2m_inno_rsrc_devres_find(innoml_cdev->pdev);
    PVRSRV_DEVICE_NODE *psDeviceNode = NULL;
    struct cpu_affinity affinity;
    InnomlGpuInfo *info = (InnomlGpuInfo *)data;
	int ret = 0;
	struct hwinfo_item hwinfo_item;
	uint32_t core_nums = fh2m_hal_get_gpu_core_nums();
	int i = 0;

	for (i = 0; i < core_nums; i++) {
		psDeviceNode = innoml_cdev->psDeviceNodeArray[i];

		info->gpuinfo[i].gpuId = psDeviceNode->sDevId.ui32InternalID;
		fh2m_inno_sprintf(info->gpuinfo[i].deviceName, sizeof(info->gpuinfo[i].deviceName), "%s%d",
				   psDeviceNode->psDevConfig->pszName,
				   psDeviceNode->sDevId.ui32InternalID);
		info->gpuinfo[i].renderId = psDeviceNode->sDevId.i32KernelDeviceID;
	}

	info->numaId = get_numa_node(psDeviceNode);
	fh2m_inno_get_cpu_affinity(info->numaId, &affinity);
	info->cpuAffinity.first_cpu = affinity.first_cpu;
	info->cpuAffinity.last_cpu = affinity.last_cpu;

	info->gpuArch = get_gpu_arch(pdev_rsrc);
    fh2m_inno_sprintf(info->name, sizeof(info->name), "%s", psDeviceNode->psDevConfig->pszName);
    info->gpuCoreNum = core_nums;
    /* uuid */
	fh2m_inno_uuid_copy(info->uuid, innoml_cdev->psDeviceNodeArray[0]->uuid);

    info->enableGtt = 1;
#if !defined(NO_HARDWARE) && defined(__G0M_SOC__)
    info->enableGtt = fh2m_hal_get_prohibit_umd_gtt_alloc();
#endif

	fh2m_inno_strncpy(info->manufactoryName, "Innosilicon", sizeof(info->manufactoryName));
	ret = fh2m_hal_hwinfo_get_item(pdev_rsrc->dev, HW_PRODUCT_NUMBER, &hwinfo_item);
	if (!ret)
		fh2m_inno_strncpy(info->productNumber, hwinfo_item.strval, sizeof(info->productNumber));
	else
		fh2m_inno_memset(info->productNumber, -1, sizeof(info->productNumber));

	ret = fh2m_hal_hwinfo_get_item(pdev_rsrc->dev, HW_SN, &hwinfo_item);
	if (!ret)
		fh2m_inno_strncpy(info->serialNumber, hwinfo_item.strval, sizeof(info->serialNumber));
	else
		fh2m_inno_memset(info->serialNumber, -1, sizeof(info->serialNumber));
#if defined(__G1_SOC__)
	fh2m_inno_strncpy(info->productName, "Fantasy I", sizeof(info->productName));
#elif defined(__G0_SOC__)
	fh2m_inno_strncpy(info->productName, "Fantasy II", sizeof(info->productName));
#elif defined(__G1P_SOC__)
	fh2m_inno_strncpy(info->productName, "Fantasy I", sizeof(info->productName));
#elif defined(__G0M_SOC__)
	fh2m_inno_strncpy(info->productName, "Fantasy II-M", sizeof(info->productName));
#endif
    return 0;
}

static int innogpu_get_version(void *innoml_ctx, void *data)
{
    InnomlVersion *version = (InnomlVersion *)data;
	struct innoml_cdev *innoml_cdev = (struct innoml_cdev*)innoml_ctx;
	struct dev_rsrc *pdev_rsrc = fh2m_inno_rsrc_devres_find(innoml_cdev->pdev);
	PVRSRV_DEVICE_NODE *psDeviceNode = innoml_cdev->psDeviceNodeArray[0];
    int ret = 0;
	struct hwinfo_item hwinfo_item;
	uint32_t vbios_major, vbios_minor, vbios_revision;

	ret = fh2m_hal_get_mcufw_version(pdev_rsrc, version->fwVersion, sizeof(version->fwVersion));
	fh2m_inno_strncpy(version->fwVersion, &(version->fwVersion[1]), sizeof(version->fwVersion));

	ret = fh2m_hal_get_mcufw_release_time(pdev_rsrc, version->fwReleaseTime, sizeof(version->fwReleaseTime));
    if(ret)
		fh2m_inno_memset(version->fwReleaseTime, -1, sizeof(version->fwReleaseTime));

	fh2m_hal_get_driver_version(version->driverVersion, sizeof(version->driverVersion));

	fh2m_inno_strncpy(version->driverReleaseTime, DRIVER_RELEASE_TIME, sizeof(version->driverReleaseTime));

	vbios_major = fh2m_hal_bmc_get_val(pdev_rsrc->dev, BMC_VBIOS_VER_MAJOR);
	vbios_minor = fh2m_hal_bmc_get_val(pdev_rsrc->dev, BMC_VBIOS_VER_MINOR);
	vbios_revision = fh2m_hal_bmc_get_val(pdev_rsrc->dev, BMC_VBIOS_VER_REVISION);

	fh2m_inno_sprintf(version->vbiosVersion, sizeof(version->vbiosVersion), "%d.%d.%d", vbios_major, vbios_minor, vbios_revision);

	ret = fh2m_hal_hwinfo_get_item(pdev_rsrc->dev, HW_PCB_VERSION, &hwinfo_item);
	if (!ret)
		fh2m_inno_strncpy(version->pcbVersion, hwinfo_item.strval, sizeof(version->pcbVersion));
	else
		fh2m_inno_memset(version->pcbVersion, -1, sizeof(version->pcbVersion));
	ret = fh2m_hal_hwinfo_get_item(pdev_rsrc->dev, HW_ODM_GEN_VERSION, &hwinfo_item);
	if (!ret)
		fh2m_inno_strncpy(version->hwinfoODMVersion, hwinfo_item.strval, sizeof(version->hwinfoODMVersion));
	else
		fh2m_inno_memset(version->hwinfoODMVersion, -1, sizeof(version->hwinfoODMVersion));
	ret = fh2m_hal_hwinfo_get_item(pdev_rsrc->dev, CU_ODM_GEN_VERSION, &hwinfo_item);
	if (!ret)
		fh2m_inno_strncpy(version->customODMVersion, hwinfo_item.strval, sizeof(version->customODMVersion));
	else
		fh2m_inno_memset(version->customODMVersion, -1, sizeof(version->customODMVersion));


    {
        char *tmp = NULL;
        fh2m_inno_strncpy(version->devName, INNOGPU_DEVICE_NAME, sizeof(version->devName));
        tmp = fh2m_RGXDevBVNCString((PVRSRV_RGXDEV_INFO *)psDeviceNode->pvDevice);
        fh2m_inno_strncpy(version->bvnc, tmp, sizeof(version->bvnc));
    }

    return 0;
}

static int innogpu_get_pci_info(void *innoml_ctx, void *data)
{
	struct innoml_cdev *innoml_cdev = (struct innoml_cdev*)innoml_ctx;
	struct dev_rsrc *pdev_rsrc = fh2m_inno_rsrc_devres_find(innoml_cdev->pdev);
    inno_pci_dev *pdev = pdev_rsrc->pdev;
    InnomlPciInfo *info = (InnomlPciInfo*)data;
	unsigned int link_speed;
	unsigned int link_width;
	unsigned int lnkcap;
	unsigned int speed[] = {2500, 5000, 8000, 16000, 32000};
	unsigned int width[] = {1, 2, 4, 8, 12, 16, 32};
	int index = 0, pcie_cap;
    unsigned short uiSubVendorID, uiSubDeviceID, uiVendorID, uiDevID;

    info->pciLinkLanes = fh2m_hal_bmc_get_val(pdev_rsrc->dev, BMC_PCIE_WIDTH);
    info->pciLinkStatus = fh2m_hal_bmc_get_val(pdev_rsrc->dev, BMC_PCIE_LINK_STATUS);
    info->generation = fh2m_hal_bmc_get_val(pdev_rsrc->dev, BMC_PCIE_SPEED);

	link_speed = info->generation;
	switch (link_speed) {
		case 1:
			index = 0;
			break;
		case 2:
			index = 1;
			break;
		case 3:
			index = 2;
			break;
		case 4:
			index = 3;
			break;
		case 5:
			index = 4;
			break;
	}
	info->speed = speed[index];

	pcie_cap = pci_find_capability(pdev, PCI_CAP_ID_EXP);
	if (pcie_cap) {
		pci_read_config_dword(pdev, pcie_cap + PCI_EXP_LNKCAP, &lnkcap);

		link_speed = lnkcap & PCI_EXP_LNKCAP_SLS;
		info->maxGeneration = link_speed;
		switch (link_speed) {
		case 1:
			index = 0;
			break;
		case 2:
			index = 1;
			break;
		case 3:
			index = 2;
			break;
		case 4:
			index = 3;
			break;
		case 5:
			index = 4;
			break;
		}
		info->maxSpeed = speed[index];

		link_width = (lnkcap & PCI_EXP_LNKCAP_MLW) >> 4;
		if (link_width & 1) {
			index = 0;
		} else if (link_width & 2) {
			index = 1;
		} else if (link_width & 4) {
			index = 2;
		} else if (link_width & 8) {
			index = 3;
		} else if (link_width & 12) {
			index = 4;
		} else if (link_width & 16) {
			index = 5;
		} else if (link_width & 32) {
			index = 6;
		}
		info->maxPciLinkLanes = width[index];
	} else {
		innoml_error("pcie capability not found\n");
	}

    info->memBarAddr = pdev_rsrc->vram_cfg.vram_host_base;
    info->memBarLen = pdev_rsrc->ddr_bar_len;
    info->regBarAddr = pdev_rsrc->bar0_paddr;
    info->regBarLen = pdev_rsrc->sys_bar_len;

    info->domain = pci_domain_nr(((struct pci_dev *)pdev)->bus);
    info->bus = ((struct pci_dev *)pdev)->bus->number;
    info->device = PCI_SLOT(((struct pci_dev *)pdev)->devfn);

    uiVendorID = fh2m_inno_get_pci_vendor(pdev);
    uiDevID = fh2m_inno_get_pci_device(pdev);
    info->pciDeviceId = (uiDevID << 16) | uiVendorID;

    uiSubVendorID = fh2m_inno_get_pci_subvendor(pdev);
    uiSubDeviceID = fh2m_inno_get_pci_subdevice(pdev);
    info->pciSubSystemId = (uiSubDeviceID << 16) | uiSubVendorID;

    info->baseClass = fh2m_inno_get_pci_baseclass(pdev);
    info->subClass =  fh2m_inno_get_pci_subclass(pdev);
    fh2m_inno_sprintf(info->busId, 32, "%04x:%02x:%02x.%01x\0",
                 pci_domain_nr(((struct pci_dev *)pdev)->bus),
                 ((struct pci_dev *)pdev)->bus->number,
                 PCI_SLOT(((struct pci_dev *)pdev)->devfn),
                 PCI_FUNC(((struct pci_dev *)pdev)->devfn));
    return 0;
}

static int innogpu_get_temp_info(void *innoml_ctx, void *data)
{
	struct innoml_cdev *innoml_cdev = (struct innoml_cdev*)innoml_ctx;
	struct dev_rsrc *pdev_rsrc = fh2m_inno_rsrc_devres_find(innoml_cdev->pdev);
    InnomlTemperatureInfo *info = (InnomlTemperatureInfo*)data;

    fh2m_hal_get_and_deal_bmc_info(pdev_rsrc->dev, (void *)BMC_BOAR_TEMP, &info->boardTemperature);
	if (info->boardTemperature == CHIP_INVALID_VAL)
		info->boardTemperature = 0xFFFFFFFF;

    fh2m_hal_get_and_deal_bmc_info(pdev_rsrc->dev, (void *)BMC_CHIP_TEMP, &info->chipTemperature);
	if (info->chipTemperature == CHIP_INVALID_VAL)
		info->chipTemperature = 0xFFFFFFFF;

    fh2m_hal_get_and_deal_bmc_info(pdev_rsrc->dev, (void *)BMC_FAN_SPEED, &info->fanSpeed);
	if (info->fanSpeed == CHIP_INVALID_VAL)
		info->fanSpeed = 0xFFFFFFFF;

    return 0;
}

static int innogpu_get_temp_state(void *innoml_ctx, void *data)
{
	struct innoml_cdev *innoml_cdev = (struct innoml_cdev*)innoml_ctx;
	struct dev_rsrc *pdev_rsrc = fh2m_inno_rsrc_devres_find(innoml_cdev->pdev);
    InnomlTemperatureState *state = (InnomlTemperatureState*)data;

    state->overTemperatureState = fh2m_hal_bmc_get_val(pdev_rsrc->dev, BMC_OVERHEAT_STATUS);

    state->highTemperatureState = fh2m_hal_bmc_get_val(pdev_rsrc->dev, BMC_HIGH_TEMP_STATUS);

    state->boardTemperatureState = fh2m_hal_bmc_get_val(pdev_rsrc->dev, BMC_BOARD_TEMP_STATUS);

    state->boardFanState = fh2m_hal_bmc_get_val(pdev_rsrc->dev, BMC_BOARD_FAN_STATUS);

    return 0;
}

static int innogpu_get_clocks_info(void *innoml_ctx, void *data)
{
	struct innoml_cdev *innoml_cdev = (struct innoml_cdev*)innoml_ctx;
	struct dev_rsrc *pdev_rsrc = fh2m_inno_rsrc_devres_find(innoml_cdev->pdev);
    gpu_freqinfo_t *gfreqinfo = NULL;
    InnomlClocksInfo *info = (InnomlClocksInfo*)data;

    gfreqinfo = (gpu_freqinfo_t *)fh2m_hal_get_gpufreq_info(pdev_rsrc->dev);
    if (gfreqinfo) {
	    info->maxGpuClock = gfreqinfo->maxfreq * 1000;
	    info->minGpuClock = gfreqinfo->minfreq * 1000;
    } else {
        innoml_error("gfreqinfo is null, so and maxGpuClock and minGpuClock is invalid\n");
    }
    info->gpuClock = fh2m_hal_get_pll(pdev_rsrc->dev, PLL_GPU);
	info->gpuClock = info->gpuClock * 1000;

	info->memoryClock = fh2m_hal_bmc_get_val(pdev_rsrc->dev, BMC_MEM_SPEED);
	info->memoryClock = (info->memoryClock * 1000) / 2;

    info->videoClock = fh2m_hal_get_pll(pdev_rsrc->dev, PLL_VPU);
	info->videoClock = info->videoClock * 1000;

    return 0;
}

static void innogpu_get_dm_utilization(RGXFWIF_GPU_UTIL_STATS *psGpuUtilStats, IMG_UINT32 ui32DMtype, unsigned int *DMUtilization)
{
    IMG_UINT32 uiDivisor, util;
    IMG_UINT32 rem;
    uiDivisor = (IMG_UINT32)psGpuUtilStats->aaui64DMOSStatCumulative[ui32DMtype][0];
    if (uiDivisor == 0U)
    {
        *DMUtilization = 0;
    } else {
        util = 100 * psGpuUtilStats->aaui64DMOSStatActive[ui32DMtype][0];
        *DMUtilization = fh2m_inno_div(util, uiDivisor, &rem);
    }
}

/* copy from  _DebugStatusDIShow() */
static int innogpu_get_gpu_utilization(void *innoml_ctx, void *data)
{
	struct innoml_cdev *innoml_cdev = (struct innoml_cdev*)innoml_ctx;
    PVRSRV_DEVICE_NODE *psDeviceNode = NULL;
	PVRSRV_RGXDEV_INFO *psDevInfo = NULL;
    InnomlGpuUtilization *ml_util = (InnomlGpuUtilization*)data;
	uint32_t core_nums = fh2m_hal_get_gpu_core_nums();
	int i = 0;

	for (i = 0; i < core_nums; i++) {
		psDeviceNode = innoml_cdev->psDeviceNodeArray[i];
		psDevInfo = psDeviceNode->pvDevice;
		ml_util->gpuUtil[i].gpuUtilization = fh2m_hal_get_gpu_utils(psDeviceNode);
		innogpu_get_dm_utilization(&psDevInfo->sGpuUtilStats, RGXFWIF_DM_TDM, &ml_util->gpuUtil[i].tdmUtilization);
		innogpu_get_dm_utilization(&psDevInfo->sGpuUtilStats, RGXFWIF_DM_GEOM, &ml_util->gpuUtil[i].geomUtilization);
		innogpu_get_dm_utilization(&psDevInfo->sGpuUtilStats, RGXFWIF_DM_3D, &ml_util->gpuUtil[i].threedUtilization);
		innogpu_get_dm_utilization(&psDevInfo->sGpuUtilStats, RGXFWIF_DM_CDM, &ml_util->gpuUtil[i].cdmUtilization);
		innogpu_get_dm_utilization(&psDevInfo->sGpuUtilStats, RGXFWIF_DM_RAY, &ml_util->gpuUtil[i].rayUtilization);
		innogpu_get_dm_utilization(&psDevInfo->sGpuUtilStats, RGXFWIF_DM_GEOM2, &ml_util->gpuUtil[i].geom2Utilization);
	}

    return 0;
}

static int innogpu_get_voltage_info(void *innoml_ctx, void *data)
{
	struct innoml_cdev *innoml_cdev = (struct innoml_cdev*)innoml_ctx;
	struct dev_rsrc *pdev_rsrc = fh2m_inno_rsrc_devres_find(innoml_cdev->pdev);
    InnomlVoltageInfo *info = (InnomlVoltageInfo*)data;

    fh2m_hal_get_and_deal_bmc_info(pdev_rsrc->dev, (void *)BMC_GPU_VOLTAGE, &info->gpuVoltage);
	if (info->gpuVoltage == CHIP_INVALID_VAL)
		info->gpuVoltage = 0xFFFFFFFF;

    return 0;
}

static int innogpu_get_power_info(void *innoml_ctx, void *data)
{
	struct innoml_cdev *innoml_cdev = (struct innoml_cdev*)innoml_ctx;
	struct dev_rsrc *pdev_rsrc = fh2m_inno_rsrc_devres_find(innoml_cdev->pdev);
    InnomlPowerInfo *info = (InnomlPowerInfo*)data;

    fh2m_hal_get_and_deal_bmc_info(pdev_rsrc->dev, (void *)BMC_GPU_POWER, &info->gpuPower);
	if (info->gpuPower == CHIP_INVALID_VAL)
		info->gpuPower = 0xFFFFFFFF;

    return 0;
}

static int innogpu_get_vram_info(void *innoml_ctx, void *data)
{
	struct innoml_cdev *innoml_cdev = (struct innoml_cdev*)innoml_ctx;
	struct dev_rsrc *pdev_rsrc = fh2m_inno_rsrc_devres_find(innoml_cdev->pdev);
	uint32_t gpu_id = 0;
    InnomlVramInfo *info = (InnomlVramInfo*)data;
	uint32_t core_nums = fh2m_hal_get_gpu_core_nums();
	int ret = 0;
	struct hwinfo_item hwinfo_item;
	unsigned int mem_type = 0;
	static char* mem_types[] = {"unknown", "DDR3", "DDR4", "LPDDR4", "LPDDR4X", "DDR5", "LPDDR5", "GDDR5", "GDDR6", "GDDR6X", "LPDDR5X"};

	for (gpu_id = 0; gpu_id < core_nums; gpu_id++) {
        struct vram_stats gpu_stat;
		fh2m_hal_get_gpu_stat(pdev_rsrc->dev, gpu_id, &gpu_stat);
        info->vraminfo[gpu_id].totalSize = gpu_stat.total_size >> 20;
        info->vraminfo[gpu_id].free = gpu_stat.free_size >> 20;
		info->vraminfo[gpu_id].used = info->vraminfo[gpu_id].totalSize - info->vraminfo[gpu_id].free;
    }

	ret = fh2m_hal_get_mem_chip(pdev_rsrc->dev, info->name, sizeof(info->name));
	if (ret)
		fh2m_inno_memset(info->name, -1, sizeof(info->name));

	ret = fh2m_hal_hwinfo_get_item(pdev_rsrc->dev, HW_MEM_TYPE, &hwinfo_item);
	if (!ret)
		mem_type = hwinfo_item.val32;
	if (mem_type == 0 || (mem_type - 'A' + 1) >= INNO_ARRAY_SIZE(mem_types)) {
		fh2m_inno_strncpy(info->type, mem_types[0], sizeof(info->type));
	} else {
		fh2m_inno_strncpy(info->type, mem_types[mem_type - 'A' + 1], sizeof(info->type));
	}

	info->speed = fh2m_hal_bmc_get_val(pdev_rsrc->dev, BMC_MEM_SPEED);

	info->number = fh2m_hal_bmc_get_val(pdev_rsrc->dev, BMC_MEM_NUM);

	ret = fh2m_hal_hwinfo_get_item(pdev_rsrc->dev, HW_MEM_SIDE_FLAG, &hwinfo_item);
	if (!ret)
		info->sideFlag = hwinfo_item.val32;

	info->bitWidth = 32;
    return 0;
}

static int innogpu_get_vpu_mem_info(void *innoml_ctx, void *data)
{
	struct innoml_cdev *innoml_cdev = (struct innoml_cdev*)innoml_ctx;
	struct dev_rsrc *pdev_rsrc = fh2m_inno_rsrc_devres_find(innoml_cdev->pdev);
    InnomlVpuMemInfo *info = (InnomlVpuMemInfo*)data;
	int i, enc=0, dec=0;
	struct role_target role = { HAL_VRAM_ROLE_VPU, 0, 0 };
	struct vram_stats stats;

	fh2m_inno_memset(&stats, 0, sizeof(stats));
	fh2m_hal_get_vram_stats(pdev_rsrc->dev, &role, true, &stats);
	info->totalSize = stats.total_size >> 20;
	info->used = fh2m_inno_atomic64_read(&(pdev_rsrc->vpuinfo.vpu_mem_used)) >> 20;
	info->free = stats.free_size >> 20;

	for (i = 0; i < HAL_MAX_VPU_CHANS; i++) {
		if (fh2m_inno_atomic64_read(&(pdev_rsrc->vpuinfo.vpu_type[i])) == 1) {
			info->decUtil[dec] = fh2m_inno_atomic64_read(&(pdev_rsrc->vpuinfo.vpu_usage[i]));
			dec++;
		} else if (fh2m_inno_atomic64_read(&(pdev_rsrc->vpuinfo.vpu_type[i])) == 2) {
			info->encUtil[enc] = fh2m_inno_atomic64_read(&(pdev_rsrc->vpuinfo.vpu_usage[i]));
			enc++;
		}
	}

	info->decUtilNum = dec;
	info->encUtilNum = enc;

    return 0;
}

static int innogpu_set_clock(void *innoml_ctx, void *data) {
    int ret;
	struct innoml_cdev *innoml_cdev = (struct innoml_cdev*)innoml_ctx;
	struct dev_rsrc *pdev_rsrc = fh2m_inno_rsrc_devres_find(innoml_cdev->pdev);
    InnomlSetClockParams *params = (InnomlSetClockParams*)data;
    uint32_t pll;
    char name[10] = { 0 };
    gpu_freqinfo_t *gfreqinfo = NULL;

    gfreqinfo = (gpu_freqinfo_t *)fh2m_hal_get_gpufreq_info(pdev_rsrc->dev);
    if (gfreqinfo == NULL) {
        innoml_error("gfreqinfo is null, so and maxGpuClock and minGpuClock is invalid\n");
    }

    pll = params->clockRank;

    if (params->ip == INNO_GPU)
    {
        if((pdev_rsrc->chip_type == CHIP_G1P_SOC) || (pdev_rsrc->chip_type == CHIP_G1_SOC))
        {
            strcpy(name, "GPU");
        }
        else if((pdev_rsrc->chip_type == CHIP_G0_SOC) || (pdev_rsrc->chip_type == CHIP_G0M_SOC))
        {
            strcpy(name, "GPU_A");
        }
        else
        {
            innoml_error("Chip type is invalid!\n");
            return -EINVAL;
        }

		if (gfreqinfo != NULL && (pll < gfreqinfo->minfreq || pll > gfreqinfo->maxfreq))
		{
            innoml_error("set pll is invalid!\n");
            return -EINVAL;
		}
    }
    else if (params->ip == INNO_MEM)
    {
        innoml_error("Not support set memort clock!\n");
        return -EINVAL;
    }
    else if (params->ip == INNO_VIDEO)
    {
        strcpy(name, "VPU");
    }
    else
    {
        return -EINVAL;
    }
    ret = fh2m_hal_set_pll_by_name(pdev_rsrc->dev, name, pll);

    return ret;
}


static int innogpu_get_p2p_caps(void *innoml_ctx, void *data)
{
	struct innoml_cdev *innoml_cdev = (struct innoml_cdev*)innoml_ctx;
	struct dev_rsrc *pdev_rsrc = fh2m_inno_rsrc_devres_find(innoml_cdev->pdev);
    PVRSRV_DATA *psPVRSRVData;
    PVRSRV_DEVICE_NODE *psTargetDevicdNode;
    inno_pci_dev *psPcieDeviceNode;
    inno_pci_dev *psTargetPcieDeviceNode;
    InnomlP2PCaps *caps = (InnomlP2PCaps*)data;
    psPVRSRVData = fh2m_PVRSRVGetPVRSRVData();
    for (psTargetDevicdNode = psPVRSRVData->psDeviceNodeList;
                    psTargetDevicdNode != NULL;
                    psTargetDevicdNode = psTargetDevicdNode->psNext)
	{
        if(psTargetDevicdNode->sDevId.ui32InternalID == caps->targetGpuId)
        {
            break;
        }
    }

    psPcieDeviceNode = pdev_rsrc->pdev;
    psTargetPcieDeviceNode = fh2m_inno_to_pci_dev(fh2m_inno_dev_get_parent(psTargetDevicdNode->psDevConfig->pvOSDevice));

    if(psPcieDeviceNode == psTargetPcieDeviceNode)
    {
        caps->linkType = PATH_SELF;
    }
    else if(fh2m_inno_devices_under_same_switch(psPcieDeviceNode,psTargetPcieDeviceNode))
    {
        caps->linkType = PATH_PIX;
    }
    else if(fh2m_inno_devices_under_same_RC(psPcieDeviceNode,psTargetPcieDeviceNode))
    {
        caps->linkType = PATH_PHB;
    }
    else
    {
        caps->linkType = PATH_SYS;
    }
    return 0;
}

static int innogpu_get_pids(void *innoml_ctx, void *data)
{
	struct innoml_cdev *innoml_cdev = (struct innoml_cdev*)innoml_ctx;
	PVRSRV_DEVICE_NODE *psDeviceNode = NULL;
    InnomlPids *pids = (InnomlPids*)data;
    IMG_UINT32 ui32Index = 0;
	int i = 0, j = 0;
	bool isduplicate;
	uint32_t core_nums = fh2m_hal_get_gpu_core_nums();
	PDLLIST_NODE pNext = NULL, pNode = NULL;
    CONNECTION_DATA *sData = NULL;
    pids->count = 0;

	for (i = 0; i < core_nums; i++) {
		psDeviceNode = innoml_cdev->psDeviceNodeArray[i];
		if(!psDeviceNode->sConnections.psNextNode){
			return 0;
		}
		dllist_foreach_node(&psDeviceNode->sConnections, pNode, pNext)
		{
			sData = IMG_CONTAINER_OF(pNode, CONNECTION_DATA, sConnectionListNode);
			if (unlikely(pids->count >= MAX_PID_NUM)) {
				goto overflow;
			}
			isduplicate = 0;
			for (j = 0; j < pids->count; j++) {
				if (pids->pid[j] == sData->pid) {
					isduplicate = 1;
					break;
				}
			}
			if (!isduplicate) {
				pids->pid[ui32Index++] = sData->pid;
				pids->count++;
			}
		}
	}

    return 0;

overflow:
	innoml_error("pid nums overflow!\n");
    return -EOVERFLOW;
}

static int innogpu_get_pid_mem_usage(void *innoml_ctx, void *data)
{
    InnomlPidMemInfo *info = (InnomlPidMemInfo*)data;
    fh2m_GetMemInfoByPID(info->pid, info->pidName, &info->vramUsed, &info->gttUsed);
    return 0;
}

static int innogpu_get_power_state(void *innoml_ctx, void *data)
{
	struct innoml_cdev *innoml_cdev = (struct innoml_cdev*)innoml_ctx;
	PVRSRV_DEVICE_NODE *psDeviceNode = innoml_cdev->psDeviceNodeArray[0];
    PVRSRV_DEV_POWER_STATE ePowerState = -1;
    InnomlPowerState *state = (InnomlPowerState *)data;
    fh2m_PVRSRVGetDevicePowerState(psDeviceNode, &ePowerState);
    if (ePowerState == PVRSRV_DEV_POWER_STATE_ON)
        state->state = INNOGPU_POWER_STATE_ON;
    else if (ePowerState == PVRSRV_DEV_POWER_STATE_OFF)
        state->state = INNOGPU_POWER_STATE_OFF;
    else if (ePowerState == PVRSRV_DEV_POWER_STATE_DEFAULT)
        state->state = INNOGPU_POWER_STATE_DEFAULT;
    else
        state->state = INNOGPU_POWER_STATE_UNKNOW;
    return 0;
}
#if 0
static int innogpu_reset(void *innoml_ctx, void *data)
{
	struct innoml_cdev *innoml_cdev = (struct innoml_cdev*)innoml_ctx;
    InnomlResetGPUResult *result = (InnomlResetGPUResult *)data;
	PVRSRV_DEVICE_NODE *psDeviceNode = NULL;
	PVRSRV_RGXDEV_INFO *psDevInfo = NULL;
	RGXFWIF_SYSINIT *psFwSysInit = NULL;
	int i = 0;
	uint32_t core_nums = fh2m_hal_get_gpu_core_nums();

	for (i = 0; i < core_nums; i++) {
		psDeviceNode = innoml_cdev->psDeviceNodeArray[i];
		psDevInfo = psDeviceNode->pvDevice;
		psFwSysInit = psDevInfo->psRGXFWIfSysInit;
		if (!psFwSysInit){
			innoml_error("Firmware %d has not been loaded!\n", i);
			return -1;
		}

		result->status[i] = gpu_reload_firmware(psDeviceNode, psDevInfo);
	}

    return 0;
}
#endif
static const struct innogpu_ioctl_desc g_innogpu_ioctls[] =
{
    { INNOGPU_IOCTL_GET_VERSION, innogpu_get_version },
    { INNOGPU_IOCTL_GET_GPU_STATUS, innogpu_get_gpu_status },
    { INNOGPU_IOCTL_GET_PCI_INFO, innogpu_get_pci_info },
    { INNOGPU_IOCTL_GET_GPU_INFO, innogpu_get_gpu_info },
    { INNOGPU_IOCTL_GET_TEMP_INFO, innogpu_get_temp_info },
    { INNOGPU_IOCTL_GET_TEMP_STATE, innogpu_get_temp_state },
    { INNOGPU_IOCTL_GET_CLOCKS_INFO, innogpu_get_clocks_info },
    { INNOGPU_IOCTL_GET_GPU_UTILIZATION, innogpu_get_gpu_utilization },
    { INNOGPU_IOCTL_GET_VOLTAGE_INFO, innogpu_get_voltage_info },
    { INNOGPU_IOCTL_GET_POWER_INFO, innogpu_get_power_info },
    { INNOGPU_IOCTL_GET_VRAM_INFO, innogpu_get_vram_info },
    { INNOGPU_IOCTL_GET_VPU_MEM_INFO, innogpu_get_vpu_mem_info },
    { INNOGPU_IOCTL_SET_CLOCK, innogpu_set_clock },
    { INNOGPU_IOCTL_GET_P2P_CAPS, innogpu_get_p2p_caps },
    { INNOGPU_IOCTL_GET_PIDS, innogpu_get_pids },
    { INNOGPU_IOCTL_GET_PID_MEM_USAGE, innogpu_get_pid_mem_usage },
    { INNOGPU_IOCTL_GET_POWER_STATE, innogpu_get_power_state },
    //{ INNOGPU_IOCTL_RESET,innogpu_reset},
    { INNOGPU_IOCTL_GET_IOCTL_VERSION, innogpu_get_ioctl_version },
};

static int g_num_ioctls = ARRAY_SIZE(g_innogpu_ioctls);

static long innogpu_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int i;
    unsigned int in_size, out_size, ksize;
	struct innoml_cdev *innoml_cdev = file->private_data;
    char *kdata = NULL;
    int retcode = -EINVAL;
    char stack_kdata[128] = {0};

    out_size = in_size = INNOGPU_IOC_SIZE(cmd);
    if ((cmd & INNOGPU_IOC_IN) == 0)
		in_size = 0;
	if ((cmd & INNOGPU_IOC_OUT) == 0)
		out_size = 0;
    ksize = max(in_size, out_size);

    if (ksize <= sizeof(stack_kdata)) {
        kdata = stack_kdata;
    } else {
        kdata = fh2m_inno_kzalloc_kernel(ksize);
        if (kdata == NULL) {
            retcode = -ENOMEM;
            goto err;
        }
    }

    if (in_size != 0) {
        if (copy_from_user(kdata, (void __user *)arg, in_size) != 0) {
		    retcode = -ENOMEM;
		    goto err;
        }
    }

    for (i = 0; i < g_num_ioctls; i++) {
        if (cmd == g_innogpu_ioctls[i].cmd) {
            retcode = g_innogpu_ioctls[i].func(innoml_cdev, kdata);
            break;
        }
    }

    if (i == g_num_ioctls) {
        innoml_error("invalid ioctl: pid=%d, cmd=0x%02x, nr=0x%02x\n", task_pid_nr(current), cmd, _IOC_NR(cmd));
        goto err;
    }

    if (out_size != 0) {
        if (copy_to_user((void __user *)arg, kdata, out_size) != 0) {
		    retcode = -ENOMEM;
		    goto err;
        }
    }
err:
    if (kdata != stack_kdata)
		fh2m_inno_kfree(kdata);
    return retcode;
}

static struct file_operations innogpu_fops = {
    .owner = THIS_MODULE,
    .open = inno_open,
    .unlocked_ioctl = innogpu_ioctl,
	.compat_ioctl = innogpu_ioctl,
};

int InnoGpuCharDevInitDriver(void)
{
    int err;
    dev_t devno;
    err = alloc_chrdev_region(&devno, 0, INNOGPU_MAX_DEV_NUM, INNOGPU_ML_DEVICE);

    if (err < 0) {
        innoml_error("Failed to allocate device numbers\n");
        return err;
    }

    g_major = MAJOR(devno);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    g_innogpu_class = class_create(INNOGPU_CLASS);
#else
    g_innogpu_class = class_create(THIS_MODULE, INNOGPU_CLASS);
#endif
    if (IS_ERR(g_innogpu_class)) {
        unregister_chrdev_region(MKDEV(g_major, 0), INNOGPU_MAX_DEV_NUM);
        return PTR_ERR(g_innogpu_class);
    }
    return 0;
}

void InnoGpuCharDevDeInitDriver(void)
{
    class_destroy(g_innogpu_class);
    unregister_chrdev_region(MKDEV(g_major, 0), INNOGPU_MAX_DEV_NUM);
}

int InnoGpuCharDevInitDevice(inno_dev *pdev)
{
    int minor_id;
	struct dev_rsrc *pdev_rsrc = fh2m_inno_rsrc_devres_find(pdev);
    minor_id = pdev_rsrc->pcie_func_idx;

	g_innogpu_cdev[minor_id].pdev = pdev;
    cdev_init(&g_innogpu_cdev[minor_id].inno_cdev, &innogpu_fops);
    g_innogpu_cdev[minor_id].inno_cdev.owner = THIS_MODULE;
    if (cdev_add(&g_innogpu_cdev[minor_id].inno_cdev, MKDEV(g_major, minor_id), 1) < 0) {
        innoml_error("Failed to add cdev");
        return -1;
    }

    device_create(g_innogpu_class, NULL, MKDEV(g_major, minor_id), NULL, INNOGPU_DEVICE_NAME"%d", minor_id);
    return 0;
}

void InnoGpuCharDevDeInitDevice(inno_dev *pdev)
{
    int minor_id;
	struct dev_rsrc *pdev_rsrc = fh2m_inno_rsrc_devres_find(pdev);
    minor_id = pdev_rsrc->pcie_func_idx;

    device_destroy(g_innogpu_class, MKDEV(g_major, minor_id));
    cdev_del(&g_innogpu_cdev[minor_id].inno_cdev);
}
