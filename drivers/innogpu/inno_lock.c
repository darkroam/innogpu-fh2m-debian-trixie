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
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/completion.h>
#include <linux/spinlock.h>
#include <linux/atomic.h>
#include <linux/device.h>
#include <linux/semaphore.h>
#include <linux/preempt.h>
#include <linux/kthread.h>

#include "inno_misc.h"
#include "inno_lock.h"

unsigned long fh2m_inno_mutex_size = sizeof(struct mutex);
unsigned long fh2m_inno_rwsem_size = sizeof(struct rw_semaphore);
unsigned long fh2m_inno_spinlock_size = sizeof(spinlock_t);
INNO_EXT_SYM(fh2m_inno_rwsem_size);
INNO_EXT_SYM(fh2m_inno_spinlock_size);

INNO_EXT_SYM(fh2m_inno_mutex_size);

void fh2m_inno_preempt_disable(void)
{
	preempt_disable();
}
INNO_EXT_SYM(fh2m_inno_preempt_disable);

void fh2m_inno_preempt_enable(void)
{
	preempt_enable();
}
INNO_EXT_SYM(fh2m_inno_preempt_enable);

void fh2m_inno_up(inno_semaphore *sem)
{
	up((struct semaphore *)sem);
}
INNO_EXT_SYM(fh2m_inno_up);

void fh2m_inno_down(inno_semaphore *sem)
{
	down((struct semaphore *)sem);
}
INNO_EXT_SYM(fh2m_inno_down);

void fh2m_inno_sema_init(inno_semaphore *sem, int val)
{
	sema_init((struct semaphore *)sem, val);
}
INNO_EXT_SYM(fh2m_inno_sema_init);

int fh2m_inno_down_interruptible(inno_semaphore *sem)
{
	return down_interruptible((struct semaphore *)sem);
}
INNO_EXT_SYM(fh2m_inno_down_interruptible);

void fh2m_inno_init_rwsem(inno_rw_semaphore *rwsem)
{
	init_rwsem((struct rw_semaphore *)rwsem);
}
INNO_EXT_SYM(fh2m_inno_init_rwsem);

void fh2m_inno_down_read(inno_rw_semaphore *rwsem)
{
	down_read((struct rw_semaphore *)rwsem);
}
INNO_EXT_SYM(fh2m_inno_down_read);

void fh2m_inno_down_read_nested(inno_rw_semaphore *rwsem, unsigned int subclass)
{
	down_read_nested((struct rw_semaphore *)rwsem, subclass);
}
INNO_EXT_SYM(fh2m_inno_down_read_nested);

void fh2m_inno_up_read(inno_rw_semaphore *rwsem)
{
	up_read((struct rw_semaphore *)rwsem);
}
INNO_EXT_SYM(fh2m_inno_up_read);

void fh2m_inno_down_write(inno_rw_semaphore *rwsem)
{
	down_write((struct rw_semaphore *)rwsem);
}
INNO_EXT_SYM(fh2m_inno_down_write);

void fh2m_inno_up_write(inno_rw_semaphore *rwsem)
{
	up_write((struct rw_semaphore *)rwsem);
}
INNO_EXT_SYM(fh2m_inno_up_write);

void fh2m_inno_mutex_init(inno_mutex *mutex)
{
	mutex_init((struct mutex *)mutex);
}
INNO_EXT_SYM(fh2m_inno_mutex_init);

void fh2m_inno_mutex_destroy(inno_mutex *mutex)
{
	mutex_destroy((struct mutex *)mutex);
}
INNO_EXT_SYM(fh2m_inno_mutex_destroy);

inno_mutex *fh2m_inno_mutex_alloc(void)
{
	struct mutex *m = (struct mutex *)kmalloc(sizeof(struct mutex), GFP_KERNEL);
	if (!m)
		return NULL;
	mutex_init(m);
	return m;
}
INNO_EXT_SYM(fh2m_inno_mutex_alloc);

void fh2m_inno_mutex_free(inno_mutex *mutex)
{
	mutex_destroy((struct mutex*)mutex);

	kfree(mutex);
}
INNO_EXT_SYM(fh2m_inno_mutex_free);

void fh2m_inno_mutex_lock(inno_mutex *mutex)
{
	mutex_lock((struct mutex *)mutex);
}
INNO_EXT_SYM(fh2m_inno_mutex_lock);

int fh2m_inno_mutex_lock_interruptible(inno_mutex *mutex)
{
	return mutex_lock_interruptible((struct mutex *)mutex);
}
INNO_EXT_SYM(fh2m_inno_mutex_lock_interruptible);

void fh2m_inno_mutex_lock_nested(inno_mutex *mutex, unsigned int subclass)
{
	mutex_lock_nested((struct mutex *)mutex, subclass);
}
INNO_EXT_SYM(fh2m_inno_mutex_lock_nested);

