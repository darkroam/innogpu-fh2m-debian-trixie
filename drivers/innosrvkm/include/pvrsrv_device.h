/**************************************************************************/ /*!
@File
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
                Additional changes in this file are
                Copyright (c) Innosilicon Technology Ltd. All Rights Reserved
@License        Dual MIT/GPLv2

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
*/ /***************************************************************************/

#ifndef PVRSRV_DEVICE_H
#define PVRSRV_DEVICE_H

#include "img_types.h"
#include "physheap_config.h"
#include "pvrsrv_error.h"
#include "pvrsrv_memalloc_physheap.h"
#include "rgx_firmware_boot.h"
#include "rgx_fwif_km.h"
#include "servicesext.h"
#include "cache_ops.h"
#include "opaque_types.h"

#if defined(SUPPORT_LINUX_DVFS) || defined(SUPPORT_PDVFS)
#include "pvr_dvfs.h"
#endif

#if defined(SUPPORT_GPUVIRT_VALIDATION)
#include "virt_validation_defs.h"
#endif

typedef struct _PVRSRV_DEVICE_CONFIG_ PVRSRV_DEVICE_CONFIG;
typedef enum _DRIVER_MODE_
{
/* Do not use these enumerations directly, to query the
   current driver mode, use the PVRSRV_VZ_MODE_IS()
   macro */
	DRIVER_MODE_NATIVE	= -1,
	DRIVER_MODE_HOST	=  0,
	DRIVER_MODE_GUEST
} PVRSRV_DRIVER_MODE;

typedef enum
{
	PVRSRV_DEVICE_LOCAL_MEMORY_ARENA_MAPPABLE = 0,
	PVRSRV_DEVICE_LOCAL_MEMORY_ARENA_NON_MAPPABLE = 1,
	PVRSRV_DEVICE_LOCAL_MEMORY_ARENA_LAST
} PVRSRV_DEVICE_LOCAL_MEMORY_ARENA;

typedef enum _PVRSRV_DEVICE_SNOOP_MODE_
{
	PVRSRV_DEVICE_SNOOP_NONE = 0,
	PVRSRV_DEVICE_SNOOP_CPU_ONLY,
	PVRSRV_DEVICE_SNOOP_DEVICE_ONLY,
	PVRSRV_DEVICE_SNOOP_CROSS,
	PVRSRV_DEVICE_SNOOP_EMULATED,
} PVRSRV_DEVICE_SNOOP_MODE;

#if defined(SUPPORT_SOC_TIMER)
typedef IMG_UINT64
(*PFN_SYS_DEV_SOC_TIMER_READ)(IMG_HANDLE hSysData);
#endif

typedef IMG_UINT64
(*PFN_SYS_DEV_SOC_GTT_DEV_BASE_GET)(PVRSRV_DEVICE_CONFIG*);

typedef IMG_UINT64
(*PFN_SYS_DEV_SOC_CPU_PA_TO_GTT_PA)(PVRSRV_DEVICE_CONFIG*, IMG_UINT64);

typedef IMG_UINT64
(*PFN_SYS_DEV_SOC_GTT_PA_TO_CPU_PA)(PVRSRV_DEVICE_CONFIG*, IMG_UINT64);

typedef IMG_BOOL
(*PFN_SLAVE_DEV_SOC_GTT_SORT_ENABLE)(void); 

typedef enum _PVRSRV_DEVICE_FABRIC_TYPE_
{
	PVRSRV_DEVICE_FABRIC_NONE = 0,
	PVRSRV_DEVICE_FABRIC_ACELITE,
	PVRSRV_DEVICE_FABRIC_FULLACE,
} PVRSRV_DEVICE_FABRIC_TYPE;

typedef enum _PVRSRV_DEVICE_RAW_INTR_
{
	PVRSRV_DEVICE_RAW_VCODEC = 0,
	PVRSRV_DEVICE_RAW_GPU = 1,
	PVRSRV_DEVICE_RAW_PCIE = 2,
	PVRSRV_DEVICE_RAW_INTR_MAX,
} PVRSRV_DEVICE_RAW_INTR;

