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
#include <linux/kthread.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/freezer.h>
#include <linux/io.h>
#include <linux/pid.h>
#include <linux/sched.h>
#include <linux/interrupt.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0))
#include <linux/sched/signal.h>
#else
#include <linux/sched.h>
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)) */

#include "inno_misc.h"
#include "inno_task.h"

#define KERNEL_ID 0xffffffffL

struct inno_work {
	struct work_struct work;
	void (*func)(void *data);
	void *data;
};

struct inno_dwork {
	struct delayed_work dwork;
	void (*func)(void *data);
	void *data;
};

inno_task *fh2m_inno_get_current_task(void)
{
	return current;
}
INNO_EXT_SYM(fh2m_inno_get_current_task);

unsigned int fh2m_inno_get_current_policy(void)
{
	return current->policy;
}
INNO_EXT_SYM(fh2m_inno_get_current_policy);

void fh2m_inno_enable_oom_killer(void)
{
	current->flags &= ~PF_DUMPCORE;
}
INNO_EXT_SYM(fh2m_inno_enable_oom_killer);

void fh2m_inno_disable_oom_killer(void)
{
	WARN_ON(current->flags & PF_DUMPCORE);
	current->flags |= PF_DUMPCORE;
}
INNO_EXT_SYM(fh2m_inno_disable_oom_killer);

void fh2m_inno_task_set_current_state(long state)
{
	set_current_state(TASK_INTERRUPTIBLE);
}
INNO_EXT_SYM(fh2m_inno_task_set_current_state);

void fh2m_inno_task_schedule()
{
	schedule();
}
INNO_EXT_SYM(fh2m_inno_task_schedule);

void fh2m_inno_task_set_user_nice(inno_task *task, long nice)
{
	set_user_nice((struct task_struct *)task, nice);
}
INNO_EXT_SYM(fh2m_inno_task_set_user_nice);

int fh2m_inno_task_get_current_nice(void)
{
	return task_nice(current);
}
INNO_EXT_SYM(fh2m_inno_task_get_current_nice);

int fh2m_inno_task_stop(inno_task *task)
{
	return kthread_stop((struct task_struct *)task);
}
INNO_EXT_SYM(fh2m_inno_task_stop);

void fh2m_inno_tasklet_kill(inno_task *task)
{
	tasklet_kill((struct tasklet_struct *)task);
}
INNO_EXT_SYM(fh2m_inno_tasklet_kill);

void fh2m_inno_do_exit(long code)
{
#ifdef INNOGPU_DO_EXIT_PRESENT
	do_exit(code);
#endif
}
INNO_EXT_SYM(fh2m_inno_do_exit);

uint64_t fh2m_inno_task_size(void)
{
	return TASK_SIZE;
}
INNO_EXT_SYM(fh2m_inno_task_size);

uint32_t fh2m_inno_task_id(void)
{
	if (in_interrupt())
		return KERNEL_ID;

	return task_tgid_nr(current);
}
INNO_EXT_SYM(fh2m_inno_task_id);

uint32_t fh2m_inno_task_pid(void)
{
	if (in_interrupt())
		return KERNEL_ID;

	return current->pid;
}
INNO_EXT_SYM(fh2m_inno_task_pid);

uint32_t fh2m_inno_task_pid_vnr(void)
{
	if (in_interrupt())
		return KERNEL_ID;

	return task_pid_vnr(current);
}
INNO_EXT_SYM(fh2m_inno_task_pid_vnr);

uint32_t fh2m_inno_task_virtual_id(void)
{
	if (in_interrupt())
		return KERNEL_ID;

	return task_tgid_vnr(current);
}
INNO_EXT_SYM(fh2m_inno_task_virtual_id);

inno_pid *fh2m_inno_find_vpid(uint32_t nr)
{
	return find_vpid(nr);
}
INNO_EXT_SYM(fh2m_inno_find_vpid);

char *fh2m_inno_get_task_name_by_pid(uint32_t nr)
{
	struct task_struct *task = pid_task(find_vpid(nr), PIDTYPE_PID);
	return task ? task->comm : NULL;
}
INNO_EXT_SYM(fh2m_inno_get_task_name_by_pid);

int fh2m_inno_kill_pid(inno_pid *pid, int sig, int priv)
{
	return kill_pid((struct pid *)pid, sig, priv);
}
INNO_EXT_SYM(fh2m_inno_kill_pid);

char *fh2m_inno_task_name(void)
{
	return current->comm;
}
INNO_EXT_SYM(fh2m_inno_task_name);