int fh2m_inno_mutex_trylock(inno_mutex *mutex)
{
	return mutex_trylock((struct mutex *)mutex);
}
INNO_EXT_SYM(fh2m_inno_mutex_trylock);

void fh2m_inno_mutex_unlock(inno_mutex *mutex)
{
	mutex_unlock((struct mutex *)mutex);
}
INNO_EXT_SYM(fh2m_inno_mutex_unlock);

int fh2m_inno_mutex_is_locked(inno_mutex *mutex)
{
	return mutex_is_locked((struct mutex *)mutex);
}
INNO_EXT_SYM(fh2m_inno_mutex_is_locked);

inno_completion *fh2m_inno_completion_alloc(void)
{
	struct completion *c = (struct completion *)kmalloc(sizeof(struct completion), GFP_KERNEL);
	if (!c)
		return NULL;
	init_completion(c);
	return c;
}
INNO_EXT_SYM(fh2m_inno_completion_alloc);

void fh2m_inno_completion_free(inno_completion *c)
{
	kfree(c);
}
INNO_EXT_SYM(fh2m_inno_completion_free);

void fh2m_inno_wait_for_completion(inno_completion *c)
{
	wait_for_completion((struct completion *)c);
}
INNO_EXT_SYM(fh2m_inno_wait_for_completion);

unsigned long fh2m_inno_wait_for_completion_timeout(inno_completion *c, unsigned long timeout)
{
	return wait_for_completion_timeout((struct completion *)c, timeout);
}
INNO_EXT_SYM(fh2m_inno_wait_for_completion_timeout);

void fh2m_inno_complete(inno_completion *c)
{
	complete((struct completion *)c);
}
INNO_EXT_SYM(fh2m_inno_complete);

void fh2m_inno_init_completion(inno_completion *c)
{
	init_completion((struct completion *) c);
}
INNO_EXT_SYM(fh2m_inno_init_completion);

void fh2m_inno_reinit_completion(inno_completion *c)
{
	reinit_completion((struct completion *) c);
}
INNO_EXT_SYM(fh2m_inno_reinit_completion);

void fh2m_inno_complete_and_exit(inno_completion *c, long code)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 17, 0))
	complete_and_exit((struct completion *)c, code);
#else
	kthread_complete_and_exit((struct completion *)c, code);
#endif
}
INNO_EXT_SYM(fh2m_inno_complete_and_exit);

void fh2m_inno_local_irq_save(unsigned long *flags)
{
	local_irq_save((*flags));
}
INNO_EXT_SYM(fh2m_inno_local_irq_save);

void fh2m_inno_local_irq_restore(unsigned long flags)
{
	local_irq_restore(flags);
}
INNO_EXT_SYM(fh2m_inno_local_irq_restore);

void fh2m_inno_spinlock_init(inno_spinlock *lock)
{
	spin_lock_init((spinlock_t *)lock);
}
INNO_EXT_SYM(fh2m_inno_spinlock_init);

inno_spinlock *fh2m_inno_spinlock_alloc(void)
{
	spinlock_t *lock = (spinlock_t *)kmalloc(sizeof(spinlock_t), GFP_KERNEL);
	if (!lock)
		return NULL;
	spin_lock_init(lock);
	return lock;
}
INNO_EXT_SYM(fh2m_inno_spinlock_alloc);

inno_spinlock *inno_spinlock_devm_alloc(inno_dev *dev)
{
	spinlock_t *lock = devm_kmalloc((struct device *)dev, sizeof(spinlock_t),
			GFP_KERNEL);
	if (!lock)
		return NULL;
	spin_lock_init(lock);
	return lock;
}

void inno_spinlock_devm_free(inno_dev *dev, inno_spinlock *lock)
{
	devm_kfree((struct device *)dev, lock);
}

void fh2m_inno_spinlock_free(inno_spinlock *lock)
{
	kfree(lock);
}
INNO_EXT_SYM(fh2m_inno_spinlock_free);

void fh2m_inno_spin_lock_irqsave(inno_spinlock *lock, unsigned long *flags)
{
	spin_lock_irqsave((spinlock_t *)lock, (*flags));
}
INNO_EXT_SYM(fh2m_inno_spin_lock_irqsave);

void fh2m_inno_spin_unlock_irqrestore(inno_spinlock *lock, unsigned long flags)
{
	spin_unlock_irqrestore((spinlock_t *)lock, flags);
}
INNO_EXT_SYM(fh2m_inno_spin_unlock_irqrestore);

void fh2m_inno_spin_lock(inno_spinlock *lock)
{
	spin_lock((spinlock_t *)lock);
}
INNO_EXT_SYM(fh2m_inno_spin_lock);

void fh2m_inno_spin_unlock(inno_spinlock *lock)
{
	spin_unlock((spinlock_t *)lock);
}
INNO_EXT_SYM(fh2m_inno_spin_unlock);

