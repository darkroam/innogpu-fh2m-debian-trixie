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
#include <linux/printk.h>
#if defined(__linux__)
 #include <linux/version.h>
 #if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
  #include <linux/stdarg.h>
 #else
  #include <stdarg.h>
 #endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0) */
#else
 #include <stdarg.h>
#endif /* __linux__ */
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/seq_file.h>
#include <linux/fs.h>

#include "inno_debug.h"
#include "inno_misc.h"
#include "inno_mm.h"
#include "inno_timer.h"
#include "inno_debugfs.h"
#include "inno_lock.h"

unsigned int s_dpu_debug = 0x00;
MODULE_PARM_DESC(s_dpu_debug, "Enable debug output, where each bit enables a debug category.\n"
		"\t\tBit 0 (0x01) (drm & fbdev code)\n"
		"\t\tBit 1 (0x02) (modeset code)\n"
		"\t\tBit 2 (0x04) (gem code)\n"
		"\t\tBit 3 (0x08) (dpu code)\n"
		"\t\tBit 4 (0x10) (hdmi code)\n"
		"\t\tBit 5 (0x20) (dp code)\n"
		"\t\tBit 6 (0x40) (lvds code)\n"
		"\t\tBit 7 (0x80) (vga code)\n");
module_param_named(s_dpu_debug, s_dpu_debug, uint, 0600);

bool s_dpu_open_log = true;
MODULE_PARM_DESC(s_dpu_open_log, "Enable save output");
module_param_named(s_dpu_open_log, s_dpu_open_log, bool, 0600);

#define DPU_LOG_ITEM_LEN 150
#define DPU_LOG_PRINT_LEN (DPU_LOG_ITEM_LEN + 34 + 1)

typedef enum log_mod_e {
	INNODPU_LOG_MOD_FATAL = 0,
	INNODPU_LOG_MOD_CRTC,
	INNODPU_LOG_MOD_HDMI,
	INNODPU_LOG_MOD_DP,
	INNODPU_LOG_MOD_VGA,
	INNODPU_LOG_MOD_LVDS,
	INNODPU_LOG_MOD_DRM,
	INNODPU_LOG_MOD_KMS,
	INNODPU_LOG_MOD_GEM,
	INNODPU_LOG_MOD_MAX,
} log_mod;

typedef struct node {
	char   *data;
	struct node *next;
} circular_node;

typedef struct {
	circular_node *head;
	circular_node *tail;
	inno_spinlock *log_lock;
	int size;
	int max_size;
} circular_list;

struct innodpu_log {
	circular_list *kfifo[INNODPU_LOG_MOD_MAX];
	struct mutex *log_mutex;
	/* record current target want get */
	log_mod mod;
};

static struct innodpu_log *inno_log = NULL;
static char *s_dpu_log_mod_name[INNODPU_LOG_MOD_MAX] = {
	"[FATAL]", "[CRTC]", "[HDMI]", "[DP]", "[VGA]", "[LVDS]", "[DRM]", "[KMS]", "[GEM]"
};

static void innodpu_log_info(inno_dev *dev, log_mod mod, const char *format, ...);

void fh2m_inno_dev_printk(char *debug_level, inno_dev *dev, const char *format, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, format);
	vaf.fmt = format;
	vaf.va = &args;

	dev_printk(debug_level, dev, "%pV", &vaf);

	va_end(args);
}

void fh2m_inno_printk(const char *format, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, format);
	vaf.fmt = format;
	vaf.va = &args;

	printk("%pV", &vaf);

	va_end(args);
}

void inno_printk_ratelimited(const char *format, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, format);
	vaf.fmt = format;
	vaf.va = &args;

	printk_ratelimited("%pV", &vaf);

	va_end(args);
}

/* Note:
 * Why fh2m_inno_trace_printk() cannot be implemented in release build type?
 * kernel api trace_printk() that fh2m_inno_trace_printk() invoking is a macro-define and not function,
 * which cause bug#12192.
 */
#if defined(INNO_GPU_LOG)
void fh2m_inno_trace_printk(const char *format, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, format);
	vaf.fmt = format;
	vaf.va = &args;

	trace_printk("%pV", &vaf);

	va_end(args);
}
#else
void fh2m_inno_trace_printk(const char *format, ...)
{
}
#endif

