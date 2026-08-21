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

#include <linux/moduleparam.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <asm/div64.h>
#include <linux/bug.h>
#include <linux/err.h>
#include <linux/utsname.h>
#include <linux/kfifo.h>
#include <linux/module.h>
#include <linux/random.h>
#include <linux/bitops.h>
#include <linux/debugfs.h>
#include <linux/proc_fs.h>
#include <linux/mmzone.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 10, 0))
#include <linux/fs.h>
#endif

#include "inno_misc.h"
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#include <linux/module_signature.h>
#else
#define PKEY_ID_PKCS7 2
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0) */

const uint32_t inno_gfp_kernel = GFP_KERNEL;

#if defined(INNOGPU_MAX_ORDER_PRESENT)
const uint32_t fh2m_inno_max_order_page = MAX_ORDER;
#else
const uint32_t fh2m_inno_max_order_page = MAX_PAGE_ORDER;
#endif
INNO_EXT_SYM(fh2m_inno_max_order_page);

uint32_t gPVRDebugLevel = 0;
module_param(gPVRDebugLevel, uint, 0644);
MODULE_PARM_DESC(gPVRDebugLevel,
		"Sets the level of debug output (default 0x7)");

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 7, 0)) && defined(CONFIG_LOONGARCH)
#include <linux/kprobes.h>
typedef unsigned long (*kallsyms_lookup_name_t)(const char *name);
extern kallsyms_lookup_name_t inno_kallsyms_lookup_name_ptr;
static struct kprobe kp = {
	.symbol_name = "kallsyms_lookup_name"
};
#endif

void fh2m_get_kallsyms_lookup_name_address(void)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 7, 0)) && defined(CONFIG_LOONGARCH)
	register_kprobe(&kp);
	inno_kallsyms_lookup_name_ptr = (kallsyms_lookup_name_t) kp.addr;
	unregister_kprobe(&kp);
#endif
}
INNO_EXT_SYM(fh2m_get_kallsyms_lookup_name_address);

int fh2m_inno_memcmp(const void *buf0, const void *buf1, uint32_t size)
{
	return memcmp(buf0, buf1, size);
}
INNO_EXT_SYM(fh2m_inno_memcmp);

uint64_t fh2m_inno_strlcat(char *dst, const char *src, uint64_t size)
{
	return strlcat(dst, src, size);
}
INNO_EXT_SYM(fh2m_inno_strlcat);

char *fh2m_inno_strchr(const char *s, int c)
{
	return strchr(s, c);
}
INNO_EXT_SYM(fh2m_inno_strchr);

char *fh2m_inno_strrchr(const char *s, int c)
{
	return strrchr(s, c);
}
INNO_EXT_SYM(fh2m_inno_strrchr);

char *fh2m_inno_strstr(const char *s1, const char *s2)
{
	return strstr(s1, s2);
}
INNO_EXT_SYM(fh2m_inno_strstr);

int fh2m_inno_sprintf(char *buf, size_t size, const char *fmt, ...)
{
	va_list arg_list;
	int count;

	va_start(arg_list, fmt);
	count = fh2m_inno_vsnprintf(buf, size, fmt, arg_list);
	va_end(arg_list);

	return count;
}
INNO_EXT_SYM(fh2m_inno_sprintf);

int fh2m_inno_vsnprintf(char *buf, uint64_t size, const char *fmt, inno_va_list args)
{
	return vsnprintf(buf, size, fmt, args);
}
INNO_EXT_SYM(fh2m_inno_vsnprintf);

char *fh2m_inno_kasprintf(uint32_t gfp, const char *fmt, ...)
{
	va_list ap;
	char *p;

	va_start(ap, fmt);
	p = kvasprintf(gfp, fmt, ap);
	va_end(ap);

	return p;
}
INNO_EXT_SYM(fh2m_inno_kasprintf);

int fh2m_inno_snprintf(char *buf, size_t size, const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i = vsnprintf(buf, size, fmt, args);
	va_end(args);

	return i;
}
INNO_EXT_SYM(fh2m_inno_snprintf);


