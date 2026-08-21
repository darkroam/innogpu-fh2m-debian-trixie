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
#include <linux/kernel.h>
#include <linux/crc32.h>
#include <asm/div64.h>

#include "inno_misc.h"
#include "inno_math.h"

unsigned long  fh2m_inno_abs(unsigned long val)
{
	return abs(val);
}
INNO_EXT_SYM(fh2m_inno_abs);

int fh2m_inno_max_type_int(int x, int y)
{
	return max_t(int, x, y);
}
INNO_EXT_SYM(fh2m_inno_max_type_int);

size_t fh2m_inno_max(size_t a, size_t b)
{
	return max(a, b);
}
INNO_EXT_SYM(fh2m_inno_max);

size_t fh2m_inno_min(size_t a, size_t b)
{
	return min(a, b);
}
INNO_EXT_SYM(fh2m_inno_min);

int fh2m_inno_min_type_int(int a, int b)
{
	return min_t(int, a, b);
}
INNO_EXT_SYM(fh2m_inno_min_type_int);

uint64_t fh2m_inno_div(uint64_t divident, uint32_t divisor, uint32_t *remainder)
{
	*remainder = do_div(divident, divisor);
	return divident;
}
INNO_EXT_SYM(fh2m_inno_div);

size_t fh2m_inno_size_round_up(size_t a, size_t b)
{
	return round_up(a, b);
}
INNO_EXT_SYM(fh2m_inno_size_round_up);

uint32_t fh2m_inno_crc32(uint32_t seed, unsigned char *data, size_t length)
{
    return crc32(seed, data, length);
}
INNO_EXT_SYM(fh2m_inno_crc32);

int fh2m_inno_int_min(void)
{
	return INT_MIN;
}
INNO_EXT_SYM(fh2m_inno_int_min);

size_t fh2m_inno_bits_to_longs(int nr)
{
	return BITS_TO_LONGS(nr);
}
INNO_EXT_SYM(fh2m_inno_bits_to_longs);
