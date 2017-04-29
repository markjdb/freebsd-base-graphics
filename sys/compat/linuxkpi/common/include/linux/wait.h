/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013, 2014 Mellanox Technologies, Ltd.
 * Copyright (c) 2017 Mark Johnston <markj@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _LINUX_WAIT_H_
#define	_LINUX_WAIT_H_

#include <linux/compiler.h>
#include <linux/list.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/spinlock.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/sleepqueue.h>

typedef struct wait_queue {
	struct list_head task_list;
	u_int flags;
	void *private;
	int (*func)(struct wait_queue *, u_int, int, void *);
} wait_queue_t;

typedef struct wait_queue_head {
	unsigned int wchan;
	spinlock_t lock;
	struct linux_file *filp;
	struct list_head task_list;
} wait_queue_head_t;

#define	init_waitqueue_head(wq) do {					\
	INIT_LIST_HEAD(&(wq)->task_list);				\
	spin_lock_init(&(wq)->lock);					\
} while (0)

#define	DECLARE_WAIT_QUEUE_HEAD(q)					\
	wait_queue_head_t q = { 					\
	    .task_list = LINUX_LIST_HEAD_INIT((q).task_list),		\
	};								\
	MTX_SYSINIT(lock, &(q).lock.m, spin_lock_name("lnxwq"), MTX_DEF)

#define	DECLARE_WAITQUEUE(name, thread)					\
	wait_queue_t name

#define	might_sleep()

extern void	linux_wake_up(void *wchan, wait_queue_head_t *q,
		    bool all, bool intronly);

#define	__wake_up_queue_locked(q, all, intronly)			\
	linux_wake_up(&(q)->wchan, q, all, intronly)

#define	__wake_up_queue(q, all, intronly) do {				\
	spin_lock(&(q)->lock);						\
	__wake_up_queue_locked(q, all, intronly);			\
	spin_unlock(&(q)->lock);					\
} while (0)

#define	wake_up(q)							\
	__wake_up_queue(q, false, false)
#define	wake_up_locked(q)						\
	__wake_up_queue_locked(q, false, false)
#define	wake_up_nr(q, nr)						\
	__wake_up_queue(q, true, false)
#define	wake_up_all(q)							\
	__wake_up_queue(q, true, false)
#define	wake_up_all_locked(q)						\
	__wake_up_queue_locked(q, true, false)
#define	wake_up_interruptible(q)					\
	__wake_up_queue(q, false, true)
#define	wake_up_interruptible_nr(q, nr)					\
	__wake_up_queue(q, true, true)
#define	wake_up_interruptible_all(q)					\
	__wake_up_queue(q, true, true)

#define	wake_up_atomic_t(a)						\
	linux_wake_up(a, NULL, true, false)
#define	wake_up_bit(p, v)						\
	linux_wake_up((void *)(((uintptr_t)(p) << 6) | v), NULL, true, false)

extern int	linux_wait_event_common(void *wchan, int timeout,	\
		    int queue, spinlock_t *lock);

/* sleepqueue indices */
#define	__WQ_INTR	0
#define	__WQ_NOINTR	1

/* XXX need to set task state after waking up */
#define	__wait_event_common(c, cond, timeout, intr, lock) ({		\
	int __end, __error, __flags, __queue, __ret;			\
									\
	__end = ticks + (timeout);					\
	if (__end == 0 && (timeout) != 0)				\
		__end = 1;						\
	__error = __ret = 0;						\
	__flags = SLEEPQ_SLEEP | ((intr) ? SLEEPQ_INTERRUPTIBLE : 0);	\
	__queue = (intr) ? __WQ_INTR : __WQ_NOINTR;			\
	for (;;) {							\
		if (SCHEDULER_STOPPED())				\
			break;						\
		sleepq_lock(c);						\
		sleepq_add(c, NULL, "lnxev", __flags, __queue);		\
		sleepq_release(c);					\
		__ret = !!(cond);					\
		if (__ret) {						\
			sleepq_remove(curthread, c);			\
			break;						\
		}							\
		__error = linux_wait_event_common(c, __end, __queue,	\
		    lock);						\
		if (__error == EINTR || __error == ERESTART) {		\
			__ret = -ERESTARTSYS;				\
			break;						\
		}							\
		__ret = !!(cond);					\
		if (__error == EWOULDBLOCK)				\
			break;						\
		if (__ret)						\
			break;						\
	}								\
	if ((timeout) != 0 && __error == 0) {				\
		__ret = __end - ticks;					\
		if (__ret <= 0)						\
			__ret = 1;					\
	}								\
	__ret;								\
})

