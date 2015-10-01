/*
 * Copyright (C) 2001,2002,2003,2004 Philippe Gerum <rpm@xenomai.org>.
 *
 * ARM port
 *   Copyright (C) 2005 Stelian Pop
 * 
 * ARM64 port
 *   Copyright (C) 2015 Dmitriy Cherkasov <dmitriy@mperpetuo.com>
 *
 * Xenomai is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Xenomai is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Xenomai; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <linux/sched.h>
#include <linux/ipipe.h>
#include <linux/mm.h>
#include <linux/jump_label.h>
#include <asm/mmu_context.h>
#include <cobalt/kernel/thread.h>
#include <asm/fpsimd.h>
#include <asm/processor.h>
#include <asm/hw_breakpoint.h>
#include <asm/fpsimd.h>

#if defined(CONFIG_XENO_ARCH_FPU)

static void enable_fpsimd(void) {
	__asm__ __volatile__("mrs x1, cpacr_el1\n\
			orr x1, x1, #(0x3 << 20)\n\
			msr cpacr_el1, x1\n\
			isb" : : : "x1", "memory", "cc");
}

static void disable_fpsimd(void) {
	__asm__ __volatile__("mrs x1, cpacr_el1\n\
				and x1, x1, #~(0x3 << 20)\n\
				msr cpacr_el1, x1\n\
				isb" : : : "x1", "memory", "cc");
}

int xnarch_fault_fpu_p(struct ipipe_trap_data *d)
{
	/* check if this is an FPU access trap to be handled by Xenomai */
	if(d->exception == IPIPE_TRAP_FPU_ACC){
		return 1;
	}
	/* FPU already enabled, propagate fault to kernel */
	return 0;
}

static inline struct fpsimd_state *get_fpu_owner(struct xnarchtcb *tcb) {
	return &(tcb->core.tsp->fpsimd_state);
}

void xnarch_leave_root(struct xnthread *root)
{
	struct xnarchtcb *rootcb = xnthread_archtcb(root);
	rootcb->fpup = get_fpu_owner(rootcb);
	disable_fpsimd();
}

void xnarch_save_fpu(struct xnthread *thread)
{
	struct xnarchtcb *tcb = &(thread->tcb);
	if (xnarch_fpu_ptr(tcb))
		fpsimd_save_state(tcb->fpup);
}

void xnarch_switch_fpu(struct xnthread *from, struct xnthread *to)
{
	struct fpsimd_state *const from_fpup = from ? from->tcb.fpup : NULL;
	struct fpsimd_state *const to_fpup = to->tcb.fpup;

	/*
	 * This only gets called if XNFPU flag is set, or if migrating to Linux.
	 * In both cases, this means turn on FPU and switch.
	 */
	enable_fpsimd();

	if (xnthread_test_state(to, XNROOT) == 0) {
		if (from_fpup == to_fpup)
			return;

		if (from_fpup)
			fpsimd_save_state(from_fpup);

		fpsimd_load_state(to_fpup);
	}
	else {
		/* Going to Linux. */
		if (from_fpup)
			fpsimd_save_state(from_fpup);

		fpsimd_load_state(to_fpup);
	}

}

int xnarch_handle_fpu_fault(struct xnthread *from, 
			struct xnthread *to, struct ipipe_trap_data *d)
{
	spl_t s;

	if (xnthread_test_state(to, XNFPU))
		/* FPU is already enabled, probably an exception */
	   return 0;

	xnlock_get_irqsave(&nklock, s);
	xnthread_set_state(to, XNFPU);
	xnlock_put_irqrestore(&nklock, s);

	xnarch_switch_fpu(from, to);

	return 1;

}

void xnarch_init_shadow_tcb(struct xnthread *thread)
{
	spl_t s;
	struct xnarchtcb *tcb = xnthread_archtcb(thread);

	tcb->fpup = &(tcb->core.host_task->thread.fpsimd_state);

	xnlock_get_irqsave(&nklock, s);
	xnthread_clear_state(thread, XNFPU);
	xnlock_put_irqrestore(&nklock, s);

}
#endif /* CONFIG_XENO_ARCH_FPU */

/* Switch support functions */
static void xnarch_tls_thread_switch(struct task_struct *next)
{
	unsigned long tpidr, tpidrro;

	if (!is_compat_task()) {
		asm("mrs %0, tpidr_el0" : "=r" (tpidr));
		current->thread.tp_value = tpidr;
	}

	if (is_compat_thread(task_thread_info(next))) {
		tpidr = 0;
		tpidrro = next->thread.tp_value;
	} else {
		tpidr = next->thread.tp_value;
		tpidrro = 0;
	}

	asm(
	"	msr	tpidr_el0, %0\n"
	"	msr	tpidrro_el0, %1"
	: : "r" (tpidr), "r" (tpidrro));
}

#ifdef CONFIG_PID_IN_CONTEXTIDR
static inline void xnarch_contextidr_thread_switch(struct task_struct *next)
{
	asm(
	"	msr	contextidr_el1, %0\n"
	"	isb"
	:
	: "r" (task_pid_nr(next)));
}
#else
static inline void xnarch_contextidr_thread_switch(struct task_struct *next)
{
}
#endif
/* End switch support functions */

void xnarch_switch_to(struct xnthread *out, struct xnthread *in)
{
	struct xnarchtcb *out_tcb = &out->tcb, *in_tcb = &in->tcb;
	struct mm_struct *prev_mm, *next_mm;
	struct task_struct *next;

	next = in_tcb->core.host_task;
	prev_mm = out_tcb->core.active_mm;

	next_mm = in_tcb->core.mm;
	if (next_mm == NULL) {
		in_tcb->core.active_mm = prev_mm;
		enter_lazy_tlb(prev_mm, next);
	} else {
		ipipe_switch_mm_head(prev_mm, next_mm, next);
		/*
		 * We might be switching back to the root thread,
		 * which we preempted earlier, shortly after "current"
		 * dropped its mm context in the do_exit() path
		 * (next->mm == NULL). In that particular case, the
		 * kernel expects a lazy TLB state for leaving the mm.
		 */
		if (next->mm == NULL)
			enter_lazy_tlb(prev_mm, next);
	}

	xnarch_tls_thread_switch(in_tcb->core.tip->task);
	xnarch_contextidr_thread_switch(in_tcb->core.tip->task);

	/* check if we need to switch FPU on return to Linux */
	if (xnthread_test_state(in, XNROOT) == 1)
		xnarch_switch_fpu(out, in);

	/*
	 * Complete any pending TLB or cache maintenance on this CPU in case
	 * the thread migrates to a different CPU.
	 */
	dsb(ish);

	cpu_switch_to(out_tcb->core.tip->task, in_tcb->core.tip->task);
}

int xnarch_escalate(void)
{
	if (ipipe_root_p) {
		ipipe_raise_irq(cobalt_pipeline.escalate_virq);
		return 1;
	}

	return 0;
}