typedef enum _PVRSRV_DEVICE_RAW_ACTION_
{
	PVRSRV_DEVICE_RAW_R = 0,
	PVRSRV_DEVICE_RAW_W = 1,
	PVRSRV_DEVICE_RAW_RW = 2,
	PVRSRV_DEVICE_ACTION_MAX,
} PVRSRV_DEVICE_RAW_ACTION;

typedef IMG_UINT32
(*PFN_SYS_DEV_CLK_FREQ_GET)(IMG_HANDLE hSysData);

typedef PVRSRV_ERROR
(*PFN_SYS_PRE_POWER)(IMG_HANDLE hSysData,
						 PVRSRV_SYS_POWER_STATE eNewPowerState,
						 PVRSRV_SYS_POWER_STATE eCurrentPowerState,
						 PVRSRV_POWER_FLAGS ePwrFlags);

typedef PVRSRV_ERROR
(*PFN_SYS_POST_POWER)(IMG_HANDLE hSysData,
						  PVRSRV_SYS_POWER_STATE eNewPowerState,
						  PVRSRV_SYS_POWER_STATE eCurrentPowerState,
						  PVRSRV_POWER_FLAGS ePwrFlags);

/*************************************************************************/ /*!
@Brief          Callback function type PFN_SYS_GET_POWER

@Description    This function queries the SoC power registers to determine
                if the power domain on which the GPU resides is powered on.

   Implementation of this callback is optional - where it is not provided,
   the driver will assume the domain power state depending on driver type:
   regular drivers assume it is unpowered at startup, while drivers with
   AutoVz support expect the GPU domain to be powered on initially. The power
   state will be then tracked internally according to the pfnPrePowerState
   and pfnPostPowerState calls using a fallback function.

@Input          psDevNode                  Pointer to node struct of the
                                           device being initialised

@Return         PVRSRV_SYS_POWER_STATE_ON  if the respective device's hardware
                                           domain is powered on
                PVRSRV_SYS_POWER_STATE_OFF if the domain is powered off
*/ /**************************************************************************/
typedef PVRSRV_SYS_POWER_STATE
(*PFN_SYS_GET_POWER)(PPVRSRV_DEVICE_NODE psDevNode);

typedef void
(*PFN_SYS_DEV_INTERRUPT_HANDLED)(PVRSRV_DEVICE_CONFIG *psDevConfig);

typedef PVRSRV_ERROR
(*PFN_SYS_DEV_CHECK_MEM_ALLOC_SIZE)(IMG_HANDLE hSysData,
									IMG_UINT64 ui64MemSize);

typedef void (*PFN_SYS_DEV_FEAT_DEP_INIT)(PVRSRV_DEVICE_CONFIG *, IMG_UINT64);

typedef void
(*PFN_SYS_DEV_HOST_CACHE_MAINTENANCE)(IMG_HANDLE hSysData,
									PVRSRV_CACHE_OP eRequestType,
									void *pvVirtStart,
									void *pvVirtEnd,
									IMG_CPU_PHYADDR sCPUPhysStart,
									IMG_CPU_PHYADDR sCPUPhysEnd);

typedef void*
(*PFN_SLAVE_DMA_CHAN)(PVRSRV_DEVICE_CONFIG*, char*, IMG_INT);

typedef void
(*PFN_SLAVE_DMA_FREE)(PVRSRV_DEVICE_CONFIG*,
					  void*);

typedef enum _PVRSRV_DMA_DIR_
{
	PVRSRV_DMA_MEM2DEV = 0,
	PVRSRV_DMA_DEV2MEM,
	PVRSRV_DMA_DEV2DEV,
	PVRSRV_DMA_MEM2MEM,
}PVRSRV_DMA_DIR;



