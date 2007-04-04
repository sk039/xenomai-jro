/**
 * @file
 * This file is part of the Xenomai project.
 *
 * @note Copyright (C) 2004 Philippe Gerum <rpm@xenomai.org> 
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*!
 * \defgroup native Native Xenomai API.
 *
 * The native Xenomai programming interface available to real-time
 * applications. This API is built over the abstract RTOS core
 * implemented by the Xenomai nucleus.
 *
 */

#include <nucleus/pod.h>
#include <nucleus/registry.h>
#ifdef __KERNEL__
#include <linux/init.h>
#include <native/syscall.h>
#endif /* __KERNEL__ */
#include <native/task.h>
#include <native/timer.h>
#include <native/sem.h>
#include <native/event.h>
#include <native/mutex.h>
#include <native/cond.h>
#include <native/pipe.h>
#include <native/queue.h>
#include <native/heap.h>
#include <native/alarm.h>
#include <native/intr.h>
#include <native/ppd.h>

MODULE_DESCRIPTION("Native skin");
MODULE_AUTHOR("rpm@xenomai.org");
MODULE_LICENSE("GPL");

static u_long tick_arg = CONFIG_XENO_OPT_NATIVE_PERIOD;
module_param_named(tick_arg, tick_arg, ulong, 0444);
MODULE_PARM_DESC(tick_arg, "Fixed clock tick value (us), 0 for tick-less mode");

xntbase_t *__native_tbase;

#ifdef CONFIG_XENO_EXPORT_REGISTRY
xnptree_t __native_ptree = {

	.dir = NULL,
	.name = "native",
	.entries = 0,
};
#endif /* CONFIG_XENO_EXPORT_REGISTRY */

int SKIN_INIT(native)
{
	int err;

	initq(&__native_global_rholder.alarmq);
	initq(&__native_global_rholder.condq);
	initq(&__native_global_rholder.eventq);
	initq(&__native_global_rholder.heapq);
	initq(&__native_global_rholder.intrq);
	initq(&__native_global_rholder.mutexq);
	initq(&__native_global_rholder.pipeq);
	initq(&__native_global_rholder.queueq);
	initq(&__native_global_rholder.semq);

	err = xncore_attach(T_LOPRIO, T_HIPRIO);

	if (err)
		goto fail;

	err = xntbase_alloc("native", tick_arg * 1000, &__native_tbase);

	if (err)
		goto fail;

	xntbase_start(__native_tbase);

	err = __native_task_pkg_init();

	if (err)
		goto cleanup_pod;

#ifdef CONFIG_XENO_OPT_NATIVE_SEM
	err = __native_sem_pkg_init();

	if (err)
		goto cleanup_task;
#endif /* CONFIG_XENO_OPT_NATIVE_SEM */

#ifdef CONFIG_XENO_OPT_NATIVE_EVENT
	err = __native_event_pkg_init();

	if (err)
		goto cleanup_sem;
#endif /* CONFIG_XENO_OPT_NATIVE_EVENT */

#ifdef CONFIG_XENO_OPT_NATIVE_MUTEX
	err = __native_mutex_pkg_init();

	if (err)
		goto cleanup_event;
#endif /* CONFIG_XENO_OPT_NATIVE_MUTEX */

#ifdef CONFIG_XENO_OPT_NATIVE_COND
	err = __native_cond_pkg_init();

	if (err)
		goto cleanup_mutex;
#endif /* CONFIG_XENO_OPT_NATIVE_MUTEX */

#ifdef CONFIG_XENO_OPT_NATIVE_PIPE
	err = __native_pipe_pkg_init();

	if (err)
		goto cleanup_cond;
#endif /* CONFIG_XENO_OPT_NATIVE_PIPE */

#ifdef CONFIG_XENO_OPT_NATIVE_QUEUE
	err = __native_queue_pkg_init();

	if (err)
		goto cleanup_pipe;
#endif /* CONFIG_XENO_OPT_NATIVE_QUEUE */

#ifdef CONFIG_XENO_OPT_NATIVE_HEAP
	err = __native_heap_pkg_init();

	if (err)
		goto cleanup_queue;
#endif /* CONFIG_XENO_OPT_NATIVE_HEAP */

#ifdef CONFIG_XENO_OPT_NATIVE_ALARM
	err = __native_alarm_pkg_init();

	if (err)
		goto cleanup_heap;
#endif /* CONFIG_XENO_OPT_NATIVE_HEAP */

#ifdef CONFIG_XENO_OPT_NATIVE_INTR
	err = __native_intr_pkg_init();

	if (err)
		goto cleanup_alarm;
#endif /* CONFIG_XENO_OPT_NATIVE_INTR */

#if defined(__KERNEL__) && defined(CONFIG_XENO_OPT_PERVASIVE)
	err = __native_syscall_init();

	if (err)
		goto cleanup_intr;
#endif /* __KERNEL__ && CONFIG_XENO_OPT_PERVASIVE */

	xnprintf("starting native API services.\n");

	return 0;		/* SUCCESS. */

#if defined(__KERNEL__) && defined(CONFIG_XENO_OPT_PERVASIVE)
      cleanup_intr:
#endif /* __KERNEL__ && CONFIG_XENO_OPT_PERVASIVE */

#ifdef CONFIG_XENO_OPT_NATIVE_INTR
	__native_intr_pkg_cleanup();

      cleanup_alarm:
#endif /* CONFIG_XENO_OPT_NATIVE_INTR */

#ifdef CONFIG_XENO_OPT_NATIVE_ALARM
	__native_alarm_pkg_cleanup();

      cleanup_heap:
#endif /* CONFIG_XENO_OPT_NATIVE_ALARM */

#ifdef CONFIG_XENO_OPT_NATIVE_HEAP
	__native_heap_pkg_cleanup();

      cleanup_queue:
#endif /* CONFIG_XENO_OPT_NATIVE_HEAP */

#ifdef CONFIG_XENO_OPT_NATIVE_QUEUE
	__native_queue_pkg_cleanup();

      cleanup_pipe:
#endif /* CONFIG_XENO_OPT_NATIVE_QUEUE */

#ifdef CONFIG_XENO_OPT_NATIVE_PIPE
	__native_pipe_pkg_cleanup();

      cleanup_cond:
#endif /* CONFIG_XENO_OPT_NATIVE_PIPE */

#ifdef CONFIG_XENO_OPT_NATIVE_COND
	__native_cond_pkg_cleanup();

      cleanup_mutex:
#endif /* CONFIG_XENO_OPT_NATIVE_COND */

#ifdef CONFIG_XENO_OPT_NATIVE_MUTEX
	__native_mutex_pkg_cleanup();

      cleanup_event:
#endif /* CONFIG_XENO_OPT_NATIVE_MUTEX */

#ifdef CONFIG_XENO_OPT_NATIVE_EVENT
	__native_event_pkg_cleanup();

      cleanup_sem:
#endif /* CONFIG_XENO_OPT_NATIVE_EVENT */

#ifdef CONFIG_XENO_OPT_NATIVE_SEM
	__native_sem_pkg_cleanup();

      cleanup_task:
#endif /* CONFIG_XENO_OPT_NATIVE_SEM */

	__native_task_pkg_cleanup();

      cleanup_pod:

	xntbase_free(__native_tbase);

	xncore_detach(err);

      fail:

	xnlogerr("native skin init failed, code %d.\n", err);

	return err;
}

