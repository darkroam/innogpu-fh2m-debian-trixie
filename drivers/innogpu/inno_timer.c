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
#include <linux/timer.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include "inno_drm_version.h"
#if (DRM_VERSION >= KERNEL_VERSION(4, 11, 0))
#include <linux/sched/clock.h>
#include <linux/sched/signal.h>
#else
#include <linux/sched.h>
#endif /* (DRM_VERSION >= KERNEL_VERSION(4, 11, 0)) */

#include "inno_misc.h"
#include "inno_timer.h"
#include <linux/jiffies.h>
#include <linux/ktime.h>

struct inno_timer {
	struct timer_list timer;
	void (*function)(unsigned long data);
	unsigned long data;
};

unsigned long fh2m_inno_time_jiffies(void)
{
	return jiffies;
}
INNO_EXT_SYM(fh2m_inno_time_jiffies);

bool fh2m_inno_time_after(unsigned long timeout)
{
	return time_after(jiffies, timeout);
}
INNO_EXT_SYM(fh2m_inno_time_after);

unsigned long fh2m_inno_msecs_to_jiffies(unsigned int m)
{
	return msecs_to_jiffies(m);
}
INNO_EXT_SYM(fh2m_inno_msecs_to_jiffies);

unsigned int fh2m_inno_jiffies_to_msecs(unsigned long j)
{
	return jiffies_to_msecs(j);
}
INNO_EXT_SYM(fh2m_inno_jiffies_to_msecs);

unsigned long fh2m_inno_usecs_to_jiffies(unsigned int u)
{
	return usecs_to_jiffies(u);
}
INNO_EXT_SYM(fh2m_inno_usecs_to_jiffies);

unsigned int fh2m_inno_jiffies_to_usecs(unsigned long j)
{
	return jiffies_to_usecs(j);
}
INNO_EXT_SYM(fh2m_inno_jiffies_to_usecs);

bool fh2m_inno_time_before(unsigned long timeout)
{
	return time_before(jiffies, timeout);
}
INNO_EXT_SYM(fh2m_inno_time_before);

struct inno_timer *fh2m_inno_timer_alloc(void)
{
	struct inno_timer *inno_timer = (struct inno_timer *)
		kzalloc(sizeof(struct inno_timer), GFP_KERNEL);
	return inno_timer;
}
INNO_EXT_SYM(fh2m_inno_timer_alloc);

void fh2m_inno_timer_free(struct inno_timer *inno_timer)
{
	kfree(inno_timer);
}
INNO_EXT_SYM(fh2m_inno_timer_free);

#if (DRM_VERSION >= KERNEL_VERSION(4, 15, 0))
static void inno_timer_callback_wrapper(struct timer_list *timer)
{
	struct inno_timer *inno_timer = container_of(timer, struct inno_timer, timer);
	inno_timer->function(inno_timer->data);
}
#else
static void inno_timer_callback_wrapper(unsigned long data)
{
	struct inno_timer *inno_timer = (struct inno_timer *)data;
	inno_timer->function(inno_timer->data);
}
#endif

void fh2m_inno_timer_setup(struct inno_timer *inno_timer,
                      void (*function)(unsigned long data), unsigned long data)
{
	inno_timer->function = function;
	inno_timer->data = data;

#if (DRM_VERSION >= KERNEL_VERSION(4, 15, 0))
	timer_setup(&inno_timer->timer, inno_timer_callback_wrapper, 0);
#else
	init_timer(&inno_timer->timer);
	inno_timer->timer.function = inno_timer_callback_wrapper;
	inno_timer->timer.data = (unsigned long)inno_timer;
#endif
}
INNO_EXT_SYM(fh2m_inno_timer_setup);

void fh2m_inno_timer_start(struct inno_timer *inno_timer, unsigned int delay)
{
	inno_timer->timer.expires = delay + jiffies;
	add_timer(&inno_timer->timer);
}
INNO_EXT_SYM(fh2m_inno_timer_start);

void fh2m_inno_timer_stop(struct inno_timer *inno_timer)
{
	del_timer_sync(&inno_timer->timer);
}
INNO_EXT_SYM(fh2m_inno_timer_stop);