uint64_t fh2m_inno_strlen(const char *str)
{
	return strlen(str);
}
INNO_EXT_SYM(fh2m_inno_strlen);

uint64_t fh2m_inno_strnlen(const char *str, uint64_t cnt)
{
	return strnlen(str, cnt);
}
INNO_EXT_SYM(fh2m_inno_strnlen);

int fh2m_inno_strncmp(const char *str1, const char *str2, uint64_t size)
{
	return strncmp(str1, str2, size);
}
INNO_EXT_SYM(fh2m_inno_strncmp);

int fh2m_inno_strcmp(const char *str1, const char *str2)
{
	return strcmp(str1, str2);
}
INNO_EXT_SYM(fh2m_inno_strcmp);

int fh2m_inno_kstrtou32(const char *s, unsigned int base, uint32_t *res)
{
	return kstrtou32(s, base, res);
}
INNO_EXT_SYM(fh2m_inno_kstrtou32);

int fh2m_inno_kstrtos32(const char *s, unsigned int base, int32_t *res)
{
	return kstrtos32(s, base, res);
}
INNO_EXT_SYM(fh2m_inno_kstrtos32);

int fh2m_inno_kstrtou64(const char *s, unsigned int base, u64 *res)
{
	return kstrtou64(s, base, res);
}
INNO_EXT_SYM(fh2m_inno_kstrtou64);

int fh2m_inno_kstrtos64(const char *s, unsigned int base, s64 *res)
{
	return kstrtos64(s, base, res);
}
INNO_EXT_SYM(fh2m_inno_kstrtos64);

char *fh2m_inno_strsep(char **s, const char *ct)
{
	return strsep(s, ct);
}
INNO_EXT_SYM(fh2m_inno_strsep);

int fh2m_inno_strcasecmp(const char *s1, const char *s2)
{
	return strcasecmp(s1, s2);
}
INNO_EXT_SYM(fh2m_inno_strcasecmp);

int fh2m_inno_strlcpy(char *dest, const char *src, int size)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6,8,0))
	return strlcpy(dest, src, size);
#else
	return strscpy(dest, src, size);
#endif
}
INNO_EXT_SYM(fh2m_inno_strlcpy);

char *fh2m_inno_strncpy(char *dest, const char *src, int count)
{
	return strncpy(dest, src, count);
}
INNO_EXT_SYM(fh2m_inno_strncpy);

int fh2m_inno_vsscanf(const char *buf, const char *fmt, inno_va_list args)
{
	return vsscanf(buf, fmt, args);
}
INNO_EXT_SYM(fh2m_inno_vsscanf);

int fh2m_inno_sscanf(const char *buf, const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i = vsscanf(buf, fmt, args);
	va_end(args);

	return i;
}
INNO_EXT_SYM(fh2m_inno_sscanf);

bool fh2m_inno_is_err(const void *ptr)
{
	return IS_ERR(ptr);
}
INNO_EXT_SYM(fh2m_inno_is_err);

bool fh2m_inno_is_err_or_null(const void *ptr)
{
	return IS_ERR_OR_NULL(ptr);
}
INNO_EXT_SYM(fh2m_inno_is_err_or_null);

long fh2m_inno_ptr_err(const void *ptr)
{
	return PTR_ERR(ptr);
}
INNO_EXT_SYM(fh2m_inno_ptr_err);

void fh2m_inno_bug(void)
{
	BUG();
}
INNO_EXT_SYM(fh2m_inno_bug);

void fh2m_inno_bug_on(int condition)
{
	BUG_ON(condition);
}
INNO_EXT_SYM(fh2m_inno_bug_on);

int fh2m_inno_warn_on(int condition)
{
	return WARN_ON(condition);
}
INNO_EXT_SYM(fh2m_inno_warn_on);

int fh2m_inno_warn_on_once(int condition)
{
	return WARN_ON_ONCE(condition);
}
INNO_EXT_SYM(fh2m_inno_warn_on_once);

const char *fh2m_inno_utsname_sysname(void)
{
	return utsname()->sysname;
}
INNO_EXT_SYM(fh2m_inno_utsname_sysname);