typedef void*
(*PFN_DMA_MAP)(PVRSRV_DEVICE_CONFIG*, IMG_BOOL, void*, IMG_UINT32);

typedef void
(*PFN_DMA_UNMAP)(PVRSRV_DEVICE_CONFIG*, void*);

typedef void
(*PFN_DMA_CALLBACK)(void*);

typedef PVRSRV_ERROR
(*PFN_DMA_TRANSFER)(PVRSRV_DEVICE_CONFIG*, void**, void**, IMG_INT32*, IMG_UINT32, IMG_INT32, PFN_DMA_CALLBACK, PFN_DMA_CALLBACK, void*);

typedef IMG_INT
(*PFN_SLAVE_DMA_MEMSET)(PVRSRV_DEVICE_CONFIG*, void*, IMG_INT, IMG_INT, IMG_BOOL);

typedef IMG_BOOL
(*PFN_SLAVE_DEV_ADDR_IS_VIS)(PVRSRV_DEVICE_CONFIG*, IMG_UINT64);

typedef void
(*PFN_SLAVE_SYNC_RAW_INTR)(PVRSRV_DEVICE_CONFIG*,
					  int,
					  int);
typedef int
(*PFN_SLAVE_SYNC_RAW_SET)(PVRSRV_DEVICE_CONFIG*,
						uint64_t);

typedef bool
(*PFN_SLAVE_RAW_ENABLED)(PVRSRV_DEVICE_CONFIG*);

typedef void
(*PFN_DEV_PHY_ADDR_2_DMA_ADDR)(PVRSRV_DEVICE_CONFIG *,
							   IMG_DMA_ADDR *,
							   IMG_DEV_PHYADDR *,
							   IMG_BOOL *,
							   IMG_UINT32,
							   IMG_BOOL);


#if defined(SUPPORT_TRUSTED_DEVICE)

typedef struct _PVRSRV_TD_FW_PARAMS_
{
	const void *pvFirmware;
	IMG_UINT32 ui32FirmwareSize;
	PVRSRV_FW_BOOT_PARAMS uFWP;
} PVRSRV_TD_FW_PARAMS;

typedef PVRSRV_ERROR
(*PFN_TD_SEND_FW_IMAGE)(IMG_HANDLE hSysData,
						PVRSRV_TD_FW_PARAMS *psTDFWParams);

typedef struct _PVRSRV_TD_POWER_PARAMS_
{
	IMG_DEV_PHYADDR sPCAddr;

	/* MIPS-only fields */
	IMG_DEV_PHYADDR sGPURegAddr;
	IMG_DEV_PHYADDR sBootRemapAddr;
	IMG_DEV_PHYADDR sCodeRemapAddr;
	IMG_DEV_PHYADDR sDataRemapAddr;
} PVRSRV_TD_POWER_PARAMS;

typedef PVRSRV_ERROR
(*PFN_TD_SET_POWER_PARAMS)(IMG_HANDLE hSysData,
						   PVRSRV_TD_POWER_PARAMS *psTDPowerParams);

typedef PVRSRV_ERROR
(*PFN_TD_RGXSTART)(IMG_HANDLE hSysData);

typedef PVRSRV_ERROR
(*PFN_TD_RGXSTOP)(IMG_HANDLE hSysData);

#endif /* defined(SUPPORT_TRUSTED_DEVICE) */

#if defined(SUPPORT_GPUVIRT_VALIDATION)
typedef void (*PFN_SYS_DEV_VIRT_INIT)(IMG_HANDLE hSysData,
                                      IMG_UINT64[GPUVIRT_VALIDATION_NUM_REGIONS][GPUVIRT_VALIDATION_NUM_OS],
                                      IMG_UINT64[GPUVIRT_VALIDATION_NUM_REGIONS][GPUVIRT_VALIDATION_NUM_OS]);
#endif /* defined(SUPPORT_GPUVIRT_VALIDATION) */

