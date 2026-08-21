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
#ifndef __INNO_WAITQUEUE_H__
#define __INNO_WAITQUEUE_H__

#include "inno_task.h"

typedef void inno_waitqueue_head;
typedef void inno_waitqueue_entry;

void fh2m_inno_init_waitqueue_head(inno_waitqueue_head *wq);

void fh2m_inno_wake_up_interruptible(inno_waitqueue_head *wq);

void fh2m_inno_wake_up_all(inno_waitqueue_head *wq);

void fh2m_inno_init_wait_entry(inno_waitqueue_entry *wq_entry, int flags);

long fh2m_inno_prepare_to_wait_event(inno_waitqueue_head *wq_head,
                                inno_waitqueue_entry *wq_entry, int state);

void fh2m_inno_finish_wait(inno_waitqueue_head *wq_head, inno_waitqueue_entry *wq_entry);

void *fh2m_inno_waitqueue_entry_alloc(void);

void fh2m_inno_waitqueue_entry_free(inno_waitqueue_entry *wq_entry);

void fh2m_inno_waitqueue_head_free(inno_waitqueue_head *head);

void *fh2m_inno_waitqueue_head_alloc(void);

void fh2m_inno_wake_up(inno_waitqueue_head *wq_head);


#define ___inno_wait_is_interruptible(state)						\
	(!__builtin_constant_p(state) ||					\
		state == fh2m_inno_get_task_interruptible() || state == fh2m_inno_get_task_killable())		\


#define ___inno_wait_cond_timeout(condition)						\
({										\
	bool __cond = (condition);						\
	if (__cond && !__ret)							\
		__ret = 1;							\
	__cond || !__ret;							\
})


#define __inno_wait_event_interruptible_timeout(wq_head, condition, timeout)		\
	___inno_wait_event(wq_head, ___inno_wait_cond_timeout(condition),			\
		      fh2m_inno_get_task_interruptible(), 0, timeout,				\
		      __ret = fh2m_inno_schedule_timeout(__ret))

#define ___inno_wait_event(wq_head, condition, state, exclusive, ret, cmd)		\
	({										\
		__label__ __out;							\
		inno_waitqueue_entry *__wq_entry = fh2m_inno_waitqueue_entry_alloc(); 				\
		long __ret = ret;	/* explicit shadow */				\
											\
		fh2m_inno_init_wait_entry(__wq_entry, exclusive ? fh2m_inno_get_flag_exclusive() : 0);	\
		for (;;) {								\
			long __int = fh2m_inno_prepare_to_wait_event(wq_head, __wq_entry, state);\
											\
			if (condition)							\
				break;							\
											\
			if (___inno_wait_is_interruptible(state) && __int) { 		\
				__ret = __int;						\
				goto __out; 					\
			}								\
											\
			cmd;								\
		}									\
		fh2m_inno_finish_wait(wq_head, __wq_entry); 				\
	__out:											\
		fh2m_inno_waitqueue_entry_free(__wq_entry);		\
		__ret;										\
	})

#define inno_wait_event_interruptible_timeout(wq_head, condition, timeout)		\
({										\
	long __ret = timeout;							\
	fh2m_inno_might_sleep();								\
	if (!___inno_wait_cond_timeout(condition))					\
		__ret = __inno_wait_event_interruptible_timeout(wq_head,		\
						condition, timeout);		\
	__ret;									\
})

#define __inno_wait_event_timeout(wq_head, condition, timeout)			\
	___inno_wait_event(wq_head, ___inno_wait_cond_timeout(condition),			\
		      fh2m_inno_get_task_uninterruptible(), 0, timeout,				\
		      __ret = fh2m_inno_schedule_timeout(__ret))

#define inno_wait_event_timeout(wq_head, condition, timeout)				\
({										\
	long __ret = timeout;							\
	fh2m_inno_might_sleep();								\
	if (!___inno_wait_cond_timeout(condition))					\
		__ret = __inno_wait_event_timeout(wq_head, condition, timeout);	\
	__ret;									\
})

#define __inno_wait_event_interruptible(wq_head, condition)				\
	___inno_wait_event(wq_head, condition, fh2m_inno_get_task_interruptible(), 0, 0,		\
		      fh2m_inno_task_schedule())

#define inno_wait_event_interruptible(wq_head, condition)				\
({										\
	int __ret = 0;								\
	fh2m_inno_might_sleep();								\
	if (!(condition))							\
		__ret = __inno_wait_event_interruptible(wq_head, condition);		\
	__ret;									\
})

#endif