void SKIN_EXIT(native)
{
	xnprintf("stopping native API services.\n");

#ifdef CONFIG_XENO_OPT_NATIVE_INTR
	__native_intr_pkg_cleanup();
#endif /* CONFIG_XENO_OPT_NATIVE_INTR */

#ifdef CONFIG_XENO_OPT_NATIVE_ALARM
	__native_alarm_pkg_cleanup();
#endif /* CONFIG_XENO_OPT_NATIVE_ALARM */

#ifdef CONFIG_XENO_OPT_NATIVE_HEAP
	__native_heap_pkg_cleanup();
#endif /* CONFIG_XENO_OPT_NATIVE_HEAP */

#ifdef CONFIG_XENO_OPT_NATIVE_QUEUE
	__native_queue_pkg_cleanup();
#endif /* CONFIG_XENO_OPT_NATIVE_QUEUE */

#ifdef CONFIG_XENO_OPT_NATIVE_PIPE
	__native_pipe_pkg_cleanup();
#endif /* CONFIG_XENO_OPT_NATIVE_PIPE */

#ifdef CONFIG_XENO_OPT_NATIVE_COND
	__native_cond_pkg_cleanup();
#endif /* CONFIG_XENO_OPT_NATIVE_COND */

#ifdef CONFIG_XENO_OPT_NATIVE_MUTEX
	__native_mutex_pkg_cleanup();
#endif /* CONFIG_XENO_OPT_NATIVE_MUTEX */

#ifdef CONFIG_XENO_OPT_NATIVE_EVENT
	__native_event_pkg_cleanup();
#endif /* CONFIG_XENO_OPT_NATIVE_EVENT */

#ifdef CONFIG_XENO_OPT_NATIVE_SEM
	__native_sem_pkg_cleanup();
#endif /* CONFIG_XENO_OPT_NATIVE_SEM */

	__native_task_pkg_cleanup();

#if defined(__KERNEL__) && defined(CONFIG_XENO_OPT_PERVASIVE)
	__native_syscall_cleanup();
#endif /* __KERNEL__ && CONFIG_XENO_OPT_PERVASIVE */

	xntbase_free(__native_tbase);

	xncore_detach(XNPOD_NORMAL_EXIT);
}

module_init(__native_skin_init);
module_exit(__native_skin_exit);