#define	wait_event(q, cond)						\
	(void)__wait_event_common(&(q).wchan, cond, 0, false, NULL)

#define	wait_event_timeout(q, cond, timeout)				\
	__wait_event_common(&(q).wchan, cond, timeout, false, NULL)

#define	wait_event_interruptible(q, cond)				\
	__wait_event_common(&(q).wchan, cond, 0, true, NULL)

#define	wait_event_interruptible_timeout(q, cond, timeout)		\
	__wait_event_common(&(q).wchan, cond, timeout, true, NULL)

/* We rely on spin_lock() and spin_lock_irq() being identical. */
#define	wait_event_interruptible_locked(q, cond) 			\
	__wait_event_common(&(q).wchan, cond, 0, true, &(q).lock)

#define	wait_event_interruptible_locked_irq(q, cond) 			\
	__wait_event_common(&(q).wchan, cond, 0, true, &(q).lock)

#define	wait_event_interruptible_lock_irq(q, cond, lock)		\
	__wait_event_common(&(q).wchan, cond, 0, true, &(lock))

/* XXX cb */
#define	wait_on_atomic_t(a, cb, state)					\
	__wait_event_common(a, atomic_read(a) == 0, 0,			\
	    ((state) & TASK_INTERRUPTIBLE) != 0, NULL)

#define	wait_on_bit_timeout(p, v, state, timeout)			\
	__wait_event_common((void *)(((uintptr_t)(p) << 6) | (v)),	\
	    ((*(p)) & (1 << (v))) == 0,	timeout,			\
	    ((state) & TASK_INTERRUPTIBLE) != 0, NULL)

int autoremove_wake_function(wait_queue_t *, u_int, int, void *);

static inline int
waitqueue_active(wait_queue_head_t *q)
{
	void *wchan;
	int ret;

	wchan = &q->wchan;
	sleepq_lock(wchan);
	ret = sleepq_sleepcnt(wchan, __WQ_INTR) +
	    sleepq_sleepcnt(wchan, __WQ_NOINTR);
	sleepq_release(wchan);
	return (ret || !list_empty(&q->task_list));
}

#define DEFINE_WAIT(name)						\
	wait_queue_t name = {						\
	    .task_list = LINUX_LIST_HEAD_INIT((name).task_list),	\
	}

static inline void
__add_wait_queue(wait_queue_head_t *q, wait_queue_t *wait)
{

	list_add(&wait->task_list, &q->task_list);
}

static inline void
__add_wait_queue_tail(wait_queue_head_t *q, wait_queue_t *wait)
{

	list_add_tail(&wait->task_list, &q->task_list);
}

static inline void
add_wait_queue(wait_queue_head_t *q, wait_queue_t *wait)
{

	spin_lock_irq(&q->lock);
	__add_wait_queue(q, wait);
	spin_unlock_irq(&q->lock);
}

static inline void
__remove_wait_queue(wait_queue_head_t *q, wait_queue_t *wait)
{

	list_del(&wait->task_list);
}

static inline void
remove_wait_queue(wait_queue_head_t *q, wait_queue_t *wait)
{

	spin_lock_irq(&q->lock);
	__remove_wait_queue(q, wait);
	spin_unlock_irq(&q->lock);
}

static inline void
prepare_to_wait(wait_queue_head_t *q, wait_queue_t *wait, int state)
{

	set_current_state(state);
}

static inline void
finish_wait(wait_queue_head_t *q, wait_queue_t *wait)
{

	set_current_state(TASK_RUNNING);
}

#endif /* _LINUX_WAIT_H_ */
