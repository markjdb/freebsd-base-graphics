/*-
 * Copyright (c) 2017 Hans Petter Selasky
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

#include <linux/kernel.h>
#include <linux/wait.h>

int
linux_try_to_wake_up(struct task_struct *task, unsigned state)
{
	int retval;

	/*
	 * To avoid loosing any wakeups this function must be made
	 * atomic with regard to wakeup by locking the sleep_lock:
	 */
	mtx_lock(&task->sleep_lock);

	/* first check if the there are any sleepers */
	if (atomic_read(&task->state) & state) {
		if (atomic_xchg(&task->state, TASK_WAKING) != TASK_WAKING) {
			wakeup_one(task);
			retval = 1;
		} else {
			retval = 0;
		}
	} else {
		retval = 0;
	}
	mtx_unlock(&task->sleep_lock);

	return (retval);
}

int
default_wake_function(wait_queue_t *wq, unsigned mode,
    int wake_flags, void *key)
{
	return (linux_try_to_wake_up(wq->private, mode));
}

int
autoremove_wake_function(wait_queue_t *wq, unsigned mode,
    int sync, void *key)
{
	int ret = linux_try_to_wake_up(wq->private, mode);

	if (ret)
		list_del_init(&wq->task_list);
	return (ret);
}

int
wake_bit_function(wait_queue_t *wq, unsigned mode,
    int sync, void *key_arg)
{
	struct wait_bit_key *key = key_arg;
	struct wait_bit_queue *wait_bit = container_of(wq,
	    struct wait_bit_queue, wait);

	if (wait_bit->key.flags == key->flags &&
	    wait_bit->key.bit_nr == key->bit_nr &&
	    test_bit(key->bit_nr, key->flags) == 0)
		return (autoremove_wake_function(wq, mode, sync, key));

	return (0);
}

void
linux_wake_up(wait_queue_head_t *wqh, unsigned mode,
    int nr, void *key)
{
#if 0
	struct list_head *ptmp;
	struct list_head *p;
#endif

	spin_lock(&wqh->lock);
#if 0
	selwakeup(&wqh->wqh_si);
	if (__predict_false(!list_empty(&wqh->wqh_file_list))) {
		list_for_each_safe(p, ptmp, &wqh->wqh_file_list) {
			struct linux_file *f;

			f = list_entry(p, struct linux_file, f_entry);
			tasklet_schedule(&f->f_kevent_tasklet);
		}
	}
#endif
	linux_wake_up_locked(wqh, mode, nr, key);
	spin_unlock(&wqh->lock);
}

void
linux_wake_up_locked(wait_queue_head_t *wqh, unsigned mode,
    int nr, void *key)
{
	wait_queue_t *curr;
	wait_queue_t *next;

	list_for_each_entry_safe(curr, next, &wqh->task_list, task_list) {
		unsigned flags = curr->flags;

		if (curr->func(curr, TASK_NORMAL, 0, key) &&
		    (flags & WQ_FLAG_EXCLUSIVE) && !--nr)
			break;
	}
}

void
linux_abort_exclusive_wait(wait_queue_head_t *wqh, wait_queue_t *wait,
    unsigned mode, void *key)
{

	__set_current_state(TASK_RUNNING);
	spin_lock(&wqh->lock);
	if (!list_empty(&wait->task_list))
		list_del_init(&wait->task_list);
	else if (waitqueue_active(wqh))
		linux_wake_up_locked_key(wqh, mode, key);
	spin_unlock(&wqh->lock);
}

DECLARE_WAIT_QUEUE_HEAD(linux_bit_wait_queue_head);

static inline int
linux_wait_on_bit_timeout_sub(struct wait_bit_key *key, unsigned mode)
{
	int timeout = key->timeout - jiffies;

	linux_set_current(curthread);

	if (time_after_eq(0, timeout))
		return (-EAGAIN);

	schedule_timeout(timeout);

	if (signal_pending_state(mode, current))
		return (-EINTR);

	return (0);
}

int
linux_wait_on_bit_timeout(wait_queue_head_t *wq, struct wait_bit_queue *wqh,
    unsigned mode)
{
	int ret = 0;

	while (1) {
		prepare_to_wait(wq, &wqh->wait, mode);
		if (test_bit(wqh->key.bit_nr, wqh->key.flags) == 0)
			break;
		ret = linux_wait_on_bit_timeout_sub(&wqh->key, mode);
		if (ret != 0)
			break;
		if (test_bit(wqh->key.bit_nr, wqh->key.flags) == 0)
			break;
	}

	finish_wait(wq, &wqh->wait);

	return (ret);
}