bool fh2m_inno_task_is_in_kernel(void)
{
	return current->mm == NULL;
}
INNO_EXT_SYM(fh2m_inno_task_is_in_kernel);

#ifdef CONFIG_FAULT_INJECTION
void fh2m_inno_task_set_current_make_fail(bool val)
{
	return current->make_it_fail = val;
}
INNO_EXT_SYM(fh2m_inno_task_set_current_make_fail);
#endif

bool fh2m_inno_set_freezable(void)
{
	return set_freezable();
}
INNO_EXT_SYM(fh2m_inno_set_freezable);

bool fh2m_inno_kthread_freezable_should_stop(void)
{
	return kthread_freezable_should_stop(NULL);
}
INNO_EXT_SYM(fh2m_inno_kthread_freezable_should_stop);

void fh2m_inno_get_task_struct(inno_task *task)
{
	get_task_struct((struct task_struct *)task);
}
INNO_EXT_SYM(fh2m_inno_get_task_struct);

void fh2m_inno_put_task_struct(inno_task *task)
{
	put_task_struct((struct task_struct *)task);
}
INNO_EXT_SYM(fh2m_inno_put_task_struct);

inno_task *fh2m_inno_task_create(int (*func)(void *data), void *data, const char name[])
{
	return kthread_run(func, data, "%s", name);
}
INNO_EXT_SYM(fh2m_inno_task_create);

inno_workqueue *fh2m_inno_create_freezable_wq(const char *name)
{
	return create_freezable_workqueue(name);
}
INNO_EXT_SYM(fh2m_inno_create_freezable_wq);

inno_workqueue *fh2m_inno_create_workqueue(const char *name)
{
	return create_workqueue(name);
}
INNO_EXT_SYM(fh2m_inno_create_workqueue);

inno_workqueue *fh2m_inno_create_singlethread_wq(const char *name)
{
	return create_singlethread_workqueue(name);
}
INNO_EXT_SYM(fh2m_inno_create_singlethread_wq);

inno_workqueue *fh2m_inno_alloc_workqueue(const char *name, unsigned int flags, int max_active)
{
	return alloc_workqueue(name, flags, max_active);
}
INNO_EXT_SYM(fh2m_inno_alloc_workqueue);

void fh2m_inno_destroy_workqueue(inno_workqueue *wq)
{
	destroy_workqueue((struct workqueue_struct *)wq);
}
INNO_EXT_SYM(fh2m_inno_destroy_workqueue);

static void inno_work_func(struct work_struct *data)
{
	struct inno_work *work = container_of(data, struct inno_work, work);
	work->func(work->data);
}

void *fh2m_inno_work_alloc(void (*func)(void *data), void *data)
{
	struct inno_work *work = (struct inno_work *)
		kzalloc(sizeof(struct inno_work), GFP_KERNEL);
	if (!work)
		return NULL;

	work->func = func;
	work->data = data;
	INIT_WORK(&work->work, inno_work_func);
	return work;
}
INNO_EXT_SYM(fh2m_inno_work_alloc);

void fh2m_inno_work_destroy(void *work)
{
	kfree(work);
}
INNO_EXT_SYM(fh2m_inno_work_destroy);

int fh2m_inno_queue_work(inno_workqueue *wq, void *wk)
{
	return queue_work((struct workqueue_struct *)wq, &((struct inno_work *)wk)->work);
}
INNO_EXT_SYM(fh2m_inno_queue_work);

int fh2m_inno_work_pending(void *wk)
{
	return work_pending(&((struct inno_work *)wk)->work);
}
INNO_EXT_SYM(fh2m_inno_work_pending);

int fh2m_inno_queue_work_on(int cpu, inno_workqueue *wq, void *wk)
{
	return queue_work_on(cpu, (struct workqueue_struct *)wq, &((struct inno_work *)wk)->work);
}
INNO_EXT_SYM(fh2m_inno_queue_work_on);

static struct delayed_work *inno_to_dwork(struct work_struct *work)
{
	return container_of(work, struct delayed_work, work);
}

static void inno_dwork_func(struct work_struct *work)
{
	struct delayed_work *dwork = inno_to_dwork(work);

	struct inno_dwork *dwk = container_of(dwork, struct inno_dwork, dwork);
	dwk->func(dwk->data);
}

