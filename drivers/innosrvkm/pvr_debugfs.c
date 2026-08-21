/*************************************************************************/ /*!
@File
@Title          DebugFS implementation of Debug Info interface.
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Implements osdi_impl.h API to provide access to driver's
                debug data via DebugFS.

                Note about locking in DebugFS module.

                Access to DebugFS is protected against the race where any
                file could be removed while being accessed or accessed while
                being removed. Any calls to debugfs_remove() will block
                until all operations are finished.

                See implementation of proxy file operations (FULL_PROXY_FUNC)
                and implementation of debugfs_file_[get|put]() in
                fs/debugfs/file.c in Linux kernel sources for more details.

                Not about locking for sequential files.

                The seq_file objects have a mutex that protects access
                to all of the file operations hence all of the sequential
                *read* operations are protected.
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
#include "img_types.h"
#include "img_defs.h"
#include "pvr_debug.h"
#include "pvr_debugfs.h"
#include "osfunc.h"
#include "allocmem.h"
#include "pvr_bridge_k.h"
#include "pvr_uaccess.h"
#include "osdi_impl.h"
#include "inno_fs.h"
#include "inno_debugfs.h"
#include "inno_misc.h"
#include "inno_srvkm.h"

#define _DRIVER_THREAD_ENTER() \
	do { \
		PVRSRV_ERROR eLocalError = PVRSRVDriverThreadEnter(NULL); \
		if (eLocalError != PVRSRV_OK) \
		{ \
			PVR_DPF((PVR_DBG_ERROR, "%s: PVRSRVDriverThreadEnter failed: %s", \
				__func__, fh2m_PVRSRVGetErrorString(eLocalError))); \
			return OSPVRSRVToNativeError(eLocalError); \
		} \
	} while (0)

#define _DRIVER_THREAD_EXIT() \
	PVRSRVDriverThreadExit(NULL)

#define PVR_DEBUGFS_PVR_DPF_LEVEL PVR_DBG_ERROR

typedef struct DFS_DIR
{
	inno_dentry *psDirEntry;
	struct DFS_DIR *psParentDir;
} DFS_DIR;

typedef struct DFS_ENTRY
{
	OSDI_IMPL_ENTRY sImplEntry;
	DI_ITERATOR_CB sIterCb;
} DFS_ENTRY;

typedef struct DFS_FILE
{
	inno_dentry *psFileEntry;
	struct DFS_DIR *psParentDir;
	struct DFS_ENTRY sEntry;
	DI_ENTRY_TYPE eType;
} DFS_FILE;

/* ----- native callbacks interface ----------------------------------------- */

static void _WriteData(void *pvNativeHandle, const void *pvData,
                       IMG_UINT32 uiSize)
{
	fh2m_inno_seq_write(pvNativeHandle, pvData, uiSize);
}

static void _VPrintf(void *pvNativeHandle, const IMG_CHAR *pszFmt,
                     va_list pArgs)
{
	fh2m_inno_seq_vprintf(pvNativeHandle, pszFmt, pArgs);
}

static void _Puts(void *pvNativeHandle, const IMG_CHAR *pszStr)
{
	fh2m_inno_seq_puts(pvNativeHandle, pszStr);
}

static IMG_BOOL _HasOverflowed(void *pvNativeHandle)
{
	return fh2m_inno_seq_has_overflowed(pvNativeHandle);
}

static OSDI_IMPL_ENTRY_CB _g_sEntryCallbacks = {
	.pfnWrite = _WriteData,
	.pfnVPrintf = _VPrintf,
	.pfnPuts = _Puts,
	.pfnHasOverflowed = _HasOverflowed,
};

/* ----- sequential file operations ----------------------------------------- */
void *fh2m_inno_debugfs_seq_start(void *entry, loff_t *puiPos)
{
	DFS_ENTRY *psEntry = entry;
	return psEntry->sIterCb.pfnStart(&psEntry->sImplEntry, puiPos);
}

void fh2m_inno_debugfs_seq_stop(void *entry, void *pvPriv)
{
	DFS_ENTRY *psEntry = entry;
	psEntry->sIterCb.pfnStop(&psEntry->sImplEntry, pvPriv);
}

void *fh2m_inno_debugfs_seq_next(void *entry, void *pvPriv, loff_t *puiPos)
{
	DFS_ENTRY *psEntry = entry;
	return psEntry->sIterCb.pfnNext(&psEntry->sImplEntry, pvPriv, puiPos);
}

int fh2m_inno_debugfs_seq_show(void *entry, void *pvPriv)
{
	DFS_ENTRY *psEntry = entry;

	return psEntry->sIterCb.pfnShow(&psEntry->sImplEntry, pvPriv);
}