static log_mod innodpu_get_mod_by_category(unsigned int category)
{
	log_mod mod = INNODPU_LOG_MOD_FATAL;

	if (category & DPU_UT_HDMI) {
		mod = INNODPU_LOG_MOD_HDMI;
	} else if (category & DPU_UT_DPU) {
		mod = INNODPU_LOG_MOD_CRTC;
	} else if (category & DPU_UT_DP) {
		mod = INNODPU_LOG_MOD_DP;
	} else if (category & DPU_UT_LVDS) {
		mod = INNODPU_LOG_MOD_LVDS;
	} else if (category & DPU_UT_VGA) {
		mod = INNODPU_LOG_MOD_VGA;
	} else if (category & DPU_UT_MEM) {
		mod = INNODPU_LOG_MOD_GEM;
	} else if (category & DPU_UT_DRM) {
		mod = INNODPU_LOG_MOD_DRM;
	} else if (category & DPU_UT_KMS) {
		mod = INNODPU_LOG_MOD_KMS;
	} else {
		mod = INNODPU_LOG_MOD_FATAL;
	}

	return mod;
}

void fh2m_innodpu_info(inno_dev *dev, unsigned int category, const char *format, ...)
{
	log_mod mod = INNODPU_LOG_MOD_FATAL;
	struct va_format vaf;
	va_list args;

	va_start(args, format);
	vaf.fmt = format;
	vaf.va = &args;

	if (category != DPU_UT_CURSOR && category != DPU_UT_GAMMA) {
		mod = innodpu_get_mod_by_category(category);
		innodpu_log_info(dev, mod, "[drm:[info] %ps] %pV", __builtin_return_address(0), &vaf);
	}

#if !defined(NO_HARDWARE)
	if (!(s_dpu_debug & category)) {
		va_end(args);
		return;
	}
#endif

	if (dev) {
		fh2m_inno_dev_printk(KERN_INFO, dev, "[drm:[info] %ps] %pV", __builtin_return_address(0), &vaf);
	} else {
		fh2m_inno_printk(KERN_INFO "[drm:[info] %ps] %pV", __builtin_return_address(0), &vaf);
	}

	va_end(args);
}

void fh2m_inno_print_hex_dump(const char *level, const char *prefix_str,
				  int prefix_type, int rowsize, int groupsize,
				  const void *buf, size_t len, bool ascii)
{
	log_mod mod = INNODPU_LOG_MOD_KMS;
	const unsigned char *ptr = buf;
	int i, linelen, remaining = len;
	unsigned char linebuf[32 * 3 + 2 + 32 + 1];

	if (rowsize != 16 && rowsize != 32) {
		rowsize = 16;
	}

	for (i = 0; i < len; i += rowsize) {
		linelen = min(remaining, rowsize);
		remaining -= rowsize;

		hex_dump_to_buffer(ptr + i, linelen, rowsize, groupsize,
				   linebuf, sizeof(linebuf), ascii);

		innodpu_log_info(NULL, mod, "%s%s\n", prefix_str, linebuf);
	}
}

void fh2m_innodpu_warn(inno_dev *dev, const char *format, ...)
{
	log_mod mod = INNODPU_LOG_MOD_FATAL;
	struct va_format vaf;
	va_list args;

	if (!s_dpu_open_log)
		return;

	va_start(args, format);
	vaf.fmt = format;
	vaf.va = &args;

	if (dev) {
		fh2m_inno_dev_printk(KERN_WARNING, dev, "[drm:[warn] %ps] %pV", __builtin_return_address(0), &vaf);
	} else {
		fh2m_inno_printk(KERN_WARNING "[drm:[warn] %ps] %pV", __builtin_return_address(0), &vaf);
	}

	innodpu_log_info(dev, mod, "[drm:[warn] %ps] %pV", __builtin_return_address(0), &vaf);

	va_end(args);
}

void fh2m_innodpu_err(inno_dev *dev, const char *format, ...)
{
	log_mod mod = INNODPU_LOG_MOD_FATAL;
	struct va_format vaf;
	va_list args;

	va_start(args, format);
	vaf.fmt = format;
	vaf.va = &args;

	if (dev) {
		fh2m_inno_dev_printk(KERN_ERR, dev, "[drm:[ err] %ps] %pV", __builtin_return_address(0), &vaf);
	} else {
		fh2m_inno_printk(KERN_ERR "[drm:[ err] %ps] %pV", __builtin_return_address(0), &vaf);
	}

	innodpu_log_info(dev, mod, "[drm:[ err] %ps] %pV", __builtin_return_address(0), &vaf);

	va_end(args);
}