void *fh2m_inno_dwork_alloc(void (*func)(void *data), void *data)
{
	struct inno_dwork *dwork = (struct inno_dwork *)
		kzalloc(sizeof(struct inno_dwork), GFP_KERNEL);
	if (!dwork)
		return NULL;

	dwork->func = func;
	dwork->data = data;
	INIT_DELAYED_WORK(&dwork->dwork, inno_dwork_func);
	return dwork;
}
INNO_EXT_SYM(fh2m_inno_dwork_alloc);

int fh2m_inno_queue_dwork(inno_workqueue *wq, inno_dwork *dwk, unsigned int msec)
{
	return queue_delayed_work((struct workqueue_struct *)wq,
		&((struct inno_dwork *)dwk)->dwork, msecs_to_jiffies(msec));
}
INNO_EXT_SYM(fh2m_inno_queue_dwork);

int fh2m_inno_schedule_dwork(inno_dwork *dwk, unsigned int msec)
{
	return schedule_delayed_work(&((struct inno_dwork *)dwk)->dwork, msecs_to_jiffies(msec));
}
INNO_EXT_SYM(fh2m_inno_schedule_dwork);

int fh2m_inno_mod_dwork(inno_dwork *dwk, unsigned int msec)
{
	return mod_delayed_work(system_wq, &((struct inno_dwork *)dwk)->dwork, msecs_to_jiffies(msec));
}
INNO_EXT_SYM(fh2m_inno_mod_dwork);

int fh2m_inno_cancel_dwork(inno_dwork *dwk)
{
	return cancel_delayed_work(&((struct inno_dwork *)dwk)->dwork);
}
INNO_EXT_SYM(fh2m_inno_cancel_dwork);

int fh2m_inno_cancel_dwork_sync(inno_dwork *dwk)
{
	return cancel_delayed_work_sync(&((struct inno_dwork *)dwk)->dwork);
}
INNO_EXT_SYM(fh2m_inno_cancel_dwork_sync);

void fh2m_inno_dwork_destroy(void *dwork)
{
	kfree(dwork);
}
INNO_EXT_SYM(fh2m_inno_dwork_destroy);

bool fh2m_inno_schedule_work(void *wk)
{
	return schedule_work(&((struct inno_work *)wk)->work);
}
INNO_EXT_SYM(fh2m_inno_schedule_work);

void fh2m_inno_flush_scheduled_work(void)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0))
	flush_scheduled_work();
#else
#if defined(INNOGPU_ATRRIBUTE_WARNING_PRESENT)
	// flush_workqueue(system_wq);
#endif
#endif
}
INNO_EXT_SYM(fh2m_inno_flush_scheduled_work);

bool fh2m_inno_cancel_work_sync(void *wk)
{
	return cancel_work_sync(&((struct inno_work *)wk)->work);
}
INNO_EXT_SYM(fh2m_inno_cancel_work_sync);

void fh2m_inno_flush_workqueue(inno_workqueue *wq)
{
	flush_workqueue((struct workqueue_struct *)wq);
}
INNO_EXT_SYM(fh2m_inno_flush_workqueue);

bool fh2m_inno_flush_work(inno_work *work)
{
	return flush_work(&((struct inno_work *)work)->work);
}
INNO_EXT_SYM(fh2m_inno_flush_work);

signed long  fh2m_inno_schedule_timeout(signed long timeout)
{
	return schedule_timeout(timeout);
}
INNO_EXT_SYM(fh2m_inno_schedule_timeout);

void fh2m_inno_might_sleep(void)
{
	might_sleep();
}
INNO_EXT_SYM(fh2m_inno_might_sleep);

unsigned int fh2m_inno_work_busy(inno_work *work)
{
	return work_busy(&((struct inno_work *)work)->work);
}
INNO_EXT_SYM(fh2m_inno_work_busy);

int fh2m_inno_get_task_interruptible(void)
{
    return TASK_INTERRUPTIBLE;
}
INNO_EXT_SYM(fh2m_inno_get_task_interruptible);

int fh2m_inno_get_task_uninterruptible(void) {
    return TASK_UNINTERRUPTIBLE;
}
INNO_EXT_SYM(fh2m_inno_get_task_uninterruptible);

int fh2m_inno_get_task_killable(void)
{
    return TASK_KILLABLE;
}
INNO_EXT_SYM(fh2m_inno_get_task_killable);

int fh2m_inno_get_flag_exclusive(void)
{
    return WQ_FLAG_EXCLUSIVE;
}
INNO_EXT_SYM(fh2m_inno_get_flag_exclusive);
