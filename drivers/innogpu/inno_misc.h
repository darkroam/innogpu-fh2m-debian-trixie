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
#ifndef __INNO_MISC_H__
#define __INNO_MISC_H__

#include <linux/types.h>
#include <linux/export.h>
#include "inno_fs.h"
#include "inno_plat_dev.h"

/* for compatibility: move few math functions from inno_misc.c to inno_math.c */
#include "inno_math.h"

/* Get a structure's address from the address of a member */
#define INNO_CONTAINER_OF(ptr, type, member) \
	(type *) ((uintptr_t) (ptr) - offsetof(type, member))

#if defined(SEPARATE_BUILD) || defined(CONFIG_DRM_INNO_APU) || defined(CONFIG_GTEST_INNO_DMA)
 #define INNO_EXT_SYM(f) EXPORT_SYMBOL(f)
 #define INNO_EXT_TRACEPOINT_SYM(f) EXPORT_TRACEPOINT_SYMBOL(f)
#else
 #define INNO_EXT_SYM(f)
 #define INNO_EXT_TRACEPOINT_SYM(f)
#endif

typedef __builtin_va_list inno_va_list;
typedef void inno_file_operations;

#define STRINGIFY_1(x) #x
#define STRINGIFY(x) STRINGIFY_1(x)

extern uint32_t gPVRDebugLevel;
extern const uint32_t fh2m_inno_max_order_page;

int fh2m_inno_memcmp(const void *buf0, const void *buf1, uint32_t size);

uint64_t fh2m_inno_strlcat(char *dst, const char *src, uint64_t size);

char *fh2m_inno_strchr(const char *s, int c);

char *fh2m_inno_strrchr(const char *s, int c);

char *fh2m_inno_strstr(const char *s1, const char *s2);

int fh2m_inno_sprintf(char *buf, size_t size, const char *fmt, ...);

int fh2m_inno_vsnprintf(char *buf, uint64_t size, const char *fmt, inno_va_list args);

uint64_t fh2m_inno_strlen(const char *str);

uint64_t fh2m_inno_strnlen(const char *str, uint64_t cnt);

int fh2m_inno_strcmp(const char *str1, const char *str2);

int fh2m_inno_strncmp(const char *str1, const char *str2, uint64_t size);

int fh2m_inno_kstrtou32(const char *s, unsigned int base, uint32_t *res);

int fh2m_inno_kstrtos32(const char *s, unsigned int base, int32_t *res);

int fh2m_inno_kstrtou64(const char *s, unsigned int base, u64 *res);

int fh2m_inno_kstrtos64(const char *s, unsigned int base, s64 *res);

char *fh2m_inno_strsep(char **s, const char *ct);

int fh2m_inno_strcasecmp(const char *s1, const char *s2);

int fh2m_inno_strlcpy(char *dest, const char *src, int size);

char *fh2m_inno_strncpy(char *dest, const char *src, int count);

int fh2m_inno_vsscanf(const char *buf, const char *fmt, inno_va_list args);

int fh2m_inno_sscanf(const char *buf, const char *fmt, ...);

bool fh2m_inno_is_err(const void *ptr);

bool fh2m_inno_is_err_or_null(const void *ptr);

long fh2m_inno_ptr_err(const void *ptr);

void fh2m_inno_bug(void);

void fh2m_inno_bug_on(int condition);

int fh2m_inno_warn_on(int condition);

int fh2m_inno_warn_on_once(int condition);

void fh2m_inno_dump_stack(void);

const char *fh2m_inno_utsname_sysname(void);

const char *fh2m_inno_utsname_release(void);

const char *fh2m_inno_utsname_version(void);

const char *fh2m_inno_utsname_machine(void);

bool fh2m_inno_kernel_version_ge(int x, int y, int z);

bool fh2m_inno_kernel_version_gt(int x, int y, int z);

bool fh2m_inno_kernel_version_lt(int x, int y, int z);

void fh2m_inno_atomic64_set(atomic64_t *v, long long i);

