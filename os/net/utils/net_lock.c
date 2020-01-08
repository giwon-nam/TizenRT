/****************************************************************************
 *
 * Copyright 2016 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * net/utils/net_lock.c
 *
 *   Copyright (C) 2011-2012, 2014-2018 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <tinyara/config.h>

#include <unistd.h>
#include <semaphore.h>
#include <assert.h>
#include <errno.h>
#include <debug.h>

#include <tinyara/irq.h>
#include <tinyara/arch.h>
#include <tinyara/semaphore.h>
#include <tinyara/net/net.h>

#include "utils/utils.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define NO_HOLDER ((pid_t)-1)

/****************************************************************************
 * Private Data
 ****************************************************************************/

static sem_t g_netlock;
static pid_t g_holder = NO_HOLDER;
static unsigned int g_count = 0;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static inline int sem_wait_uninterruptible(FAR sem_t *sem)
{
	int ret;

	do {
		/* Take the semaphore (perhaps waiting) */

		ret = sem_wait(sem);
		if (ret < 0) {
			ret = -get_errno();
		}
	} while (ret == -EINTR || ret == -ECANCELED);

	return ret;
}

static inline int sem_timedwait_uninterruptible(FAR sem_t *sem, FAR const struct timespec *abstime)
{
	int ret;

	do {
		/* Take the semaphore (perhaps waiting) */

		ret = sem_timedwait(sem, abstime);
		if (ret < 0) {
			ret = -get_errno();
		}
	} while (ret == -EINTR || ret == -ECANCELED);

	return ret;
}

/****************************************************************************
 * Name: _net_takesem
 *
 * Description:
 *   Take the semaphore, waiting indefinitely.
 *   REVISIT: Should this return if -EINTR?
 *
 ****************************************************************************/

static int _net_takesem(void)
{
	return sem_wait_uninterruptible(&g_netlock);
}

/****************************************************************************
 * Name: _net_timedwait
 ****************************************************************************/

static int
_net_timedwait(sem_t *sem, bool interruptable, unsigned int timeout)
{
	unsigned int count;
	irqstate_t flags;
	int blresult;
	int ret;

	flags = irqsave(); /* No interrupts */
	sched_lock();	  /* No context switches */

	/* Release the network lock, remembering my count.  net_breaklock will
	 * return a negated value if the caller does not hold the network lock.
	 */

	blresult = net_breaklock(&count);

	/* Now take the semaphore, waiting if so requested. */

	if (timeout != UINT_MAX) {
		struct timespec abstime;

		DEBUGVERIFY(clock_gettime(CLOCK_REALTIME, &abstime));

		abstime.tv_sec += timeout / MSEC_PER_SEC;
		abstime.tv_nsec += timeout % MSEC_PER_SEC * NSEC_PER_MSEC;
		if (abstime.tv_nsec >= NSEC_PER_SEC) {
			abstime.tv_sec++;
			abstime.tv_nsec -= NSEC_PER_SEC;
		}

		/* Wait until we get the lock or until the timeout expires */

		if (interruptable) {
			ret = sem_timedwait(sem, &abstime);
		} else {
			ret = sem_timedwait_uninterruptible(sem, &abstime);
		}
	} else {
		/* Wait as long as necessary to get the lock */

		if (interruptable) {
			ret = sem_wait(sem);
		} else {
			ret = sem_wait_uninterruptible(sem);
		}
	}

	/* Recover the network lock at the proper count (if we held it before) */

	if (blresult >= 0) {
		net_restorelock(count);
	}

	sched_unlock();
	irqrestore(flags);
	return ret;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: net_lockinitialize
 *
 * Description:
 *   Initialize the locking facility
 *
 ****************************************************************************/

void net_lockinitialize(void)
{
	sem_init(&g_netlock, 0, 1);
}

/****************************************************************************
 * Name: net_lock
 *
 * Description:
 *   Take the network lock
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   Zero (OK) is returned on success; a negated errno value is returned on
 *   failured (probably -ECANCELED).
 *
 ****************************************************************************/

int net_lock(void)
{
	pid_t me = getpid();
	int ret = OK;

	/* Does this thread already hold the semaphore? */

	if (g_holder == me) {
		/* Yes.. just increment the reference count */

		g_count++;
	} else {
		/* No.. take the semaphore (perhaps waiting) */

		ret = _net_takesem();
		if (ret >= 0) {
			/* Now this thread holds the semaphore */

			g_holder = me;
			g_count = 1;
		}
	}

	return ret;
}

/****************************************************************************
 * Name: net_unlock
 *
 * Description:
 *   Release the network lock.
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

void net_unlock(void)
{
	DEBUGASSERT(g_holder == getpid() && g_count > 0);

	/* If the count would go to zero, then release the semaphore */

	if (g_count == 1) {
		/* We no longer hold the semaphore */

		g_holder = NO_HOLDER;
		g_count = 0;
		sem_post(&g_netlock);
	} else {
		/* We still hold the semaphore. Just decrement the count */

		g_count--;
	}
}