typedef struct _PVRSRV_ROBUSTNESS_ERR_DATA_HOST_WDG_
{
	IMG_UINT32 ui32Status;     /*!< FW status */
	IMG_UINT32 ui32Reason;     /*!< Reason for FW status */
} PVRSRV_ROBUSTNESS_ERR_DATA_HOST_WDG;

typedef struct _PVRSRV_ROBUSTNESS_ERR_DATA_FW_PF_
{
	IMG_DEV_VIRTADDR sFWFaultAddr;     /*!< FW page fault address */
} PVRSRV_ROBUSTNESS_ERR_DATA_FW_PF;

typedef struct _PVRSRV_ROBUSTNESS_ERR_DATA_CHECKSUM_
{
	IMG_UINT32 ui32ExtJobRef;     /*!< External Job Reference of any affected GPU work */
	RGXFWIF_DM eDM;               /*!< Data Master which was running any affected GPU work */
} PVRSRV_ROBUSTNESS_ERR_DATA_CHECKSUM;

typedef struct _PVRSRV_ROBUSTNESS_NOTIFY_DATA_
{
	RGX_CONTEXT_RESET_REASON eResetReason; /*!< Reason for error/reset */
	IMG_PID                  pid;          /*!< Pid of process which created the errored context */
	union
	{
		PVRSRV_ROBUSTNESS_ERR_DATA_CHECKSUM sChecksumErrData; /*!< Data returned for checksum errors */
		PVRSRV_ROBUSTNESS_ERR_DATA_FW_PF    sFwPFErrData;     /*!< Data returned for FW page faults */
		PVRSRV_ROBUSTNESS_ERR_DATA_HOST_WDG sHostWdgData;     /*!< Data returned for Host Wdg FW faults */
	} uErrData;
} PVRSRV_ROBUSTNESS_NOTIFY_DATA;

typedef void
(*PFN_SYS_DEV_ERROR_NOTIFY)(IMG_HANDLE hSysData,
						    PVRSRV_ROBUSTNESS_NOTIFY_DATA *psRobustnessErrorData);

struct _PVRSRV_DEVICE_CONFIG_
{
	/*! OS device passed to SysDevInit (linux: 'struct device') */
	void *pvOSDevice;

	/*!
	 *! Service representation of pvOSDevice. Should be set to NULL when the
	 *! config is created in SysDevInit. Set by Services once a device node has
	 *! been created for this config and unset before SysDevDeInit is called.
	 */
	struct _PVRSRV_DEVICE_NODE_ *psDevNode;

	/*! Name of the device */
	IMG_CHAR *pszName;

	/*! Version of the device (optional) */
	IMG_CHAR *pszVersion;

	/*! Register bank address */
	IMG_CPU_PHYADDR sRegsCpuPBase;
	/*! Register bank size */
	IMG_UINT32 ui32RegsSize;
	/*! Device interrupt number */
	IMG_UINT32 ui32IRQ;
	/*! Name of the card*/
	IMG_CHAR *chipName;
	/*! Alias of the card*/
	IMG_CHAR *chipAlias;
	/* Enable low power use 1 spu */
	IMG_BOOL bLowPowerEnable;

	IMG_UINT32 ui32DeviceID;
	IMG_UINT32 ui32SPUNum;
	IMG_UINT32 ui32ClustersNum;
	IMG_UINT32 ui32SPU1Clusters;
	IMG_UINT32 ui32TPUBufLookUpSize;
	IMG_UINT32 ui32SGLMaxTextureBufSize;
	IMG_UINT32 ui32GLESMaxTextureBufSize;

	PVRSRV_DEVICE_SNOOP_MODE eCacheSnoopingMode;

	/*! Device specific data handle */
	IMG_HANDLE hDevData;

	/*! System specific data that gets passed into system callback functions. */
	IMG_HANDLE hSysData;