static void inno_seq_vsprintf(inno_seq_file *seqf, const char *fmt, va_list args)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0)
	seq_vprintf(seqf, fmt, args);
#else
	char buf[512];
	vsnprintf(buf, 512, fmt, args);
	seq_printf(seqf, "%s", buf);
#endif
}

void fh2m_inno_seq_printf(inno_seq_file *m, const char *f, ...)
{
#if !defined(__G3_NE__)
	va_list args;

	va_start(args, f);
	inno_seq_vsprintf(m, f, args);
	va_end(args);
#endif
}

static circular_list *init_circular_list(int max_size)
{
	circular_list *log_handle = (circular_list *)fh2m_inno_kzalloc_kernel(sizeof(circular_list));

	if (!log_handle) {
		return NULL;
	}

	log_handle->head = NULL;
	log_handle->tail = NULL;
	log_handle->size = 0;
	log_handle->max_size = max_size;

	return log_handle;
}

static void insert_node(circular_list *list_handle, char *data)
{
	circular_node *new_node  = NULL;
	circular_node *temp_node = NULL;
	unsigned long flags;

	if (!list_handle) {
		return;
	}

	if (list_handle->size < list_handle->max_size) {
		/* insert tail node */
		new_node = (circular_node *)fh2m_inno_kzalloc_kernel(sizeof(circular_node));
		if (!new_node) {
			return;
		}
		/* copy printk data */
		new_node->data = kstrndup(data, DPU_LOG_PRINT_LEN, GFP_KERNEL);
		if (!new_node->data) {
			if (new_node) {
				fh2m_inno_kfree(new_node);
				new_node = NULL;
			}
			return;
		}

		fh2m_inno_mb();
		fh2m_inno_spin_lock_irqsave(list_handle->log_lock, &flags);
		if (list_handle->head == NULL) {
			list_handle->head = new_node;
		} else {
			list_handle->tail->next = new_node;
		}

		list_handle->tail = new_node;
		list_handle->tail->next = list_handle->head;
		list_handle->size++;
		fh2m_inno_spin_unlock_irqrestore(list_handle->log_lock, flags);
	} else {
		/* remove header node, and insert_node */
		fh2m_inno_spin_lock_irqsave(list_handle->log_lock, &flags);
		temp_node = list_handle->head;
		list_handle->head = list_handle->head->next;
		list_handle->size--;
		list_handle->tail->next = NULL;
		fh2m_inno_spin_unlock_irqrestore(list_handle->log_lock, flags);
		if (temp_node->data) {
			fh2m_inno_kfree(temp_node->data);
			temp_node->data = NULL;
		}
		if (temp_node) {
			fh2m_inno_kfree(temp_node);
			temp_node = NULL;
		}
		fh2m_inno_mb();
		insert_node(list_handle, data);
	}
}

static void clear_circular_list(circular_list *list_handle)
{
	circular_node *current_node = list_handle->head;
	circular_node *temp_node = NULL;
	unsigned long flags;
	int i;

	if (!list_handle) {
		return;
	}

	fh2m_inno_spin_lock_irqsave(list_handle->log_lock, &flags);
	for (i = 0; i < list_handle->size; i++) {
		temp_node = current_node;
		current_node = current_node->next;
		if (temp_node->data) {
			fh2m_inno_kfree(temp_node->data);
			temp_node->data = NULL;
		}
		if (temp_node) {
			fh2m_inno_kfree(temp_node);
			temp_node = NULL;
		}
	}

	list_handle->head = NULL;
	list_handle->size = 0;
	fh2m_inno_spin_unlock_irqrestore(list_handle->log_lock, flags);
}

static void free_circular_list(circular_list *list_handle)
{
	circular_node *current_node = list_handle->head;
	circular_node *temp_node = NULL;
	unsigned long flags;
	int i;

	if (!list_handle) {
		return;
	}

	fh2m_inno_spin_lock_irqsave(list_handle->log_lock, &flags);
	for (i = 0; i < list_handle->size; i++) {
		temp_node = current_node;
		current_node = current_node->next;
		if (temp_node->data) {
			fh2m_inno_kfree(temp_node->data);
			temp_node->data = NULL;
		}
		if (temp_node) {
			fh2m_inno_kfree(temp_node);
			temp_node = NULL;
		}
	}
	fh2m_inno_spin_unlock_irqrestore(list_handle->log_lock, flags);

	if (list_handle->log_lock) {
		fh2m_inno_spinlock_free(list_handle->log_lock);
		list_handle->log_lock = NULL;
	}

	if (list_handle) {
		fh2m_inno_kfree(list_handle);
		list_handle = NULL;
	}
}

