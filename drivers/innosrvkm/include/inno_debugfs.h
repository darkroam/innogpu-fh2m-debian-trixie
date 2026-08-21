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
#ifndef __INNO_DEBUGFS_H__
#define __INNO_DEBUGFS_H__

#include "inno_fs.h"

int fh2m_inno_seq_open_for_pvr(inno_file *file);

int fh2m_inno_single_open_for_pvr(inno_file *file);

void fh2m_inno_seq_set_prvdata(inno_seq_file *seqf, void *priv);

void *fh2m_inno_seq_get_prvdata(inno_seq_file *seqf);

int fh2m_inno_seq_release(inno_inode *inode, inno_file *file);

int fh2m_inno_single_release(inno_inode *inode, inno_file *file);

void fh2m_inno_seq_vprintf(inno_seq_file *seqf, const char *fmt, va_list args);

void fh2m_inno_seq_puts(inno_seq_file *seqf, const char *s);

bool fh2m_inno_seq_has_overflowed(inno_seq_file *seqf);

void *fh2m_inno_debugfs_file_get_prvdata(inno_file *file);

const char *fh2m_inno_debugfs_file_name(inno_file *file);

ssize_t fh2m_inno_seq_read(inno_file *file, char __user *buf, size_t count, loff_t *pos);

int fh2m_inno_seq_write(inno_seq_file *seq, const void *data, size_t len);

loff_t fh2m_inno_seq_lseek(inno_file *file, loff_t off, int origin);

loff_t fh2m_inno_file_pos(inno_file *file);

void fh2m_inno_file_set_pos(inno_file *file, loff_t pos);

inno_dentry *fh2m_inno_debugfs_create_file_for_pvr(const char *name, umode_t mode,
		inno_dentry *parent, void *data, bool is_gen);

void fh2m_inno_debugfs_remove(inno_dentry *dentry);

inno_dentry *fh2m_inno_debugfs_create_dir(const char *name, inno_dentry *parent);

int fh2m_inno_debugorprocfs_register(void);
#endif