const char *fh2m_inno_utsname_release(void)
{
	return utsname()->release;
}
INNO_EXT_SYM(fh2m_inno_utsname_release);

const char *fh2m_inno_utsname_version(void)
{
	return utsname()->version;
}
INNO_EXT_SYM(fh2m_inno_utsname_version);

const char *fh2m_inno_utsname_machine(void)
{
	return utsname()->machine;
}
INNO_EXT_SYM(fh2m_inno_utsname_machine);

bool fh2m_inno_kernel_version_ge(int x, int y, int z)
{
	return LINUX_VERSION_CODE >= KERNEL_VERSION(x, y, z);
}
INNO_EXT_SYM(fh2m_inno_kernel_version_ge);

bool fh2m_inno_kernel_version_gt(int x, int y, int z)
{
	return LINUX_VERSION_CODE > KERNEL_VERSION(x, y, z);
}
INNO_EXT_SYM(fh2m_inno_kernel_version_gt);

bool fh2m_inno_kernel_version_lt(int x, int y, int z)
{
	return LINUX_VERSION_CODE < KERNEL_VERSION(x, y, z);
}
INNO_EXT_SYM(fh2m_inno_kernel_version_lt);

void fh2m_inno_atomic64_set(atomic64_t *v, long long i)
{
	atomic64_set(v, i);
}
INNO_EXT_SYM(fh2m_inno_atomic64_set);

void fh2m_inno_atomic64_add(long long i, atomic64_t *v)
{
	atomic64_add(i, v);
}
INNO_EXT_SYM(fh2m_inno_atomic64_add);

void fh2m_inno_atomic64_sub(long long i, atomic64_t *v)
{
	atomic64_sub(i, v);
}
INNO_EXT_SYM(fh2m_inno_atomic64_sub);

long long fh2m_inno_atomic64_read(const atomic64_t *v)
{
	return atomic64_read(v);
}
INNO_EXT_SYM(fh2m_inno_atomic64_read);

void fh2m_inno_atomic64_xor(long long i, atomic64_t *v)
{
	atomic64_xor(i, v);
}
INNO_EXT_SYM(fh2m_inno_atomic64_xor);

void fh2m_inno_kill_fasync(void **fp)
{
	kill_fasync((struct fasync_struct **)fp, SIGIO, POLL_IN);
}
INNO_EXT_SYM(fh2m_inno_kill_fasync);

unsigned int fh2m_inno_kfifo_len(void *fifo)
{
	return kfifo_len((struct kfifo *)fifo);
}
INNO_EXT_SYM(fh2m_inno_kfifo_len);

int fh2m_inno_kfifo_is_full(void *fifo)
{
	return kfifo_is_full((struct kfifo *)fifo);
}
INNO_EXT_SYM(fh2m_inno_kfifo_is_full);

void fh2m_inno_kfifo_reset(void *fifo)
{
	kfifo_reset((struct kfifo *)fifo);
}
INNO_EXT_SYM(fh2m_inno_kfifo_reset);

void fh2m_inno_kfifo_free(void *fifo)
{
	kfifo_free((struct kfifo *)fifo);
}
INNO_EXT_SYM(fh2m_inno_kfifo_free);

int fh2m_inno_kfifo_alloc(void *fifo, unsigned int size, gfp_t gfp_mask)
{
	return kfifo_alloc((struct kfifo *)fifo, size, gfp_mask);
}
INNO_EXT_SYM(fh2m_inno_kfifo_alloc);

unsigned int fh2m_inno_kfifo_in_spinlocked(void *kfifo, void *buf, unsigned long n, void *lock)
{
	return kfifo_in_spinlocked((struct kfifo *)kfifo, buf, n, (spinlock_t *)lock);
}
INNO_EXT_SYM(fh2m_inno_kfifo_in_spinlocked);

unsigned int fh2m_inno_kfifo_out_spinlocked(void *kfifo, void *buf, unsigned long n, void *lock)
{
	return kfifo_out_spinlocked((struct kfifo *)kfifo, buf, n, (spinlock_t *)lock);
}
INNO_EXT_SYM(fh2m_inno_kfifo_out_spinlocked);

