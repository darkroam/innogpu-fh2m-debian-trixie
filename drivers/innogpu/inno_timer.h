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
#ifndef __INNO_TIMER_H__
#define __INNO_TIMER_H__

#include <linux/types.h>

typedef s64 inno_ktime;

struct inno_timer;

unsigned long fh2m_inno_time_jiffies(void);

bool fh2m_inno_time_after(unsigned long timeout);

unsigned long fh2m_inno_msecs_to_jiffies(unsigned int m);

unsigned int fh2m_inno_jiffies_to_msecs(unsigned long j);

unsigned long fh2m_inno_usecs_to_jiffies(unsigned int u);

unsigned int fh2m_inno_jiffies_to_usecs(unsigned long j);

bool fh2m_inno_time_before(unsigned long timeout);

struct inno_timer *fh2m_inno_timer_alloc(void);

void fh2m_inno_timer_free(struct inno_timer *inno_timer);

void fh2m_inno_timer_setup(struct inno_timer *inno_timer,
		void (*function)(unsigned long data), unsigned long data);

void fh2m_inno_timer_start(struct inno_timer *inno_timer, unsigned int delay);

void fh2m_inno_timer_stop(struct inno_timer *inno_timer);

uint64_t fh2m_inno_clockns64(void);

uint64_t fh2m_inno_clockmonotonic(void);

uint64_t fh2m_inno_clockmonotonic_raw(void);

void fh2m_inno_ndelay(uint64_t ns);

void fh2m_inno_udelay(uint64_t us);

void fh2m_inno_msleep(uint32_t ms);

void fh2m_inno_mdelay(unsigned int ms);

void fh2m_inno_usleep_range(unsigned long min, unsigned long max);

inno_ktime fh2m_inno_ktime_get(void);

inno_ktime fh2m_inno_ktime_sub(inno_ktime end, inno_ktime start);

int fh2m_inno_ktime_compare(const inno_ktime cmp1, const inno_ktime cmp2);

inno_ktime fh2m_inno_ktime_add_us(const inno_ktime kt, const u64 usec);

s64 fh2m_inno_ktime_to_us(const inno_ktime kt);

#endif

