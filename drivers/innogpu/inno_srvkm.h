/*
* Copyright (c) Innosilicon Technology Ltd. All Rights Reserved
* Dual MIT/GPLv2
*
* The contents of this file are subject to the MIT license as set out below.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* Alternatively, the contents of this file may be used under the terms of
* the GNU General Public License Version 2 ("GPL") in which case the provisions
* of GPL are applicable instead of those above.
*
* If you wish to allow use of your version of this file only under the terms of
* GPL, and not to allow others to use your version of this file under the terms
* of the MIT license, indicate your decision by deleting the provisions above
* and replace them with the notice and other provisions required by GPL as set
* out in the file called "GPL-COPYING" included in this distribution. If you do
* not delete the provisions above, a recipient may use your version of this file
* under the terms of either the MIT license or GPL.
*
* This License is also included in this distribution in the file called
* "MIT-COPYING".
*
* EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
* PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
* BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
* PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
* COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
* IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
* CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef __INNO_SRVKM_H__
#define __INNO_SRVKM_H__

#include "inno_fs.h"
#include "inno_debugfs.h"
#include "inno_mm.h"

void fh2m_inno_pmr_cpumapcount_dec(void *pmr);

int fh2m_inno_pmr_unref(void *pmr);

void fh2m_inno_pmr_ref(void *pmr);

int fh2m_inno_pmr_lock_sys_phys_addr(void *pmr);

int fh2m_inno_pmr_unlock_sys_phys_addr(void *pmr);

void fh2m_inno_stats_remove_mem_alloc_record(int type, uint64_t key, uint32_t pid);

void fh2m_inno_stats_decr_mem_alloc_stat(int type, uint64_t size, uint32_t pid);

int fh2m_inno_pmr_write_bytes(void *pmr, uint64_t offset, uint8_t *buffer, uint64_t bufsize, size_t *bytes);

int fh2m_inno_pmr_read_bytes(void *pmr, uint64_t offset, uint8_t *buffer, uint64_t bufsize, size_t *bytes);

ssize_t fh2m_inno_debugfs_write(inno_file *psFile, const char __user *pszBuffer, size_t uiCount, loff_t *puiPos);

loff_t fh2m_inno_debugfs_lseek(inno_file *psFile, loff_t iOffset, int iOrigin);

ssize_t fh2m_inno_debugfs_read(inno_file *psFile, char __user *pcBuffer, size_t uiCount, loff_t *puiPos);

int fh2m_inno_debugfs_close(inno_inode *psINode, inno_file *psFile, void *data);

int fh2m_inno_debugfs_open(inno_inode *psINode, inno_file *psFile, void *data);

int fh2m_inno_debugfs_seq_show(void *entry, void *pvPriv);

void *fh2m_inno_debugfs_seq_start(void *entry, loff_t *puiPos);

void fh2m_inno_debugfs_seq_stop(void *entry, void *pvPriv);

void *fh2m_inno_debugfs_seq_next(void *entry, void *pvPriv, loff_t *puiPos);

bool inno_pmr_is_inv(void *pmr);

void inno_pmr_flush(void *pmr);

int inno_pmr_handle_page_fault(void *pmr, inno_vm_area *vma, inno_vm_fault *vmf);
#endif