/* ----- file operations ---------------------------------------------------- */
int fh2m_inno_debugfs_open(inno_inode *psINode, inno_file *psFile, void *data)
{
	DFS_FILE *psDFSFile;
	int iRes;

	PVR_LOG_RETURN_IF_FALSE(psINode != NULL, "psDFSFile is NULL", -EIO);

	_DRIVER_THREAD_ENTER();

	psDFSFile = data;

	if (psDFSFile->sEntry.sIterCb.pfnStart != NULL)
	{
		iRes = fh2m_inno_seq_open_for_pvr(psFile);
	}
	else
	{
		/* private data is NULL as it's going to be set below */
		iRes = fh2m_inno_single_open_for_pvr(psFile);
	}

	if (iRes == 0)
	{
		inno_seq_file *psSeqFile = fh2m_inno_get_file_prvdata(psFile);

		DFS_ENTRY *psEntry = OSAllocMem(sizeof(*psEntry));
		if (psEntry == NULL)
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: OSAllocMem() failed", __func__));
			iRes = -ENOMEM;
			goto return_;
		}

		*psEntry = psDFSFile->sEntry;
		fh2m_inno_seq_set_prvdata(psSeqFile, psEntry);
		psEntry->sImplEntry.pvNative = psSeqFile;
	}
	else
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to seq_open psFile, returning %d",
		        __func__, iRes));
	}

return_:
	_DRIVER_THREAD_EXIT();

	return iRes;
}

int fh2m_inno_debugfs_close(inno_inode *psINode, inno_file *psFile, void *data)
{
	DFS_FILE *psDFSFile = data;
	DFS_ENTRY *psEntry;
	int iRes;
	inno_seq_file *psSeqFile = fh2m_inno_get_file_prvdata(psFile);

	PVR_LOG_RETURN_IF_FALSE(psDFSFile != NULL, "psDFSFile is NULL",
	                        -EIO);

	_DRIVER_THREAD_ENTER();

	/* save pointer to DFS_ENTRY */
	psEntry = fh2m_inno_seq_get_prvdata(psSeqFile);

	if (psDFSFile->sEntry.sIterCb.pfnStart != NULL)
	{
		iRes = fh2m_inno_seq_release(psINode, psFile);
	}
	else
	{
		iRes = fh2m_inno_single_release(psINode, psFile);
	}

	/* free DFS_ENTRY allocated in _Open */
	OSFreeMem(psEntry);

	/* Validation check as seq_release (and single_release which calls it)
	 * never fail */
	if (iRes != 0)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to release psFile, returning %d",
		        __func__, iRes));
	}

	_DRIVER_THREAD_EXIT();

	return iRes;
}

ssize_t fh2m_inno_debugfs_read(inno_file *psFile, char __user *pcBuffer,
                     size_t uiCount, loff_t *puiPos)
{
	ssize_t iRes = -1;
	DFS_FILE *psDFSFile = fh2m_inno_debugfs_file_get_prvdata(psFile);

	_DRIVER_THREAD_ENTER();

	if (psDFSFile->eType == DI_ENTRY_TYPE_GENERIC)
	{
		iRes = fh2m_inno_seq_read(psFile, pcBuffer, uiCount, puiPos);
		if (iRes < 0)
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: failed to read from file, pfnRead() "
			        "returned %zd", __func__, iRes));
			goto return_;
		}
	}
	else if (psDFSFile->eType == DI_ENTRY_TYPE_RANDOM_ACCESS)
	{
		DFS_ENTRY *psEntry = &psDFSFile->sEntry;
		IMG_UINT64 ui64Count = uiCount;

		IMG_CHAR *pcLocalBuffer = OSAllocMem(uiCount);
		PVR_GOTO_IF_FALSE(pcLocalBuffer != NULL, return_);

		iRes = psEntry->sIterCb.pfnRead(pcLocalBuffer, ui64Count, puiPos,
		                                psEntry->sImplEntry.pvPrivData);
		if (iRes < 0)
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: failed to read from file, pfnRead() "
			        "returned %zd", __func__, iRes));
			OSFreeMem(pcLocalBuffer);
			goto return_;
		}

		if (pvr_copy_to_user(pcBuffer, pcLocalBuffer, iRes) != 0)
		{
			iRes = -1;
		}

		OSFreeMem(pcLocalBuffer);
	}

return_:
	_DRIVER_THREAD_EXIT();

	return iRes;
}