	IMG_BOOL bHasNonMappableLocalMemory;

	/*! Indicates if system supports FBCDC v3.1 */
	IMG_BOOL bHasFBCDCVersion31;

	/*! Physical Heap definitions for this device.
	 * eDefaultHeap must be set to GPU_LOCAL or CPU_LOCAL. Specifying any other value
	 *    (e.g. DEFAULT) will lead to an error at device discovery.
	 * pasPhysHeap array must contain at least one PhysHeap, the declared default heap.
	 */
	PVRSRV_PHYS_HEAP  eDefaultHeap;
	PHYS_HEAP_CONFIG *pasPhysHeaps;
	IMG_UINT32 ui32PhysHeapCount;
	IMG_UINT32 ui32VPUPhysHeapCount;
	IMG_UINT32 ui32VPUPhysHeapBase;
	IMG_UINT32 ui32DPUPhysHeapCount;
	IMG_UINT32 ui32DPUPhysHeapBase;

	/*!
	 *! Callbacks to change system device power state at the beginning and end
	 *! of a power state change (optional).
	 */
	PFN_SYS_PRE_POWER pfnPrePowerState;
	PFN_SYS_POST_POWER pfnPostPowerState;
	PFN_SYS_GET_POWER  pfnGpuDomainPower;

	/*! Callback to obtain the clock frequency from the device (optional). */
	PFN_SYS_DEV_CLK_FREQ_GET pfnClockFreqGet;

#if defined(SUPPORT_SOC_TIMER)
	/*! Callback to read SoC timer register value (mandatory). */
	PFN_SYS_DEV_SOC_TIMER_READ	pfnSoCTimerRead;
#endif

	/*!
	 *! Callback to perform host CPU cache maintenance. Might be needed for
	 *! architectures which allow extensions such as RISC-V (optional).
	 */
	PFN_SYS_DEV_HOST_CACHE_MAINTENANCE pfnHostCacheMaintenance;
	IMG_BOOL bHasPhysicalCacheMaintenance;

#if defined(SUPPORT_TRUSTED_DEVICE)
	/*!
	 *! Callback to send FW image and FW boot time parameters to the trusted
	 *! device.
	 */
	PFN_TD_SEND_FW_IMAGE pfnTDSendFWImage;

	/*!
	 *! Callback to send parameters needed in a power transition to the trusted
	 *! device.
	 */
	PFN_TD_SET_POWER_PARAMS pfnTDSetPowerParams;

	/*! Callbacks to ping the trusted device to securely run RGXStart/Stop() */
	PFN_TD_RGXSTART pfnTDRGXStart;
	PFN_TD_RGXSTOP pfnTDRGXStop;

#if defined(PVR_ANDROID_HAS_DMA_HEAP_FIND)
	/*! Name of DMA heap to allocate secure memory from. Used with dma_heap_find. */
	IMG_CHAR *pszSecureDMAHeapName;
#endif
#endif /* defined(SUPPORT_TRUSTED_DEVICE) */

	/*! Function that does device feature specific system layer initialisation */
	PFN_SYS_DEV_FEAT_DEP_INIT	pfnSysDevFeatureDepInit;

#if defined(SUPPORT_LINUX_DVFS) || defined(SUPPORT_PDVFS)
	PVRSRV_DVFS sDVFS;
#endif

#if defined(SUPPORT_ALT_REGBASE)
	IMG_DEV_PHYADDR sAltRegsGpuPBase;
#endif

	/*!
	 *! Indicates if device physical address 0x0 might be used as GPU memory
	 *! (e.g. LMA system or UMA system with CPU PA 0x0 reserved by the OS,
	 *!  but CPU PA != device PA and device PA 0x0 available for the GPU)
	 */
	IMG_BOOL bDevicePA0IsValid;