void fh2m_inno_spin_lock_irq(inno_spinlock *lock)
{
	spin_lock_irq((spinlock_t *)lock);
}
INNO_EXT_SYM(fh2m_inno_spin_lock_irq);

void fh2m_inno_spin_unlock_irq(inno_spinlock *lock)
{
	spin_unlock_irq((spinlock_t *)lock);
}
INNO_EXT_SYM(fh2m_inno_spin_unlock_irq);

int32_t fh2m_inno_atomic_read(const atomic_t *counter)
{
	return atomic_read(counter);
}
INNO_EXT_SYM(fh2m_inno_atomic_read);

void fh2m_inno_atomic_write(atomic_t *counter, int32_t v)
{
	atomic_set(counter, v);
}
INNO_EXT_SYM(fh2m_inno_atomic_write);

void fh2m_inno_atomic_inc(atomic_t *counter)
{
	atomic_inc(counter);
}
INNO_EXT_SYM(fh2m_inno_atomic_inc);

void fh2m_inno_atomic_dec(atomic_t *counter)
{
	atomic_dec(counter);
}
INNO_EXT_SYM(fh2m_inno_atomic_dec);

int32_t fh2m_inno_atomic_inc_return(atomic_t *counter)
{
	return atomic_inc_return(counter);
}
INNO_EXT_SYM(fh2m_inno_atomic_inc_return);

int32_t fh2m_inno_atomic_dec_return(atomic_t *counter)
{
	return atomic_dec_return(counter);
}
INNO_EXT_SYM(fh2m_inno_atomic_dec_return);

int32_t fh2m_inno_atomic_fetch_inc(atomic_t *counter)
{
	return atomic_fetch_inc(counter);
}
INNO_EXT_SYM(fh2m_inno_atomic_fetch_inc);

int32_t fh2m_inno_atomic_cmpxchg(atomic_t *counter, int old, int new)
{
	return atomic_cmpxchg(counter, old, new);
}
INNO_EXT_SYM(fh2m_inno_atomic_cmpxchg);

int32_t fh2m_inno_atomic_xchg(atomic_t *counter, int v)
{
	return atomic_xchg(counter, v);
}
INNO_EXT_SYM(fh2m_inno_atomic_xchg);

int32_t fh2m_inno_atomic_add_return(int32_t v, atomic_t *counter)
{
	return atomic_add_return(v, counter);
}
INNO_EXT_SYM(fh2m_inno_atomic_add_return);

bool fh2m_inno_atomic_add_unless(atomic_t *counter, int a, int u)
{
	return atomic_add_unless(counter, a, u);
}
INNO_EXT_SYM(fh2m_inno_atomic_add_unless);

int32_t fh2m_inno_atomic_sub_return(atomic_t *counter, int32_t v)
{
	return atomic_sub_return(v, counter);
}
INNO_EXT_SYM(fh2m_inno_atomic_sub_return);

int fh2m_inno_atomic_fetch_andnot_relaxed(int i, atomic_t *v)
{
	return atomic_fetch_andnot_relaxed(i, v);
}
INNO_EXT_SYM(fh2m_inno_atomic_fetch_andnot_relaxed);

int64_t fh2m_inno_cmpxchg64_relaxed(int64_t *ptr, int64_t old, int64_t new)
{
	return cmpxchg_relaxed(ptr, old, new);
}
INNO_EXT_SYM(fh2m_inno_cmpxchg64_relaxed);

inno_rwlock* fh2m_inno_rwlock_alloc(void)
{
	rwlock_t *rw_lock = NULL;

	rw_lock = kmalloc(sizeof(rwlock_t), GFP_KERNEL);
	if (!rw_lock)
		return rw_lock;
	else
		rwlock_init(rw_lock);

	return (inno_rwlock *)rw_lock;
}
INNO_EXT_SYM(fh2m_inno_rwlock_alloc);

void fh2m_inno_rwlock_free(inno_rwlock *rw_lock)
{
	if (!rw_lock)
		return;
	
	kfree(rw_lock);
}
INNO_EXT_SYM(fh2m_inno_rwlock_free);

void fh2m_inno_read_lock(inno_rwlock *rw_lock)
{
	read_lock((rwlock_t *)rw_lock);
}
INNO_EXT_SYM(fh2m_inno_read_lock);

void fh2m_inno_read_unlock(inno_rwlock *rw_lock)
{
	read_unlock((rwlock_t *)rw_lock);
}
INNO_EXT_SYM(fh2m_inno_read_unlock);

void fh2m_inno_write_lock(inno_rwlock *rw_lock)
{
	write_lock((rwlock_t *)rw_lock);
}
INNO_EXT_SYM(fh2m_inno_write_lock);

void fh2m_inno_write_unlock(inno_rwlock *rw_lock)
{
	write_unlock((rwlock_t *)rw_lock);
}
INNO_EXT_SYM(fh2m_inno_write_unlock);