loff_t fh2m_inno_debugfs_lseek(inno_file *psFile, loff_t iOffset, int iOrigin)
{
	loff_t iRes = -1;
	DFS_FILE *psDFSFile = fh2m_inno_debugfs_file_get_prvdata(psFile);

	_DRIVER_THREAD_ENTER();

	if (psDFSFile->eType == DI_ENTRY_TYPE_GENERIC)
	{
		iRes = fh2m_inno_seq_lseek(psFile, iOffset, iOrigin);
		if (iRes < 0)
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: failed to set file position in psFile<%p> to offset "
			        "%lld, iOrigin %d, seq_lseek() returned %lld (dentry='%s')", __func__,
			        psFile, iOffset, iOrigin, iRes, fh2m_inno_debugfs_file_name(psFile)));
			goto return_;
		}
	}
	else if (psDFSFile->eType == DI_ENTRY_TYPE_RANDOM_ACCESS)
	{
		DFS_ENTRY *psEntry = &psDFSFile->sEntry;
		IMG_UINT64 ui64Pos;

		switch (iOrigin)
		{
			case SEEK_SET:
				ui64Pos = fh2m_inno_file_pos(psFile) + iOffset;
				break;
			case SEEK_CUR:
				ui64Pos = iOffset;
				break;
			case SEEK_END:
				/* not supported as we don't know the file size here */
				/* fall through */
			default:
				return -1;
		}

		/* only pass the absolute position to the callback, it's up to the
		 * implementer to determine if the position is valid */

		iRes = psEntry->sIterCb.pfnSeek(ui64Pos,
		                                psEntry->sImplEntry.pvPrivData);
		if (iRes < 0)
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: failed to set file position to offset "
			        "%lld, pfnSeek() returned %lld", __func__,
			        iOffset, iRes));
			goto return_;
		}

		fh2m_inno_file_set_pos(psFile, ui64Pos);
	}

return_:
	_DRIVER_THREAD_EXIT();

	return iRes;
}

ssize_t fh2m_inno_debugfs_write(inno_file *psFile, const char __user *pszBuffer,
                      size_t uiCount, loff_t *puiPos)
{
	DFS_FILE *psDFSFile = fh2m_inno_debugfs_file_get_prvdata(psFile);
	DI_ITERATOR_CB *psIter = &psDFSFile->sEntry.sIterCb;
	IMG_CHAR *pcLocalBuffer;
	IMG_UINT64 ui64Count = uiCount + 1, ui64Pos = *puiPos;
	IMG_INT64 i64Res = -EIO;

	PVR_LOG_RETURN_IF_FALSE(psDFSFile != NULL, "psDFSFile is NULL",
	                        -EIO);
	PVR_LOG_RETURN_IF_FALSE(psIter->pfnWrite != NULL, "pfnWrite is NULL",
	                        -EIO);

	_DRIVER_THREAD_ENTER();

	/* Make sure we allocate the smallest amount of needed memory*/
	ui64Count = psIter->ui32WriteLenMax;
	PVR_LOG_GOTO_IF_FALSE(uiCount <= ui64Count, "uiCount too long", return_);
	ui64Count = MIN(uiCount + 1, ui64Count);

	/* allocate buffer with one additional byte for NUL character */
	pcLocalBuffer = OSAllocMem(ui64Count);
	PVR_LOG_GOTO_IF_FALSE(pcLocalBuffer != NULL, "OSAllocMem() failed",
	                      return_);

	i64Res = pvr_copy_from_user(pcLocalBuffer, pszBuffer, ui64Count);
	PVR_LOG_GOTO_IF_FALSE(i64Res == 0, "pvr_copy_from_user() failed",
	                      free_local_buffer_);

	/* ensure that the framework user gets a NUL terminated buffer */
	pcLocalBuffer[ui64Count - 1] = '\0';

	i64Res = psIter->pfnWrite(pcLocalBuffer, ui64Count, &ui64Pos,
	                          psDFSFile->sEntry.sImplEntry.pvPrivData);
	PVR_LOG_GOTO_IF_FALSE(i64Res >= 0, "pfnWrite failed", free_local_buffer_);

	*puiPos = ui64Pos;

free_local_buffer_:
	OSFreeMem(pcLocalBuffer);

return_:
	_DRIVER_THREAD_EXIT();

	return i64Res;
}

/* ----- DI implementation interface ---------------------------------------- */

static PVRSRV_ERROR _Init(void)
{
	return PVRSRV_OK;
}

static void _DeInit(void)
{
}

