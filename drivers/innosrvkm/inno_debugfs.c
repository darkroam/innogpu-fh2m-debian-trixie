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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include "inno_srvkm.h"
#include "inno_misc.h"
#include "inno_debugfs.h"
#include "pvr_debugfs.h"
#include "pvr_procfs.h"

static int inno_debugfs_file_open(struct inode *inode, struct file *file)
{
	return fh2m_inno_debugfs_open(inode, file, inode->i_private);
}

static int inno_debugfs_file_close(struct inode *inode, struct file *file)
{
	return fh2m_inno_debugfs_close(inode, file, inode->i_private);
}

static ssize_t inno_debugfs_file_read(struct file *file, char __user *buf,
		size_t count, loff_t *pos)
{
	return fh2m_inno_debugfs_read(file, buf, count, pos);
}

static loff_t inno_debugfs_file_lseek(struct file *file, loff_t off, int origin)
{
	return fh2m_inno_debugfs_lseek(file, off, origin);
}

static ssize_t inno_debugfs_file_write(struct file *file, const char __user *buffer,
		size_t count, loff_t *pos)
{
	return fh2m_inno_debugfs_write(file, buffer, count, pos);
}

static const struct file_operations inno_debugfs_file_gen_ops = {
	.owner   = THIS_MODULE,
	.open    = inno_debugfs_file_open,
	.release = inno_debugfs_file_close,
	.read    = inno_debugfs_file_read,
	.llseek  = inno_debugfs_file_lseek,
	.write   = inno_debugfs_file_write,
};

static const struct file_operations inno_debugfs_file_rnd_ops = {
	.owner  = THIS_MODULE,
	.read   = inno_debugfs_file_read,
	.llseek = inno_debugfs_file_lseek,
	.write  = inno_debugfs_file_write,
};

static void *inno_seq_start_ops(struct seq_file *seqf, loff_t *pos)
{
	void *entry = seqf->private;
	return fh2m_inno_debugfs_seq_start(entry, pos);
}

static void inno_seq_stop_ops(struct seq_file *seqf, void *priv)
{
	void *entry = seqf->private;
	fh2m_inno_debugfs_seq_stop(entry, priv);
}

static void *inno_seq_next_ops(struct seq_file *seqf, void *priv, loff_t *pos)
{
	void *entry = seqf->private;
	return fh2m_inno_debugfs_seq_next(entry, priv, pos);
}

static int inno_seq_show_ops(struct seq_file *seqf, void *priv)
{
	void *entry = seqf->private;
	return fh2m_inno_debugfs_seq_show(entry, priv);
}

static struct seq_operations inno_seq_ops = {
	.start = inno_seq_start_ops,
	.stop  = inno_seq_stop_ops,
	.next  = inno_seq_next_ops,
	.show  = inno_seq_show_ops,
};

int fh2m_inno_seq_open_for_pvr(inno_file *file)
{
	return seq_open((struct file *)file, &inno_seq_ops);
}
INNO_EXT_SYM(fh2m_inno_seq_open_for_pvr);

int fh2m_inno_single_open_for_pvr(inno_file *file)
{
	return single_open((struct file *)file, inno_seq_show_ops, NULL);
}
INNO_EXT_SYM(fh2m_inno_single_open_for_pvr);

void fh2m_inno_seq_set_prvdata(inno_seq_file *seqf, void *priv)
{
	((struct seq_file *)seqf)->private = priv;
}
INNO_EXT_SYM(fh2m_inno_seq_set_prvdata);

void *fh2m_inno_seq_get_prvdata(inno_seq_file *seqf)
{
	return ((struct seq_file *)seqf)->private;
}
INNO_EXT_SYM(fh2m_inno_seq_get_prvdata);

int fh2m_inno_seq_release(inno_inode *inode, inno_file *file)
{
	return seq_release((struct inode *)inode, (struct file *)file);
}
INNO_EXT_SYM(fh2m_inno_seq_release);

int fh2m_inno_single_release(inno_inode *inode, inno_file *file)
{
	return single_release((struct inode *)inode, (struct file *)file);
}
INNO_EXT_SYM(fh2m_inno_single_release);

void *fh2m_inno_debugfs_file_get_prvdata(inno_file *file)
{
	return ((struct file *)file)->f_path.dentry->d_inode->i_private;
}
INNO_EXT_SYM(fh2m_inno_debugfs_file_get_prvdata);

const char *fh2m_inno_debugfs_file_name(inno_file *file)
{
	return ((struct file *)file)->f_path.dentry->d_name.name;
}
INNO_EXT_SYM(fh2m_inno_debugfs_file_name);