/****************************************************************************
 * Name: net_breaklock
 *
 * Description:
 *   Break the lock, return information needed to restore re-entrant lock
 *   state.
 *
 ****************************************************************************/

int net_breaklock(FAR unsigned int *count)
{
	irqstate_t flags;
	pid_t me = getpid();
	int ret = -EPERM;

	DEBUGASSERT(count != NULL);

	flags = irqsave(); /* No interrupts */
	if (g_holder == me) {
		/* Return the lock setting */

		*count = g_count;

		/* Release the network lock  */

		g_holder = NO_HOLDER;
		g_count = 0;

		sem_post(&g_netlock);
		ret = OK;
	}

	irqrestore(flags);
	return ret;
}

/****************************************************************************
 * Name: net_restorelock
 *
 * Description:
 *   Restore the locked state
 *
 * Returned Value:
 *   Zero (OK) is returned on success; a negated errno value is returned on
 *   failured (probably -ECANCELED).
 *
 ****************************************************************************/

int net_restorelock(unsigned int count)
{
	pid_t me = getpid();
	int ret;

	DEBUGASSERT(g_holder != me);

	/* Recover the network lock at the proper count */

	ret = _net_takesem();
	if (ret >= 0) {
		g_holder = me;
		g_count = count;
	}

	return ret;
}

/****************************************************************************
 * Name: net_timedwait
 *
 * Description:
 *   Atomically wait for sem (or a timeout( while temporarily releasing
 *   the lock on the network.
 *
 *   Caution should be utilized.  Because the network lock is relinquished
 *   during the wait, there could changes in the network state that occur
 *   before the lock is recovered.  Your design should account for this
 *   possibility.
 *
 * Input Parameters:
 *   sem     - A reference to the semaphore to be taken.
 *   timeout - The relative time to wait until a timeout is declared.
 *
 * Returned Value:
 *   Zero (OK) is returned on success; a negated errno value is returned on
 *   any failure.
 *
 ****************************************************************************/

int net_timedwait(sem_t *sem, unsigned int timeout)
{
	return _net_timedwait(sem, true, timeout);
}

/****************************************************************************
 * Name: net_lockedwait
 *
 * Description:
 *   Atomically wait for sem while temporarily releasing the network lock.
 *
 *   Caution should be utilized.  Because the network lock is relinquished
 *   during the wait, there could changes in the network state that occur
 *   before the lock is recovered.  Your design should account for this
 *   possibility.
 *
 * Input Parameters:
 *   sem - A reference to the semaphore to be taken.
 *
 * Returned Value:
 *   Zero (OK) is returned on success; a negated errno value is returned on
 *   any failure.
 *
 ****************************************************************************/

int net_lockedwait(sem_t *sem)
{
	return net_timedwait(sem, UINT_MAX);
}

/****************************************************************************
 * Name: net_timedwait_uninterruptible
 *
 * Description:
 *   This function is wrapped version of net_timedwait(), which is
 *   uninterruptible and convenient for use.
 *
 * Input Parameters:
 *   sem     - A reference to the semaphore to be taken.
 *   timeout - The relative time to wait until a timeout is declared.
 *
 * Returned Value:
 *   Zero (OK) is returned on success; a negated errno value is returned on
 *   any failure.
 *
 ****************************************************************************/

int net_timedwait_uninterruptible(sem_t *sem, unsigned int timeout)
{
	return _net_timedwait(sem, false, timeout);
}

/****************************************************************************
 * Name: net_lockedwait_uninterruptible
 *
 * Description:
 *   This function is wrapped version of net_lockedwait(), which is
 *   uninterruptible and convenient for use.
 *
 * Input Parameters:
 *   sem - A reference to the semaphore to be taken.
 *
 * Returned Value:
 *   Zero (OK) is returned on success; a negated errno value is returned on
 *   any failure.
 *
 ****************************************************************************/

int net_lockedwait_uninterruptible(sem_t *sem)
{
	return net_timedwait_uninterruptible(sem, UINT_MAX);
}