static PVRSRV_ERROR _CreateFile(const IMG_CHAR *pszName,
                                DI_ENTRY_TYPE eType,
                                const DI_ITERATOR_CB *psIterCb,
                                void *pvPrivData,
                                void *pvParentDir,
                                void **pvFile)
{
	DFS_DIR *psParentDir = pvParentDir;
	DFS_FILE *psFile;
	umode_t uiMode = S_IFREG;
	inno_dentry *psEntry;
	IMG_BOOL bIsGenAccess;
	PVRSRV_ERROR eError;

	PVR_LOG_RETURN_IF_INVALID_PARAM(pvFile != NULL, "pvFile");
	PVR_LOG_RETURN_IF_INVALID_PARAM(pvParentDir != NULL, "pvParentDir");

	switch (eType)
	{
		case DI_ENTRY_TYPE_GENERIC:
			bIsGenAccess = true;
			break;
		case DI_ENTRY_TYPE_RANDOM_ACCESS:
			bIsGenAccess = false;
			break;
		default:
			PVR_DPF((PVR_DBG_ERROR, "eType invalid in %s()", __func__));
			eError = PVRSRV_ERROR_INVALID_PARAMS;
			goto return_;
	}

	psFile = OSAllocMem(sizeof(*psFile));
	PVR_LOG_GOTO_IF_NOMEM(psFile, eError, return_);

	uiMode |= psIterCb->pfnShow != NULL || psIterCb->pfnRead != NULL ?
	        S_IRUGO : 0;
	uiMode |= psIterCb->pfnWrite != NULL ? S_IWUSR : 0;

	psEntry = fh2m_inno_debugfs_create_file_for_pvr(pszName, uiMode,
			psParentDir->psDirEntry, psFile, bIsGenAccess);
	if (fh2m_inno_is_err_or_null(psEntry))
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Cannot create debugfs '%s' file",
		        __func__, pszName));

		eError = psEntry == NULL ?
		        PVRSRV_ERROR_OUT_OF_MEMORY : PVRSRV_ERROR_INVALID_DEVICE;
		goto free_file_;
	}

	psFile->eType = eType;
	psFile->sEntry.sIterCb = *psIterCb;
	psFile->sEntry.sImplEntry.pvPrivData = pvPrivData;
	psFile->sEntry.sImplEntry.pvNative = NULL;
	psFile->sEntry.sImplEntry.psCb = &_g_sEntryCallbacks;
	psFile->psParentDir = psParentDir;
	psFile->psFileEntry = psEntry;

	*pvFile = psFile;

	return PVRSRV_OK;

free_file_:
	OSFreeMem(psFile);

return_:
	return eError;
}

static void _DestroyFile(void *pvFile)
{
	DFS_FILE *psFile = pvFile;

	PVR_ASSERT(psFile != NULL);

	fh2m_inno_debugfs_remove(psFile->psFileEntry);
	OSFreeMem(psFile);
}

static PVRSRV_ERROR _CreateDir(const IMG_CHAR *pszName,
                               void *pvParentDir,
                               void **ppvDir)
{
	DFS_DIR *psNewDir;
	inno_dentry *psDirEntry, *psParentDir = NULL;

	PVR_LOG_RETURN_IF_INVALID_PARAM(pszName != NULL, "pszName");
	PVR_LOG_RETURN_IF_INVALID_PARAM(ppvDir != NULL, "ppvDir");

	psNewDir = OSAllocMem(sizeof(*psNewDir));
	PVR_LOG_RETURN_IF_NOMEM(psNewDir, "OSAllocMem");

	psNewDir->psParentDir = pvParentDir;

	if (pvParentDir != NULL)
	{
		psParentDir = psNewDir->psParentDir->psDirEntry;
	}

	psDirEntry = fh2m_inno_debugfs_create_dir(pszName, psParentDir);
	if (fh2m_inno_is_err_or_null(psDirEntry))
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Cannot create '%s' debugfs directory",
		        __func__, pszName));
		OSFreeMem(psNewDir);
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	psNewDir->psDirEntry = psDirEntry;
	*ppvDir = psNewDir;

	return PVRSRV_OK;
}

static void _DestroyDir(void *pvDir)
{
	DFS_DIR *psDir = pvDir;

	PVR_ASSERT(psDir != NULL);

	fh2m_inno_debugfs_remove(psDir->psDirEntry);
	OSFreeMem(psDir);
}

PVRSRV_ERROR PVRDebugFsRegister(void)
{
	OSDI_IMPL_CB sImplCb = {
		.pfnInit = _Init,
		.pfnDeInit = _DeInit,
		.pfnCreateEntry = _CreateFile,
		.pfnDestroyEntry = _DestroyFile,
		.pfnCreateGroup = _CreateDir,
		.pfnDestroyGroup = _DestroyDir
	};

	return DIRegisterImplementation("debugfs", &sImplCb);
}