static bool innodpu_log_mod_check(log_mod mod)
{
	return ((mod >= INNODPU_LOG_MOD_FATAL) && (mod < INNODPU_LOG_MOD_MAX));
}

static char *get_mod_name_by_id(log_mod mod)
{
	static char *invalid_mod = "invalid_mod";

	if (innodpu_log_mod_check(mod)) {
		return s_dpu_log_mod_name[mod];
	}

	return invalid_mod;
}

static void innodpu_log_print(inno_dev *dev, log_mod mod, char *message)
{
	char temp_buffer[DPU_LOG_PRINT_LEN] = {"\0"};
	inno_ktime current_time = fh2m_inno_ktime_get();
	circular_list *kfifo = NULL;
	unsigned long utime;
	int len = 0, i;

	/* fixme: close kfifo save log */
	if (!s_dpu_open_log) {
		return ;
	}

	if (unlikely(!inno_log)) {
		return;
	}

	kfifo = inno_log->kfifo[mod];

	utime = fh2m_inno_ktime_to_us(current_time);
	fh2m_inno_snprintf(temp_buffer, DPU_LOG_ITEM_LEN + 34, "%7.7s:%8.7s(%5.5d) [%5ld.%3.3ld] %s",
			 get_mod_name_by_id(mod), current->comm, current->tgid,
			 utime / 1000 / 1000, utime / 1000 % 1000, message);

	len = fh2m_inno_strlen(temp_buffer);
	if (len > DPU_LOG_PRINT_LEN) {
		len = DPU_LOG_PRINT_LEN;
	}
	if (len > 0 && temp_buffer[len - 1] != '\n') {
		fh2m_inno_strlcat(temp_buffer, "\n", sizeof(temp_buffer));
	}

	insert_node(kfifo, temp_buffer);

	if (mod == INNODPU_LOG_MOD_KMS) {
		for (i = INNODPU_LOG_MOD_CRTC; i <= INNODPU_LOG_MOD_LVDS; i++) {
			kfifo = inno_log->kfifo[i];
			insert_node(kfifo, temp_buffer);
		}
	}
}

static void innodpu_log_info(inno_dev *dev, log_mod mod, const char *format, ...)
{
	char temp_buffer[DPU_LOG_ITEM_LEN] = {"\0"};
	struct va_format vaf;
	va_list args;

	/* check debug mod here */
	if (!innodpu_log_mod_check(mod)) {
		return;
	}

	va_start(args, format);
	vaf.fmt = format;
	vaf.va  = &args;
	fh2m_inno_vsnprintf(temp_buffer, DPU_LOG_ITEM_LEN, format, args);
	va_end(args);

	innodpu_log_print(dev, mod, temp_buffer);
}

int fh2m_innodpu_log_init(void)
{
	int ret = 0, mod;

	inno_log = fh2m_inno_kzalloc_kernel(sizeof(*inno_log));
	if (!inno_log) {
		return -ENOMEM;
	}

	inno_log->mod = INNODPU_LOG_MOD_FATAL;

	inno_log->log_mutex = fh2m_inno_mutex_alloc();

	for (mod = 0; mod < INNODPU_LOG_MOD_MAX; mod++) {
		inno_log->kfifo[mod] = init_circular_list(1000);
		inno_log->kfifo[mod]->log_lock = fh2m_inno_spinlock_alloc();
	}

	return ret;
}

void fh2m_innodpu_log_fini(void)
{
	int i = 0;

	if (unlikely(!inno_log)) {
		return;
	}

	if (inno_log->log_mutex) {
		fh2m_inno_mutex_free(inno_log->log_mutex);
	}

	for (i = 0; i < INNODPU_LOG_MOD_MAX; i++) {
		free_circular_list(inno_log->kfifo[i]);
	}

	fh2m_inno_kfree(inno_log);
	inno_log = NULL;
}

