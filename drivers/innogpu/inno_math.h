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
#ifndef __INNO_MATH_H__
#define __INNO_MATH_H__

#include <linux/types.h>

#define INNO_ARRAY_SIZE(ARR) (sizeof(ARR) / sizeof((ARR)[0]))
#define INNO_BIT(b) (1UL << (b))
#define INNO_GENMASK_32(h, l) (((~((unsigned int)0)) - (1UL << (l)) + 1) & \
			((~((unsigned int)0)) >> (32 - 1 - (h))))
#define INNO_GENMASK_64(h, l) (((~((unsigned long long)0)) - (1ULL << (l)) + 1) & \
			((~((unsigned long long)0)) >> (64 - 1 - (h))))
#define INNO_BITS_PER_BYTE (8)
#define INNO_LOWER_32_BITS(n) ((u32)((n) & 0xffffffff))

#define __INNO_ALIGN_KERNEL_MASK(x, mask) (((x) + (mask)) & ~(mask))
#define INNO_ALIGN(x, a) __INNO_ALIGN_KERNEL_MASK(x, (typeof(x))(a) - 1)

#define __bf_shf(x) (__builtin_ffsll(x) - 1)
#define INNO_FIELD_PREP(_mask, _val)						\
	({								\
		((typeof(_mask))(_val) << __bf_shf(_mask)) & (_mask);	\
	})

#define INNO_FIELD_GET(_mask, _reg)						\
	({								\
		(typeof(_mask))(((_reg) & (_mask)) >> __bf_shf(_mask));	\
	})

unsigned long fh2m_inno_abs(unsigned long val);

int fh2m_inno_max_type_int(int x, int y);

size_t fh2m_inno_max(size_t a, size_t b);

size_t fh2m_inno_min(size_t a, size_t b);

int fh2m_inno_min_type_int(int a, int b);

uint64_t fh2m_inno_div(uint64_t divident, uint32_t divisor, uint32_t *remainder);

size_t fh2m_inno_size_round_up(size_t a, size_t b);

uint32_t fh2m_inno_crc32(uint32_t seed, unsigned char *data, size_t length);

int fh2m_inno_int_min(void);

size_t fh2m_inno_bits_to_longs(int nr);

#endif /* __INNO_MATH_H__ */

