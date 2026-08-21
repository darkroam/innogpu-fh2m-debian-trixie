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
#ifndef __INNO_DEBUG_H__
#define __INNO_DEBUG_H__

#include "inno_fs.h"
#include "inno_plat_dev.h"
#include <linux/kern_levels.h>

#define LOG_MAX_PRINT_TIMES 20
enum {
	INNO_DUMP_PREFIX_NONE,
	INNO_DUMP_PREFIX_ADDRESS,
	INNO_DUMP_PREFIX_OFFSET
};

#define inno_error(fmt, ...) \
	do { \
		fh2m_inno_printk(KERN_ERR fmt, ##__VA_ARGS__); \
	} while (0);

#define inno_info(fmt, ...) \
	do { \
		fh2m_inno_printk(KERN_INFO fmt, ##__VA_ARGS__); \
	} while (0);

#define inno_emerg(fmt, ...) \
	do { \
		fh2m_inno_printk(KERN_EMERG fmt, ##__VA_ARGS__); \
	} while (0);

#define inno_alert(fmt, ...) \
	do { \
		fh2m_inno_printk(KERN_ALERT fmt, ##__VA_ARGS__); \
	} while (0);

#define inno_crit(fmt, ...) \
	do { \
		fh2m_inno_printk(KERN_CRIT fmt, ##__VA_ARGS__); \
	} while (0);

#define inno_warning(fmt, ...) \
	do { \
		fh2m_inno_printk(KERN_WARNING fmt, ##__VA_ARGS__); \
	} while (0);

#define inno_notice(fmt, ...) \
	do { \
		fh2m_inno_printk(KERN_NOTICE fmt, ##__VA_ARGS__); \
	} while (0);

#define inno_debug(fmt, ...) \
	do { \
		fh2m_inno_printk(KERN_DEBUG fmt, ##__VA_ARGS__); \
	} while (0);

#define inno_error_ratelimited(fmt, ...) \
	do { \
		inno_printk_ratelimited(KERN_ERR fmt, ##__VA_ARGS__); \
	} while (0);

#define inno_info_ratelimited(fmt, ...) \
	do { \
		inno_printk_ratelimited(KERN_INFO fmt, ##__VA_ARGS__); \
	} while (0);

#define inno_emerg_ratelimited(fmt, ...) \
	do { \
		inno_printk_ratelimited(KERN_EMERG fmt, ##__VA_ARGS__); \
	} while (0);

#define inno_alert_ratelimited(fmt, ...) \
	do { \
		inno_printk_ratelimited(KERN_ALERT fmt, ##__VA_ARGS__); \
	} while (0);

#define inno_crit_ratelimited(fmt, ...) \
	do { \
		inno_printk_ratelimited(KERN_CRIT fmt, ##__VA_ARGS__); \
	} while (0);

#define inno_warning_ratelimited(fmt, ...) \
	do { \
		inno_printk_ratelimited(KERN_WARNING fmt, ##__VA_ARGS__); \
	} while (0);

#define inno_notice_ratelimited(fmt, ...) \
	do { \
		inno_printk_ratelimited(KERN_NOTICE fmt, ##__VA_ARGS__); \
	} while (0);

#define inno_debug_ratelimited(fmt, ...) \
	do { \
		inno_printk_ratelimited(KERN_DEBUG fmt, ##__VA_ARGS__); \
	} while (0);

/* =========================== used by drm dpu =========================*/
#define INNODPU_WARN_RETURN_CHECK(_cond, _ret) do { \
	if (!(_cond)) { \
		fh2m_innodpu_warn(NULL, "trigger assert at line %d\n", __LINE__); \
		fh2m_inno_warn_on(0); \
		return (_ret); \
	} \
} while (0)

/* used by err & warn, default printk */
#define DPU_UT_NONE		0x00
/* used by drm & fbdev */
#define DPU_UT_DRM		0x01
/* used by modeset and dpu common printk */
#define DPU_UT_KMS		0x02
/* used by gem */
#define DPU_UT_MEM		0x04
/* used by kms & dpu */
#define DPU_UT_DPU		0x08
/* used by kms & hdmi */
#define DPU_UT_HDMI		0x10
/* used by kms & dp */
#define DPU_UT_DP		0x20
/* used by kms & lvds */
#define DPU_UT_LVDS		0x40
/* used by kms & vga */
#define DPU_UT_VGA		0x80
#define DPU_UT_CURSOR	0x100
#define DPU_UT_GAMMA	0x200

/* used by vkms */
#define DPU_UT_VKMS		0x800

#define inno_drm_info(dev, fmt, ...) fh2m_innodpu_info(dev, DPU_UT_DRM, fmt, ##__VA_ARGS__)
#define inno_drm_warn(dev, fmt, ...) fh2m_innodpu_warn(dev, fmt, ##__VA_ARGS__)
#define inno_drm_err(dev, fmt, ...)   fh2m_innodpu_err(dev, fmt, ##__VA_ARGS__)

#define kms_info(dev, fmt, ...) fh2m_innodpu_info(dev, DPU_UT_KMS, fmt, ##__VA_ARGS__)
#define kms_warn(dev, fmt, ...) fh2m_innodpu_warn(dev, fmt, ##__VA_ARGS__)
#define kms_err(dev, fmt, ...)   fh2m_innodpu_err(dev, fmt, ##__VA_ARGS__)

#define gem_info(dev, fmt, ...) fh2m_innodpu_info(dev, DPU_UT_MEM, fmt, ##__VA_ARGS__)
#define gem_warn(dev, fmt, ...) fh2m_innodpu_warn(dev, fmt, ##__VA_ARGS__)
#define gem_err(dev, fmt, ...)   fh2m_innodpu_err(dev, fmt, ##__VA_ARGS__)

#define dpu_info(dev, fmt, ...) fh2m_innodpu_info(dev, DPU_UT_DPU, fmt, ##__VA_ARGS__)
#define dpu_warn(dev, fmt, ...) fh2m_innodpu_warn(dev, fmt, ##__VA_ARGS__)
#define dpu_err(dev, fmt, ...)   fh2m_innodpu_err(dev, fmt, ##__VA_ARGS__)

#define hdmi_info(dev, fmt, ...) fh2m_innodpu_info(dev, DPU_UT_HDMI, fmt, ##__VA_ARGS__)
#define hdmi_warn(dev, fmt, ...) fh2m_innodpu_warn(dev, fmt, ##__VA_ARGS__)
#define hdmi_err(dev, fmt, ...)   fh2m_innodpu_err(dev, fmt, ##__VA_ARGS__)

#define dp_info(dev, fmt, ...) fh2m_innodpu_info(dev, DPU_UT_DP, fmt, ##__VA_ARGS__)
#define dp_warn(dev, fmt, ...) fh2m_innodpu_warn(dev, fmt, ##__VA_ARGS__)
#define dp_err(dev, fmt, ...)   fh2m_innodpu_err(dev, fmt, ##__VA_ARGS__)

#define lvds_info(dev, fmt, ...) fh2m_innodpu_info(dev, DPU_UT_LVDS, fmt, ##__VA_ARGS__)
#define lvds_warn(dev, fmt, ...) fh2m_innodpu_warn(dev, fmt, ##__VA_ARGS__)
#define lvds_err(dev, fmt, ...)   fh2m_innodpu_err(dev, fmt, ##__VA_ARGS__)

#define vga_info(dev, fmt, ...) fh2m_innodpu_info(dev, DPU_UT_VGA, fmt, ##__VA_ARGS__)
#define vga_warn(dev, fmt, ...) fh2m_innodpu_warn(dev, fmt, ##__VA_ARGS__)
#define vga_err(dev, fmt, ...)   fh2m_innodpu_err(dev, fmt, ##__VA_ARGS__)

#define conn_info(dev, fmt, ...) fh2m_innodpu_info(dev, DPU_UT_KMS, fmt, ##__VA_ARGS__)
#define conn_warn(dev, fmt, ...) fh2m_innodpu_warn(dev, fmt, ##__VA_ARGS__)
#define conn_err(dev, fmt, ...)   fh2m_innodpu_err(dev, fmt, ##__VA_ARGS__)

extern const struct file_operations fh2m_s_inno_dpu_log_fops;
extern unsigned int s_dpu_debug;
void fh2m_innodpu_info(inno_dev *dev, unsigned int category, const char *format, ...);
void fh2m_innodpu_warn(inno_dev *dev, const char *format, ...);
void fh2m_innodpu_err(inno_dev *dev, const char *format, ...);
void fh2m_inno_seq_printf(inno_seq_file *m, const char *f, ...);
int  fh2m_innodpu_log_init(void);
void fh2m_innodpu_log_fini(void);
/* =========================== used by drm dpu =========================*/

void fh2m_inno_dev_printk(char *debug_level, inno_dev *dev, const char *format, ...);
void fh2m_inno_printk(const char *format, ...);
void inno_printk_ratelimited(const char *format, ...);
void fh2m_inno_trace_printk(const char *format, ...);
void fh2m_inno_print_hex_dump(const char *level, const char *prefix_str,
						 int prefix_type, int rowsize, int groupsize,
						 const void *buf, size_t len, bool ascii);

#endif