ssize_t fh2m_inno_seq_read(inno_file *file, char __user *buf, size_t count, loff_t *pos)
{
	return seq_read((struct file *)file, buf, count, pos);
}
INNO_EXT_SYM(fh2m_inno_seq_read);

int fh2m_inno_seq_write(inno_seq_file *seq, const void *data, size_t len)
{
	return seq_write(seq, data, len);
}
INNO_EXT_SYM(fh2m_inno_seq_write);

loff_t fh2m_inno_seq_lseek(inno_file *file, loff_t off, int origin)
{
	return seq_lseek((struct file *)file, off, origin);
}
INNO_EXT_SYM(fh2m_inno_seq_lseek);

loff_t fh2m_inno_file_pos(inno_file *file)
{
	return ((struct file *)file)->f_pos;
}
INNO_EXT_SYM(fh2m_inno_file_pos);

void fh2m_inno_file_set_pos(inno_file *file, loff_t pos)
{
	((struct file *)file)->f_pos = pos;
}
INNO_EXT_SYM(fh2m_inno_file_set_pos);

void fh2m_inno_seq_vprintf(inno_seq_file *seqf, const char *fmt, va_list args)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0)
	seq_vprintf(seqf, fmt, args);
#else
	char buf[512];
	vsnprintf(buf, 512, fmt, args);
	seq_printf(seqf, "%s", buf);
#endif
}
INNO_EXT_SYM(fh2m_inno_seq_vprintf);

void fh2m_inno_seq_puts(inno_seq_file *seqf, const char *s)
{
	seq_puts(seqf, s);
}
INNO_EXT_SYM(fh2m_inno_seq_puts);

bool fh2m_inno_seq_has_overflowed(inno_seq_file *seqf)
{
	struct seq_file *file = (struct seq_file *)seqf;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)
	return seq_has_overflowed(file);
#else
	return file->count == file->size;
#endif
}
INNO_EXT_SYM(fh2m_inno_seq_has_overflowed);

inno_dentry *fh2m_inno_debugfs_create_file_for_pvr(const char *name, umode_t mode,
		inno_dentry *parent, void *data, bool is_gen)
{
	if (is_gen)
		return debugfs_create_file(name, mode, parent, data, &inno_debugfs_file_gen_ops);
	else
		return debugfs_create_file(name, mode, parent, data, &inno_debugfs_file_rnd_ops);
}
INNO_EXT_SYM(fh2m_inno_debugfs_create_file_for_pvr);

void fh2m_inno_debugfs_remove(inno_dentry *dentry)
{
	debugfs_remove((struct dentry *)dentry);
}
INNO_EXT_SYM(fh2m_inno_debugfs_remove);

inno_dentry *fh2m_inno_debugfs_create_dir(const char *name, inno_dentry *parent)
{
	return debugfs_create_dir(name, parent);
}
INNO_EXT_SYM(fh2m_inno_debugfs_create_dir);

int fh2m_inno_debugorprocfs_register(void)
{
#if defined(CONFIG_PROC_FS)
	PVRProcFsRegister2();
#endif

#if defined(ANDROID) && !defined(__INNO_CONTAINER__)
#if defined(CONFIG_PROC_FS)
	return PVRProcFsRegister();
#elif defined(CONFIG_DEBUG_FS)
	return PVRDebugFsRegister();
#endif /* defined(CONFIG_PROC_FS) || defined(CONFIG_DEBUG_FS) */
#else
#if defined(CONFIG_DEBUG_FS)
	return PVRDebugFsRegister();
#elif defined(CONFIG_PROC_FS)
	return PVRProcFsRegister();
#endif /* defined(CONFIG_DEBUG_FS) || defined(CONFIG_PROC_FS) */
#endif /* defined(ANDROID) */
}
INNO_EXT_SYM(fh2m_inno_debugorprocfs_register);

INNO_EXT_SYM(fh2m_inno_debugfs_seq_start);
INNO_EXT_SYM(fh2m_inno_debugfs_seq_stop);
INNO_EXT_SYM(fh2m_inno_debugfs_seq_next);
INNO_EXT_SYM(fh2m_inno_debugfs_seq_show);
INNO_EXT_SYM(fh2m_inno_debugfs_open);
INNO_EXT_SYM(fh2m_inno_debugfs_close);
INNO_EXT_SYM(fh2m_inno_debugfs_read);
INNO_EXT_SYM(fh2m_inno_debugfs_lseek);
INNO_EXT_SYM(fh2m_inno_debugfs_write);
