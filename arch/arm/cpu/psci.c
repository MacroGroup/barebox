// SPDX-License-Identifier: GPL-2.0-only

#define pr_fmt(fmt)  "psci: " fmt

#include <common.h>
#include <asm/psci.h>
#include <linux/arm-smccc.h>
#include <asm/secure.h>
#include <asm/system.h>
#include <restart.h>
#include <globalvar.h>
#include <init.h>
#include <magicvar.h>

#ifdef CONFIG_ARM_PSCI_DEBUG

/*
 * PSCI debugging functions. Board code can specify a putc() function
 * which is used for debugging output. Beware that this function is
 * called while the kernel is running. This means the kernel could have
 * turned off clocks, configured other baudrates and other stuff that
 * might confuse the putc function. So it can well be that the debugging
 * code itself is the problem when somethings not working. You have been
 * warned.
 */

static void (*__putc)(void *ctx, int c);
static void *putc_ctx;

void psci_set_putc(void (*putcf)(void *ctx, int c), void *ctx)
{
        __putc = putcf;
        putc_ctx = ctx;
}

void psci_putc(char c)
{
	if (__putc)
		__putc(putc_ctx, c);
}

int psci_puts(const char *str)
{
	int n = 0;

	while (*str) {
		if (*str == '\n')
			psci_putc('\r');

		psci_putc(*str);
		str++;
		n++;
	}

	return n;
}

int psci_printf(const char *fmt, ...)
{
	va_list args;
	uint i;
	char printbuffer[128];

	va_start(args, fmt);
	i = vsprintf(printbuffer, fmt, args);
	va_end(args);

	psci_puts(printbuffer);

        return i;
}
#endif

static struct psci_ops *psci_ops;

void psci_set_ops(struct psci_ops *ops)
{
	psci_ops = ops;
}

static unsigned long psci_version(void)
{
	psci_printf("%s\n", __func__);
	return ARM_PSCI_VER_1_0;
}

static unsigned long psci_cpu_suspend(u32 power_state, unsigned long entry,
				      u32 context_id)
{
	psci_printf("%s\n", __func__);

	if (psci_ops->cpu_suspend)
		return psci_ops->cpu_suspend(power_state, entry, context_id);

	return ARM_PSCI_RET_NOT_SUPPORTED;
}

static unsigned long psci_cpu_off(void)
{
	psci_printf("%s\n", __func__);

	if (psci_ops->cpu_off)
		return psci_ops->cpu_off();

	return ARM_PSCI_RET_NOT_SUPPORTED;
}

static unsigned long cpu_entry[ARM_SECURE_MAX_CPU];
static unsigned long context[ARM_SECURE_MAX_CPU];

static unsigned long psci_cpu_on(u32 cpu_id, unsigned long entry, u32 context_id)
{
	psci_printf("%s: %d 0x%08lx\n", __func__, cpu_id, entry);

	if (cpu_id >= ARM_SECURE_MAX_CPU)
		return ARM_PSCI_RET_INVAL;

	cpu_entry[cpu_id] = entry;
	context[cpu_id] = context_id;
	dsb();

	if (psci_ops->cpu_on)
		return psci_ops->cpu_on(cpu_id);

	return ARM_PSCI_RET_NOT_SUPPORTED;
}

static unsigned long psci_system_off(void)
{
	psci_printf("%s\n", __func__);

	if (psci_ops->system_off)
		psci_ops->system_off();

	while(1);

	return 0;
}

static unsigned long psci_system_reset(void)
{
	psci_printf("%s\n", __func__);

	if (psci_ops->system_reset)
		psci_ops->system_reset();

	restart_machine(0);
}

/* Avoid missing prototype warning, called from assembly */
void psci_entry(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6,
		struct arm_smccc_res *res);

void psci_entry(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6,
		struct arm_smccc_res *res)
{
	psci_printf("%s entry, function: 0x%08x\n", __func__, r0);

	switch (r0) {
	case ARM_PSCI_0_2_FN_PSCI_VERSION:
		res->a0 = psci_version();
		break;
	case ARM_PSCI_0_2_FN_CPU_SUSPEND:
		res->a0 = psci_cpu_suspend(r1, r2, r3);
		break;
	case ARM_PSCI_0_2_FN_CPU_OFF:
		res->a0 = psci_cpu_off();
		break;
	case ARM_PSCI_0_2_FN_CPU_ON:
		res->a0 = psci_cpu_on(r1, r2, r3);
		break;
	case ARM_PSCI_0_2_FN_SYSTEM_OFF:
		psci_system_off();
		break;
	case ARM_PSCI_0_2_FN_SYSTEM_RESET:
		psci_system_reset();
		break;
	default:
		res->a0 = ARM_PSCI_RET_NOT_SUPPORTED;
		break;
	}
}

/* Avoid missing prototype warning, called from assembly */
int psci_cpu_entry_c(void);

static int of_psci_do_fixup(struct device_node *root, void *unused)
{
	if (bootm_arm_security_state() < ARM_STATE_NONSECURE)
		return 0;

	return of_psci_fixup(root, ARM_PSCI_VER_1_0, "smc");
}

int psci_cpu_entry_c(void)
{
	void (*entry)(u32 context);
	int cpu;
	u32 context_id;

	__armv7_secure_monitor_install();
	cpu = psci_get_cpu_id();
	entry = (void *)cpu_entry[cpu];
	context_id = context[cpu];

	if (bootm_arm_security_state() == ARM_STATE_HYP)
		armv7_switch_to_hyp();

	psci_printf("core #%d enter function 0x%p\n", cpu, entry);

	entry(context_id);

	while (1);
}

static int armv7_psci_init(void)
{
	return of_register_fixup(of_psci_do_fixup, NULL);
}
device_initcall(armv7_psci_init);

