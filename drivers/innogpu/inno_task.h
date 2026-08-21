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
#ifndef __INNO_TASK_H__
#define __INNO_TASK_H__

#include <linux/types.h>
#include "inno_misc.h"

typedef void inno_task;
typedef void inno_workqueue;
typedef void inno_work;
typedef void inno_dwork;
typedef void inno_pid;

#define INNO_WQ_UNBOUND (1 << 1)
#define INNO_WQ_FREEZABLE (1 << 2)
#define INNO_WQ_HIGHPRI             (1 << 4)
#define INNO_WQ_MAX_ACTIVE          512
#define INNO_WQ_MAX_UNBOUND_PER_CPU 4
/* unbound wq's aren't per-cpu, scale max_active according to #cpus */
#define INNO_WQ_UNBOUND_MAX_ACTIVE	\
	fh2m_inno_max_type_int(INNO_WQ_MAX_ACTIVE, fh2m_inno_num_possible_cpus() * INNO_WQ_MAX_UNBOUND_PER_CPU)

#define inno_might_sleep_if(cond) do { if (cond) fh2m_inno_might_sleep(); } while (0)

inno_task *fh2m_inno_get_current_task(void);

unsigned int fh2m_inno_get_current_policy(void);

void fh2m_inno_enable_oom_killer(void);

void fh2m_inno_disable_oom_killer(void);

void fh2m_inno_task_set_current_state(long state);

void fh2m_inno_task_schedule(void);

void fh2m_inno_task_set_user_nice(inno_task *task, long nice);

int fh2m_inno_task_get_current_nice(void);

int fh2m_inno_task_stop(inno_task *task);

void fh2m_inno_tasklet_kill(inno_task *task);

void fh2m_inno_do_exit(long code);

uint64_t fh2m_inno_task_size(void);

uint32_t fh2m_inno_task_id(void);

uint32_t fh2m_inno_task_pid(void);

uint32_t fh2m_inno_task_pid_vnr(void);

uint32_t fh2m_inno_task_virtual_id(void);

inno_pid *fh2m_inno_find_vpid(uint32_t nr);

char *fh2m_inno_get_task_name_by_pid(uint32_t nr);

int fh2m_inno_kill_pid(inno_pid *pid, int sig, int priv);

char *fh2m_inno_task_name(void);

bool fh2m_inno_task_is_in_kernel(void);

#ifdef CONFIG_FAULT_INJECTION
void fh2m_inno_task_set_current_make_fail(bool val);
#endif

bool fh2m_inno_set_freezable(void);

bool fh2m_inno_kthread_freezable_should_stop(void);

void fh2m_inno_get_task_struct(inno_task *task);

void fh2m_inno_put_task_struct(inno_task *task);

inno_task *fh2m_inno_task_create(int (*func)(void *data), void *data, const char name[]);

inno_workqueue *fh2m_inno_create_freezable_wq(const char *name);

inno_workqueue *fh2m_inno_create_workqueue(const char *name);

inno_workqueue *fh2m_inno_create_singlethread_wq(const char *name);

inno_workqueue *fh2m_inno_alloc_workqueue(const char *name, unsigned int flags, int max_active);

void fh2m_inno_destroy_workqueue(inno_workqueue *wq);

void *fh2m_inno_work_alloc(void (*func)(void *data), void *data);

void fh2m_inno_work_destroy(inno_work *work);

void *fh2m_inno_dwork_alloc(void (*func)(void *data), void *data);

int fh2m_inno_queue_dwork(inno_workqueue *wq, inno_dwork *dwk, unsigned int msec);

int fh2m_inno_schedule_dwork(inno_dwork *dwk, unsigned int msec);

void fh2m_inno_flush_scheduled_work(void);

int fh2m_inno_mod_dwork(inno_dwork *dwk, unsigned int msec);

int fh2m_inno_cancel_dwork(inno_dwork *dwk);

int fh2m_inno_cancel_dwork_sync(inno_dwork *dwk);

void fh2m_inno_dwork_destroy(void *dwork);

int fh2m_inno_queue_work(inno_workqueue *wq, inno_work *wk);

int fh2m_inno_work_pending(void *wk);

int fh2m_inno_queue_work_on(int cpu, inno_workqueue *wq, void *wk);

bool fh2m_inno_schedule_work(void *wk);

bool fh2m_inno_cancel_work_sync(void *wk);

void fh2m_inno_flush_workqueue(inno_workqueue *wq);

bool fh2m_inno_flush_work(inno_work *work);

signed long  fh2m_inno_schedule_timeout(signed long timeout);

void fh2m_inno_might_sleep(void);
unsigned int fh2m_inno_work_busy(inno_work *work);

int fh2m_inno_get_task_interruptible(void);

int fh2m_inno_get_task_killable(void);

int fh2m_inno_get_flag_exclusive(void);

int fh2m_inno_get_task_uninterruptible(void);
#endif