static int innodpu_log_read(struct seq_file *m, void *data)
{
	circular_node *current_node = NULL;
	circular_node *temp_node = NULL;
	circular_list *kfifo = NULL;
	unsigned long flags;
	int i;


	if (unlikely(!inno_log)) {
		return 0;
	}

	if (!innodpu_log_mod_check(inno_log->mod)) {
		return 0;
	}

	kfifo = inno_log->kfifo[inno_log->mod];
	if (!kfifo) {
		return -EINVAL;
	}

	fh2m_inno_spin_lock_irqsave(kfifo->log_lock, &flags);
	current_node = kfifo->head;
	for (i = 0; i < kfifo->size; i++) {
		temp_node = current_node;
		current_node = current_node->next;
		seq_printf(m, "%s", temp_node->data);
	}
	fh2m_inno_spin_unlock_irqrestore(kfifo->log_lock, flags);

	return 0;
}

/* debufs node this is the entry to userspace */
static int innodpu_log_open(struct inode *inode, struct file *file)
{
	struct drm_info_node *node = inode->i_private;

	return single_open(file, innodpu_log_read, node);
}

/* this maybe use to clear log fifo, to do */
static ssize_t innodpu_log_write(struct file *file,
		const char __user * user_buffer, size_t count, loff_t * position)
{
	char temp_buffer[64] = {"\0"};
	int err = -EFAULT;

	if (unlikely(!inno_log)) {
		return err;
	}

	fh2m_inno_mutex_lock(inno_log->log_mutex);
	count = min(count, ARRAY_SIZE(temp_buffer) - 1);
	err   = fh2m_inno_copy_from_user(temp_buffer, user_buffer, count);
	if (err) {
		goto unlocked;
	}

	if (!fh2m_inno_strncmp(temp_buffer, "fatal", sizeof("fatal") - 1)) {
		inno_log->mod = INNODPU_LOG_MOD_FATAL;
	} else if (!fh2m_inno_strncmp(temp_buffer, "crtc", sizeof("crtc") - 1)) {
		inno_log->mod = INNODPU_LOG_MOD_CRTC;
	} else if (!fh2m_inno_strncmp(temp_buffer, "hdmi", sizeof("hdmi") - 1)) {
		inno_log->mod = INNODPU_LOG_MOD_HDMI;
	} else if (!fh2m_inno_strncmp(temp_buffer, "dp", sizeof("dp") - 1)) {
		inno_log->mod = INNODPU_LOG_MOD_DP;
	} else if (!fh2m_inno_strncmp(temp_buffer, "vga", sizeof("vga") - 1)) {
		inno_log->mod = INNODPU_LOG_MOD_VGA;
	} else if (!fh2m_inno_strncmp(temp_buffer, "gem", sizeof("gem") - 1)) {
		inno_log->mod = INNODPU_LOG_MOD_GEM;
	} else if (!fh2m_inno_strncmp(temp_buffer, "lvds", sizeof("lvds") - 1)) {
		inno_log->mod = INNODPU_LOG_MOD_LVDS;
	} else if (!fh2m_inno_strncmp(temp_buffer, "drm", sizeof("drm") - 1)) {
		inno_log->mod = INNODPU_LOG_MOD_DRM;
	} else if (!fh2m_inno_strncmp(temp_buffer, "kms", sizeof("kms") - 1)) {
		inno_log->mod = INNODPU_LOG_MOD_KMS;
	} else if (!fh2m_inno_strncmp(temp_buffer, "clear", sizeof("clear") - 1)) {
		clear_circular_list(inno_log->kfifo[inno_log->mod]);
	}

unlocked:
	fh2m_inno_mutex_unlock(inno_log->log_mutex);

	return count;
}

const struct file_operations fh2m_s_inno_dpu_log_fops = {
	.owner = THIS_MODULE,
	.open  = innodpu_log_open,
	.write = innodpu_log_write,
	.read	 = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};
/* used by drm dpu */

INNO_EXT_SYM(fh2m_inno_dev_printk);
INNO_EXT_SYM(fh2m_inno_printk);
INNO_EXT_SYM(fh2m_inno_trace_printk);
INNO_EXT_SYM(fh2m_inno_print_hex_dump);
INNO_EXT_SYM(fh2m_innodpu_info);
INNO_EXT_SYM(fh2m_innodpu_warn);
INNO_EXT_SYM(fh2m_innodpu_err);
INNO_EXT_SYM(fh2m_innodpu_log_init);
INNO_EXT_SYM(fh2m_innodpu_log_fini);
INNO_EXT_SYM(fh2m_inno_seq_printf);
INNO_EXT_SYM(fh2m_s_inno_dpu_log_fops);
