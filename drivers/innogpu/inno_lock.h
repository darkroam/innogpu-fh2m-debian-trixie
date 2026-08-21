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

#ifndef __INNO_LOCK_H__
#define __INNO_LOCK_H__

#include <linux/types.h>
#include "inno_plat_dev.h"

typedef void inno_mutex;
typedef void inno_completion;
typedef void inno_spinlock;
typedef void inno_rw_semaphore;
typedef void inno_semaphore;
typedef void inno_rwlock;

extern unsigned long fh2m_inno_rwsem_size;
extern unsigned long fh2m_inno_mutex_size;
extern unsigned long fh2m_inno_spinlock_size;

void fh2m_inno_preempt_disable(void);

void fh2m_inno_preempt_enable(void);

void fh2m_inno_up(inno_semaphore *sem);

void fh2m_inno_down(inno_semaphore *sem);

void fh2m_inno_sema_init(inno_semaphore *sem, int val);

int fh2m_inno_down_interruptible(inno_semaphore *sem);

void fh2m_inno_init_rwsem(inno_rw_semaphore *rwsem);

void fh2m_inno_down_read(inno_rw_semaphore *rwsem);

void fh2m_inno_down_read_nested(inno_rw_semaphore *rwsem, unsigned int subclass);

void fh2m_inno_up_read(inno_rw_semaphore *rwsem);

void fh2m_inno_down_write(inno_rw_semaphore *rwsem);

void fh2m_inno_up_write(inno_rw_semaphore *rwsem);

void fh2m_inno_mutex_init(inno_mutex *mutex);

void fh2m_inno_mutex_destroy(inno_mutex *mutex);

inno_mutex *fh2m_inno_mutex_alloc(void);

void fh2m_inno_mutex_free(inno_mutex *mutex);

void fh2m_inno_mutex_lock(inno_mutex *mutex);

int fh2m_inno_mutex_lock_interruptible(inno_mutex *mutex);

void fh2m_inno_mutex_lock_nested(inno_mutex *mutex, unsigned int subclass);

void fh2m_inno_mutex_unlock(inno_mutex *mutex);

int fh2m_inno_mutex_trylock(inno_mutex *mutex);

int fh2m_inno_mutex_is_locked(inno_mutex *mutex);

inno_completion *fh2m_inno_completion_alloc(void);

void fh2m_inno_completion_free(inno_completion *);

void fh2m_inno_wait_for_completion(inno_completion *);

unsigned long fh2m_inno_wait_for_completion_timeout(inno_completion *c, unsigned long timeout);

void fh2m_inno_complete(inno_completion *);

void fh2m_inno_init_completion(inno_completion *);

void fh2m_inno_reinit_completion(inno_completion *);

void fh2m_inno_complete_and_exit(inno_completion *c, long code);

void fh2m_inno_local_irq_save(unsigned long *flags);

void fh2m_inno_local_irq_restore(unsigned long flags);

void fh2m_inno_spinlock_init(inno_spinlock *lock);

inno_spinlock *fh2m_inno_spinlock_alloc(void);

void fh2m_inno_spinlock_free(inno_spinlock *);

inno_spinlock *inno_spinlock_devm_alloc(inno_dev *dev);

void inno_spinlock_devm_free(inno_dev *dev, inno_spinlock *lock);

void fh2m_inno_spin_lock_irqsave(inno_spinlock *, unsigned long *flags);

void fh2m_inno_spin_unlock_irqrestore(inno_spinlock *, unsigned long flags);

void fh2m_inno_spin_lock(inno_spinlock *lock);

void fh2m_inno_spin_unlock(inno_spinlock *lock);

void fh2m_inno_spin_lock_irq(inno_spinlock *lock);

void fh2m_inno_spin_unlock_irq(inno_spinlock *lock);

int32_t fh2m_inno_atomic_read(const atomic_t *counter);

void fh2m_inno_atomic_write(atomic_t *counter, int32_t v);

void fh2m_inno_atomic_inc(atomic_t *counter);

void fh2m_inno_atomic_dec(atomic_t *counter);

int32_t fh2m_inno_atomic_inc_return(atomic_t *counter);

int32_t fh2m_inno_atomic_dec_return(atomic_t *counter);

int32_t fh2m_inno_atomic_fetch_inc(atomic_t *counter);

int32_t fh2m_inno_atomic_cmpxchg(atomic_t *counter, int old, int new);

int32_t fh2m_inno_atomic_xchg(atomic_t *counter, int v);

int32_t fh2m_inno_atomic_add_return(int32_t v, atomic_t *counter);

bool fh2m_inno_atomic_add_unless(atomic_t *counter, int a, int u);

int32_t fh2m_inno_atomic_sub_return(atomic_t *counter, int32_t v);

int fh2m_inno_atomic_fetch_andnot_relaxed(int i, atomic_t *v);

int64_t fh2m_inno_cmpxchg64_relaxed(int64_t *ptr, int64_t old, int64_t new);

inno_rwlock* fh2m_inno_rwlock_alloc(void);

void fh2m_inno_rwlock_free(inno_rwlock *rw_lock);

void fh2m_inno_read_lock(inno_rwlock *rw_lock);

void fh2m_inno_read_unlock(inno_rwlock *rw_lock);

void fh2m_inno_write_lock(inno_rwlock *rw_lock);

void fh2m_inno_write_unlock(inno_rwlock *rw_lock);
#endif
