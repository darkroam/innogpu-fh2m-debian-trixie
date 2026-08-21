/*************************************************************************/ /*!
@File           km_apphint.h
@Title          Apphint internal header
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Linux kernel AppHint control
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
*/ /**************************************************************************/

#ifndef KM_APPHINT_H
#define KM_APPHINT_H

#if defined(__cplusplus)
extern "C" {
#endif

#include "pvrsrv_apphint.h"
#include "km_apphint_defs.h"
#include "device.h"

#define APPHINT_DEVICES_MAX 32
/*
*******************************************************************************
 Data types
******************************************************************************/
union apphint_value {
	IMG_UINT64 UINT64;
	IMG_UINT32 UINT32;
	IMG_BOOL BOOL;
	IMG_CHAR *STRING;
};

union apphint_query_action {
	PVRSRV_ERROR (*UINT64)(const PVRSRV_DEVICE_NODE *device,
	                       const void *private_data, IMG_UINT64 *value);
	PVRSRV_ERROR (*UINT32)(const PVRSRV_DEVICE_NODE *device,
	                       const void *private_data, IMG_UINT32 *value);
	PVRSRV_ERROR (*BOOL)(const PVRSRV_DEVICE_NODE *device,
	                     const void *private_data, IMG_BOOL *value);
	PVRSRV_ERROR (*STRING)(const PVRSRV_DEVICE_NODE *device,
	                       const void *private_data, IMG_CHAR **value);
};

union apphint_set_action {
	PVRSRV_ERROR (*UINT64)(const PVRSRV_DEVICE_NODE *device,
	                       const void *private_data, IMG_UINT64 value);
	PVRSRV_ERROR (*UINT32)(const PVRSRV_DEVICE_NODE *device,
	                       const void *private_data, IMG_UINT32 value);
	PVRSRV_ERROR (*BOOL)(const PVRSRV_DEVICE_NODE *device,
	                     const void *private_data, IMG_BOOL value);
	PVRSRV_ERROR (*STRING)(const PVRSRV_DEVICE_NODE *device,
	                       const void *private_data, IMG_CHAR *value);
};

struct apphint_action {
	union apphint_query_action query; /*!< Query callbacks. */
	union apphint_set_action set;     /*!< Set callbacks. */
	const PVRSRV_DEVICE_NODE *device; /*!< Pointer to the device node.*/
	const void *private_data;         /*!< Opaque data passed to `query` and
	                                       `set` callbacks. */
	union apphint_value stored;       /*!< Value of the AppHint. */
	bool free;                        /*!< Flag indicating that memory has been
	                                       allocated for this AppHint and it
	                                       needs to be freed on deinit. */
	bool initialised;                 /*!< Flag indicating if the AppHint has
	                                       been already initialised. */
};

struct apphint_state
{
	struct workqueue_struct *workqueue;
	DI_GROUP *debuginfo_device_rootdir[PVRSRV_MAX_DEVICES];
	DI_ENTRY *debuginfo_device_entry[PVRSRV_MAX_DEVICES][APPHINT_DEBUGINFO_DEVICE_ID_MAX];
	DI_GROUP *debuginfo_rootdir;
	DI_ENTRY *debuginfo_entry[APPHINT_DEBUGINFO_ID_MAX];
	DI_GROUP *buildvar_rootdir;
	DI_ENTRY *buildvar_entry[APPHINT_BUILDVAR_ID_MAX];

	unsigned int num_devices;
	PVRSRV_DEVICE_NODE *devices[PVRSRV_MAX_DEVICES];
	unsigned int initialized;

	/* Array contains value space for 1 copy of all apphint values defined
	 * (for device 1) and N copies of device specific apphint values for
	 * multi-device platforms.
	 */
	struct apphint_action val[APPHINT_ID_MAX + ((PVRSRV_MAX_DEVICES-1)*APPHINT_DEBUGINFO_DEVICE_ID_MAX)];

} ;

int pvr_apphint_init(void);
void pvr_apphint_deinit(void);
int pvr_apphint_device_register(PVRSRV_DEVICE_NODE *device);
void pvr_apphint_device_unregister(PVRSRV_DEVICE_NODE *device);
void pvr_apphint_dump_state(PVRSRV_DEVICE_NODE *device);

int pvr_apphint_get_uint64(PVRSRV_DEVICE_NODE *device, APPHINT_ID ue, IMG_UINT64 *pVal);
int pvr_apphint_get_uint32(PVRSRV_DEVICE_NODE *device, APPHINT_ID ue, IMG_UINT32 *pVal);
int pvr_apphint_get_bool(PVRSRV_DEVICE_NODE *device, APPHINT_ID ue, IMG_BOOL *pVal);
int pvr_apphint_get_string(PVRSRV_DEVICE_NODE *device, APPHINT_ID ue, IMG_CHAR *pBuffer, size_t size);

int pvr_apphint_set_uint64(PVRSRV_DEVICE_NODE *device, APPHINT_ID ue, IMG_UINT64 Val);
int pvr_apphint_set_uint32(PVRSRV_DEVICE_NODE *device, APPHINT_ID ue, IMG_UINT32 Val);
int pvr_apphint_set_bool(PVRSRV_DEVICE_NODE *device, APPHINT_ID ue, IMG_BOOL Val);
int pvr_apphint_set_string(PVRSRV_DEVICE_NODE *device, APPHINT_ID ue, IMG_CHAR *pBuffer, size_t size);

void pvr_apphint_register_handlers_uint64(APPHINT_ID id,
	PVRSRV_ERROR (*query)(const PVRSRV_DEVICE_NODE *device, const void *private_data, IMG_UINT64 *value),
	PVRSRV_ERROR (*set)(const PVRSRV_DEVICE_NODE *device, const void *private_data, IMG_UINT64 value),
	const PVRSRV_DEVICE_NODE *device,
	const void * private_data);
void pvr_apphint_register_handlers_uint32(APPHINT_ID id,
	PVRSRV_ERROR (*query)(const PVRSRV_DEVICE_NODE *device, const void *private_data, IMG_UINT32 *value),
	PVRSRV_ERROR (*set)(const PVRSRV_DEVICE_NODE *device, const void *private_data, IMG_UINT32 value),
	const PVRSRV_DEVICE_NODE *device,
	const void *private_data);
void pvr_apphint_register_handlers_bool(APPHINT_ID id,
	PVRSRV_ERROR (*query)(const PVRSRV_DEVICE_NODE *device, const void *private_data, IMG_BOOL *value),
	PVRSRV_ERROR (*set)(const PVRSRV_DEVICE_NODE *device, const void *private_data, IMG_BOOL value),
	const PVRSRV_DEVICE_NODE *device,
	const void *private_data);
void pvr_apphint_register_handlers_string(APPHINT_ID id,
	PVRSRV_ERROR (*query)(const PVRSRV_DEVICE_NODE *device, const void *private_data, IMG_CHAR **value),
	PVRSRV_ERROR (*set)(const PVRSRV_DEVICE_NODE *device, const void *private_data, IMG_CHAR *value),
	const PVRSRV_DEVICE_NODE *device,
	const void *private_data);
int apphint_kparam_set(const char *val, void *arg);
int apphint_kparam_get(char *buffer, void *arg);
#if defined(__cplusplus)
}
#endif
#endif /* KM_APPHINT_H */

/******************************************************************************
 End of file (km_apphint.h)
******************************************************************************/
