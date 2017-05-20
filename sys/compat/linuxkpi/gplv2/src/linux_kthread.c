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

#include <linux/compiler.h>
#include <linux/atomic.h>
#include <linux/wait.h>

enum {
	KTHREAD_SHOULD_STOP_MASK = (1 << 0),
	KTHREAD_SHOULD_PARK_MASK = (1 << 1),
	KTHREAD_IS_PARKED_MASK = (1 << 2),
};

bool
kthread_should_park(void)
{

	return (atomic_read(&current->kthread_flags) & KTHREAD_SHOULD_PARK_MASK);
}

int
kthread_park(struct task_struct *task)
{

	if (task == NULL)
		return (-ENOSYS);

	if (atomic_read(&task->kthread_flags) & KTHREAD_IS_PARKED_MASK)
		goto done;

	atomic_or(KTHREAD_SHOULD_PARK_MASK, &task->kthread_flags);

	if (task == current)
		goto done;

	wake_up_process(task);
	wait_for_completion(&task->parked);
done:
	return (0);
}

void
kthread_unpark(struct task_struct *task)
{

	if (task == NULL)
		return;

	atomic_andnot(KTHREAD_SHOULD_PARK_MASK, &task->kthread_flags);

	if (atomic_fetch_andnot(KTHREAD_IS_PARKED_MASK,
	    &task->kthread_flags) & KTHREAD_IS_PARKED_MASK) {
		wake_up_state(task, TASK_PARKED);
	}
}

void
kthread_parkme(void)
{
	struct task_struct *task = current;

	/* don't park threads without a task struct */
	if (task == NULL)
		return;

	set_task_state(task, TASK_PARKED);

	while (atomic_read(&task->kthread_flags) & KTHREAD_SHOULD_PARK_MASK) {
		if (!(atomic_fetch_or(KTHREAD_IS_PARKED_MASK,
		    &task->kthread_flags) & KTHREAD_IS_PARKED_MASK)) {
			complete(&task->parked);
		}
		schedule();
		set_task_state(task, TASK_PARKED);
	}

	atomic_andnot(KTHREAD_IS_PARKED_MASK, &task->kthread_flags);

	set_task_state(task, TASK_RUNNING);
}
