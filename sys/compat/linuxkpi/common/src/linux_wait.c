/*-
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/file.h>
#include <sys/malloc.h>
#include <sys/sleepqueue.h>

#include <linux/file.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/jiffies.h>
#include <linux/poll.h>
#include <linux/spinlock.h>
#include <linux/wait.h>

void
linux_poll_wait(struct linux_file *filp, wait_queue_head_t *q,
    poll_table *p __unused)
{

	spin_lock(&q->lock);
	if (q->filp != NULL && q->filp != filp)
		panic("polling for multiple files on queue %p", q);
	if (q->filp == NULL)
		fhold(filp->_file);
	q->filp = filp;
	selrecord(curthread, &filp->f_selinfo);
	spin_unlock(&q->lock);
}

int
linux_wait_event_common(void *wchan, int timeout, int queue, spinlock_t *lock)
{
	int error;

	/* XXX after locking sleepq? */
	if (lock != NULL)
		spin_unlock_irq(lock);

	sleepq_lock(wchan);
	if (timeout != 0)
		sleepq_set_timeout_sbt(wchan, tick_sbt * timeout, 0,
		    C_HARDCLOCK | C_ABSOLUTE);
	if (queue == __WQ_INTR) {
		if (timeout != 0)
			error = sleepq_timedwait_sig(wchan, queue);
		else
			error = sleepq_wait_sig(wchan, queue);
	} else {
		if (timeout != 0)
			error = sleepq_timedwait(wchan, queue);
		else {
			error = 0;
			sleepq_wait(wchan, queue);
		}
	}

	if (lock != NULL)
		spin_lock_irq(lock);

	return (error);
}

void
linux_wake_up(void *wchan, wait_queue_head_t *q, bool all, bool intronly)
{
	wait_queue_t *wq;
	struct list_head *pos, *tmp;
	struct linux_file *filp;
	struct thread *td;
	int wakeup_swapper;

	td = curthread;

	wakeup_swapper = 0;
	sleepq_lock(wchan);
	if (all) {
		if (!intronly)
			wakeup_swapper |= sleepq_broadcast(wchan,
			    SLEEPQ_SLEEP, 0, __WQ_NOINTR);
		wakeup_swapper |= sleepq_broadcast(wchan, SLEEPQ_SLEEP,
		    0, __WQ_INTR);
	} else {
		if (!intronly)
			wakeup_swapper |= sleepq_signal(wchan, SLEEPQ_SLEEP,
			    0, __WQ_NOINTR);
		wakeup_swapper |= sleepq_signal(wchan, SLEEPQ_SLEEP,
		    0, __WQ_INTR);
	}
	sleepq_release(wchan);
	if (wakeup_swapper)
		kick_proc0();

	list_for_each_safe(pos, tmp, &q->task_list) {
		wq = __containerof(pos, wait_queue_t, task_list);
		if (wq->func != NULL)
			(void)wq->func(wq, TASK_INTERRUPTIBLE |
			    (intronly ? 0 : TASK_UNINTERRUPTIBLE), wq->flags,
			    wchan);
		else
			list_del_init(pos);
	}

	if (q == NULL)
		return;
	filp = q->filp;
	q->filp = NULL;
	if (filp != NULL) {
		selwakeup(&filp->f_selinfo);
		KNOTE(&filp->f_selinfo.si_note, 1, 0);
		fdrop(filp->_file, td);
	}
}