void fh2m_inno_atomic64_add(long long i, atomic64_t *v);

void fh2m_inno_atomic64_sub(long long i, atomic64_t *v);

long long fh2m_inno_atomic64_read(const atomic64_t *v);

void fh2m_inno_atomic64_xor(long long i, atomic64_t *v);

void fh2m_inno_kill_fasync(void **fp);

unsigned int fh2m_inno_kfifo_len(void *fifo);

int fh2m_inno_kfifo_is_full(void *fifo);

void fh2m_inno_kfifo_reset(void *fifo);

void fh2m_inno_kfifo_free(void *fifo);

int fh2m_inno_kfifo_alloc(void *fifo, unsigned int size, gfp_t gfp_mask);

unsigned int fh2m_inno_kfifo_in_spinlocked(void *kfifo, void *buf, unsigned long n, void *lock);

unsigned int fh2m_inno_kfifo_out_spinlocked(void *kfifo, void *buf, unsigned long n, void *lock);

char *fh2m_inno_kasprintf(uint32_t gfp, const char *fmt, ...);

int fh2m_inno_snprintf(char *buf, size_t size, const char *fmt, ...);

unsigned long fh2m_inno_simple_strtoul(const char *cp, char **endp, unsigned int base);

void fh2m_inno_this_module_put(void);

void fh2m_inno_mb(void);

void fh2m_inno_smp_mb(void);

void fh2m_inno_smp_wmb(void);

void fh2m_inno_smp_rmb(void);

void fh2m_inno_cpu_relax(void);

bool fh2m_inno_is_aligned(uint64_t val, int aligned);

uint32_t fh2m_inno_align(uint64_t val, uint64_t aligned);

int fh2m_inno_num_possible_cpus(void);

int fh2m_inno_get_unused_fd_flags_o_cloexec(void);

#if defined (__i386__) || defined(__x86_64__)
unsigned int fh2m_inno_tsc_khz(void);
#endif

uint64_t fh2m_inno_div_round_up(uint64_t val, uint64_t div);

bool fh2m_inno_test_and_set_bit(unsigned long nr, volatile unsigned long *addr);

bool fh2m_inno_test_and_clear_bit(unsigned long nr, volatile unsigned long *addr);

void fh2m_inno_set_bit(unsigned long nr, volatile unsigned long *addr);

void fh2m_inno_clear_bit(unsigned long nr, volatile unsigned long *addr);

int fh2m_inno_ffz(unsigned long word);

uint32_t fh2m_inno_cpu_to_le32(uint32_t val);

uint32_t fh2m_inno_le32_to_cpu(uint32_t val);

uint64_t fh2m_inno_cpu_to_le64(uint64_t val);

uint32_t fh2m_inno_le64_to_cpu(uint64_t val);

uint32_t fh2m_inno_upper_32_bits(uint64_t val);

uint32_t fh2m_inno_lower_32_bits(uint64_t val);

unsigned long fh2m_inno_fls(unsigned long val);

unsigned long fh2m_inno_ffs(unsigned long val);

int fh2m_inno_kernel_config_numa(void);

unsigned int fh2m_inno_get_pkey_id_pkcs7(void);

unsigned int fh2m_inno_hweight8(unsigned int w);

void fh2m_get_kallsyms_lookup_name_address(void);

inno_dentry* fh2m_inno_debugfs_or_procfs_create_dir(char *pszName, inno_dentry* parent);

void fh2m_inno_debugfs_or_procfs_remove_dir(inno_dentry* psEntry);

inno_dentry* fh2m_inno_debugfs_or_procfs_create_file(inno_dev *dev, char *pszName, int uimode, inno_dentry* parent, const inno_file_operations *file_ops);

void fh2m_inno_debugfs_or_procfs_remove_file(inno_dentry* psEntry, char *pszName, inno_dentry* parent);

void *fh2m_inno_get_dfs_file(inno_inode *inode);

void fh2m_inno_get_random_bytes(void *buf, int nbytes);
#endif