unsigned long fh2m_inno_simple_strtoul(const char *cp, char **endp, unsigned int base)
{
	return simple_strtoull(cp, endp, base);
}
INNO_EXT_SYM(fh2m_inno_simple_strtoul);

void fh2m_inno_this_module_put(void)
{
	module_put(THIS_MODULE);
}
INNO_EXT_SYM(fh2m_inno_this_module_put);

void fh2m_inno_mb(void)
{
	mb();
}
INNO_EXT_SYM(fh2m_inno_mb);

void fh2m_inno_smp_mb(void)
{
	smp_mb();
}
INNO_EXT_SYM(fh2m_inno_smp_mb);

void fh2m_inno_smp_wmb(void)
{
	smp_wmb();
}
INNO_EXT_SYM(fh2m_inno_smp_wmb);

void fh2m_inno_smp_rmb(void)
{
	smp_rmb();
}
INNO_EXT_SYM(fh2m_inno_smp_rmb);

void fh2m_inno_cpu_relax(void)
{
	cpu_relax();
}
INNO_EXT_SYM(fh2m_inno_cpu_relax);

bool fh2m_inno_is_aligned(uint64_t val, int aligned)
{
	return IS_ALIGNED((unsigned long)val, aligned);
}
INNO_EXT_SYM(fh2m_inno_is_aligned);

uint32_t fh2m_inno_align(uint64_t val, uint64_t aligned)
{
	return ALIGN(val, aligned);
}
INNO_EXT_SYM(fh2m_inno_align);

int fh2m_inno_num_possible_cpus(void)
{
	return num_possible_cpus();
}
INNO_EXT_SYM(fh2m_inno_num_possible_cpus);

void fh2m_inno_get_random_bytes(void *buf, int nbytes)
{
	get_random_bytes(buf, nbytes);
}
INNO_EXT_SYM(fh2m_inno_get_random_bytes);

#if defined (__i386__) || defined(__x86_64__)
extern unsigned int tsc_khz;
unsigned int fh2m_inno_tsc_khz(void)
{
	return tsc_khz;
}
INNO_EXT_SYM(fh2m_inno_tsc_khz);
#endif

uint64_t fh2m_inno_div_round_up(uint64_t val, uint64_t div)
{
	return DIV_ROUND_UP(val, div);
}
INNO_EXT_SYM(fh2m_inno_div_round_up);

bool fh2m_inno_test_and_set_bit(unsigned long nr, volatile unsigned long *addr)
{
	return test_and_set_bit(nr, addr);
}
INNO_EXT_SYM(fh2m_inno_test_and_set_bit);

bool fh2m_inno_test_and_clear_bit(unsigned long nr, volatile unsigned long *addr)
{
	return test_and_clear_bit(nr, addr);
}
INNO_EXT_SYM(fh2m_inno_test_and_clear_bit);

void fh2m_inno_set_bit(unsigned long nr, volatile unsigned long *addr)
{
	set_bit(nr, addr);
}
INNO_EXT_SYM(fh2m_inno_set_bit);

void fh2m_inno_clear_bit(unsigned long nr, volatile unsigned long *addr)
{
	clear_bit(nr, addr);
}
INNO_EXT_SYM(fh2m_inno_clear_bit);

int fh2m_inno_ffz(unsigned long word)
{
	return ffz(word);
}
INNO_EXT_SYM(fh2m_inno_ffz);

uint32_t fh2m_inno_cpu_to_le32(uint32_t val)
{
	return cpu_to_le32(val);
}
INNO_EXT_SYM(fh2m_inno_cpu_to_le32);

uint32_t fh2m_inno_le32_to_cpu(uint32_t val)
{
	return le32_to_cpu(val);
}
INNO_EXT_SYM(fh2m_inno_le32_to_cpu);

uint64_t fh2m_inno_cpu_to_le64(uint64_t val)
{
	return cpu_to_le64(val);
}
INNO_EXT_SYM(fh2m_inno_cpu_to_le64);