	/*!
	 *! Function to initialize System-specific virtualization. If not supported
	 *! this should be a NULL reference. Only present if
	 *! SUPPORT_GPUVIRT_VALIDATION is defined.
	 */
#if defined(SUPPORT_GPUVIRT_VALIDATION)
	PFN_SYS_DEV_VIRT_INIT		pfnSysDevVirtInit;
#endif

	/*!
	 *! Callback to notify system layer of device errors.
	 *! NB. implementers should ensure that the minimal amount of work is
	 *! done in the callback function, as it will be executed in the main
	 *! RGX MISR. (e.g. any blocking or lengthy work should be performed by
	 *! a worker queue/thread instead.)
	 */
	PFN_SYS_DEV_ERROR_NOTIFY	pfnSysDevErrorNotify;

	/*!
	 *!  Slave PCIE DMA channel request callbacks
	 */
	PFN_SLAVE_DMA_CHAN pfnSlaveDMAGetChan;
	PFN_SLAVE_DMA_FREE pfnSlaveDMAFreeChan;



	/*!
	 *!  DMA map callbacks
	 */
	PFN_DMA_MAP pfnDmaMap;

	/*!
	 *!  DMA unmap callbacks
	 */
	PFN_DMA_UNMAP pfnDmaUnMap;

	/*!
	 *!  DMA transfer callbacks
	 */
	PFN_DMA_TRANSFER pfnDmaTransfer;

	/*!
	 *!  DMA memset callbacks
	 */
	PFN_SLAVE_DMA_MEMSET pfnDmaMemset;

	PFN_SLAVE_DEV_ADDR_IS_VIS pfnDevAddrIsVisble;

	/*!
	 *! Slave RAW callbacks
	 */
	PFN_SLAVE_SYNC_RAW_INTR pfnSyncRawIntr;
	PFN_SLAVE_SYNC_RAW_SET pfnSetRawAddr;
	PFN_SLAVE_RAW_ENABLED pfnRawEnabled;

	PFN_SYS_DEV_SOC_GTT_DEV_BASE_GET pfnGetGttDevBase;
	PFN_SYS_DEV_SOC_CPU_PA_TO_GTT_PA pfnCpuPaToGttPa;
	PFN_SYS_DEV_SOC_GTT_PA_TO_CPU_PA pfnGttPaToCpuPa;
	IMG_BOOL bISGttNeedFallBack;
	PFN_SLAVE_DEV_SOC_GTT_SORT_ENABLE pfnGttSortEn;

	/*!
	 *!  Conversion of device memory to DMA addresses
	 */
	PFN_DEV_PHY_ADDR_2_DMA_ADDR pfnDevPhysAddr2DmaAddr;
	/*!
	 *!  Conversion of device memory to AXI DMA addresses
	 */
	PFN_DEV_PHY_ADDR_2_DMA_ADDR pfnDevPhysAddr2AXIDmaAddr;
	/*!
	 *!  DMA channel count
	 */
	IMG_UINT32 uiDmaChanCount;
	/*!
	 *!  DMA channel names
	 */
	IMG_CHAR *pszDmaTxChanName;
	IMG_CHAR *pszDmaRxChanName;
	IMG_CHAR *pszAXIDmaChanName;
	/*!
	 *!  DMA device transfer restrictions
	 */
	IMG_UINT32 ui32DmaAlignment;
	IMG_UINT32 ui32DmaTransferUnit;

	IMG_UINT32 ui32AXIDmaAlignment;
	IMG_UINT32 ui32AXIDmaTransferUnit;
	/*!
	 *!  System-wide presence of DMA capabilities
	 */
	IMG_BOOL bHasDma;
	IMG_BOOL bHasInvMem;
	IMG_BOOL bHasGttMem;
	IMG_BOOL bHasLimitedGttMem;
	IMG_BOOL bSupportDmaMemset;
	IMG_BOOL bShareVPUToDPU;
	IMG_BOOL bINVShareVPUToDPU;
	void *hPCIeResource;
};

#endif /* PVRSRV_DEVICE_H*/