uint64_t fh2m_inno_clockns64(void)
{
	uint64_t timenow;
	preempt_disable();
	timenow = sched_clock();
	preempt_enable();
	return timenow;
}
INNO_EXT_SYM(fh2m_inno_clockns64);

uint64_t fh2m_inno_clockmonotonic(void)
{
	ktime_t time = ktime_get();

#if (DRM_VERSION >= KERNEL_VERSION(4, 10, 0))
	return time;
#else
	return time.tv64;
#endif
}
INNO_EXT_SYM(fh2m_inno_clockmonotonic);

uint64_t fh2m_inno_clockmonotonic_raw(void)
{
#if (DRM_VERSION >= KERNEL_VERSION(5, 6, 0))
	struct timespec64 ts;
	ktime_get_raw_ts64(&ts);
	return ts.tv_sec * 1000000000 + ts.tv_nsec;
#else
	struct timespec ts;
	getrawmonotonic(&ts);
	return (uint64_t) ts.tv_sec * 1000000000 + ts.tv_nsec;
#endif
}
INNO_EXT_SYM(fh2m_inno_clockmonotonic_raw);

void fh2m_inno_ndelay(uint64_t ns)
{
	ndelay(ns);
}
INNO_EXT_SYM(fh2m_inno_ndelay);

void fh2m_inno_udelay(uint64_t us)
{
	udelay(us);
}
INNO_EXT_SYM(fh2m_inno_udelay);

void fh2m_inno_mdelay(unsigned int ms)
{
	mdelay(ms);
}
INNO_EXT_SYM(fh2m_inno_mdelay);

void fh2m_inno_msleep(uint32_t ms)
{
	msleep(ms);
}
INNO_EXT_SYM(fh2m_inno_msleep);

void fh2m_inno_usleep_range(unsigned long min, unsigned long max)
{
	usleep_range(min, max);
}
INNO_EXT_SYM(fh2m_inno_usleep_range);

inno_ktime fh2m_inno_ktime_get(void)
{
#if (DRM_VERSION >= KERNEL_VERSION(4, 10, 0))
	return ktime_get();
#else
	ktime_t ktime = {0};
	ktime = ktime_get();
	return ktime.tv64;
#endif
}
INNO_EXT_SYM(fh2m_inno_ktime_get);

inno_ktime fh2m_inno_ktime_sub(inno_ktime end, inno_ktime start)
{
#if (DRM_VERSION >= KERNEL_VERSION(4, 10, 0))
	return ktime_sub(end, start);
#else
	ktime_t ktime = {0};
	ktime_t inno_start = {0};
	ktime_t inno_end = {0};
	inno_start.tv64 = start;
	inno_end.tv64 = end;
	ktime = ktime_sub(inno_end, inno_start);
	return ktime.tv64;
#endif
}
INNO_EXT_SYM(fh2m_inno_ktime_sub);

int fh2m_inno_ktime_compare(const inno_ktime cmp1, const inno_ktime cmp2)
{
#if (DRM_VERSION >= KERNEL_VERSION(4, 10, 0))
	return ktime_compare(cmp1, cmp2);
#else
	ktime_t inno_cmp1 = {0};
	ktime_t inno_cmp2 = {0};
	inno_cmp1.tv64 = cmp1;
	inno_cmp2.tv64 = cmp2;
	return ktime_compare(inno_cmp1, inno_cmp2);
#endif
}
INNO_EXT_SYM(fh2m_inno_ktime_compare);

inno_ktime fh2m_inno_ktime_add_us(const inno_ktime kt, const u64 usec)
{
#if (DRM_VERSION >= KERNEL_VERSION(4, 10, 0))
	return ktime_add_us(kt, usec);
#else
	ktime_t ktime = {0};
	ktime_t inno_kt = {0};
	inno_kt.tv64 = kt;
	ktime = ktime_add_us(inno_kt, usec);
	return ktime.tv64;
#endif
}
INNO_EXT_SYM(fh2m_inno_ktime_add_us);

s64 fh2m_inno_ktime_to_us(const inno_ktime kt)
{
#if (DRM_VERSION >= KERNEL_VERSION(4, 10, 0))
	return ktime_to_us(kt);
#else
	ktime_t inno_kt = {0};
	inno_kt.tv64 = kt;
	return ktime_to_us(inno_kt);
#endif
}
INNO_EXT_SYM(fh2m_inno_ktime_to_us);