uint32_t fh2m_inno_le64_to_cpu(uint64_t val)
{
	return le64_to_cpu(val);
}
INNO_EXT_SYM(fh2m_inno_le64_to_cpu);

uint32_t fh2m_inno_upper_32_bits(uint64_t val)
{
	return upper_32_bits(val);
}
INNO_EXT_SYM(fh2m_inno_upper_32_bits);

uint32_t fh2m_inno_lower_32_bits(uint64_t val)
{
	return lower_32_bits(val);
}
INNO_EXT_SYM(fh2m_inno_lower_32_bits);

unsigned long fh2m_inno_fls(unsigned long val)
{
	return __fls(val);
}
INNO_EXT_SYM(fh2m_inno_fls);

unsigned long fh2m_inno_ffs(unsigned long val)
{
	return __ffs(val);
}
INNO_EXT_SYM(fh2m_inno_ffs);

int fh2m_inno_kernel_config_numa(void)
{
#if defined(CONFIG_NUMA)
	return 1;
#else
	return 0;
#endif
}
INNO_EXT_SYM(fh2m_inno_kernel_config_numa);

unsigned int fh2m_inno_get_pkey_id_pkcs7(void)
{
	return PKEY_ID_PKCS7;
}
INNO_EXT_SYM(fh2m_inno_get_pkey_id_pkcs7);

unsigned int fh2m_inno_hweight8(unsigned int w)
{
	return hweight8(w);
}
INNO_EXT_SYM(fh2m_inno_hweight8);

inno_dentry* fh2m_inno_debugfs_or_procfs_create_dir(char *pszName, inno_dentry* parent)
{
#if defined(CONFIG_DEBUG_FS)
	return debugfs_create_dir(pszName, parent);
#elif defined(CONFIG_PROC_FS)
	return proc_mkdir(pszName, parent);;
#endif /* defined(CONFIG_DEBUG_FS) || defined(CONFIG_PROC_FS) */
}
INNO_EXT_SYM(fh2m_inno_debugfs_or_procfs_create_dir);

void fh2m_inno_debugfs_or_procfs_remove_dir(inno_dentry* psEntry)
{
#if defined(CONFIG_DEBUG_FS)
	debugfs_remove(psEntry);
#elif defined(CONFIG_PROC_FS)
	proc_remove(psEntry);
#endif /* defined(CONFIG_DEBUG_FS) || defined(CONFIG_PROC_FS) */
}
INNO_EXT_SYM(fh2m_inno_debugfs_or_procfs_remove_dir);

inno_dentry* fh2m_inno_debugfs_or_procfs_create_file(inno_dev *dev, char *pszName, int uimode, inno_dentry* parent, const inno_file_operations *file_ops)
{
#if defined(CONFIG_DEBUG_FS)
	return debugfs_create_file(pszName, uimode, parent, dev, file_ops);
#elif defined(CONFIG_PROC_FS)
	return proc_create_data(pszName, uimode, parent, file_ops, dev);
#endif /* defined(CONFIG_DEBUG_FS) || defined(CONFIG_PROC_FS) */
}
INNO_EXT_SYM(fh2m_inno_debugfs_or_procfs_create_file);

void fh2m_inno_debugfs_or_procfs_remove_file(inno_dentry* psEntry, char *pszName, inno_dentry* parent)
{
#if defined(CONFIG_DEBUG_FS)
	debugfs_remove(psEntry);
#elif defined(CONFIG_PROC_FS)
	remove_proc_entry(pszName, parent);
#endif /* defined(CONFIG_DEBUG_FS) || defined(CONFIG_PROC_FS) */
}
INNO_EXT_SYM(fh2m_inno_debugfs_or_procfs_remove_file);

void *fh2m_inno_get_dfs_file(inno_inode *inode)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,17,0))
	return PDE_DATA((struct inode *)inode);
#else
	return pde_data((struct inode *)inode);
#endif
}
INNO_EXT_SYM(fh2m_inno_get_dfs_file);
