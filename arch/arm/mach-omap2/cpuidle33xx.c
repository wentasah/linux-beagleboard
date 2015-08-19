/*
 * CPU idle for AM33XX SoCs
 *
 * Copyright (C) 2011 Texas Instruments Incorporated. http://www.ti.com/
 *
 * Derived from Davinci CPU idle code
 * (arch/arm/mach-davinci/cpuidle.c)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/cpuidle.h>
#include <linux/sched.h>
#include <asm/proc-fns.h>

#include <plat/emif.h>

#include "am33xx.h"

#define AM33XX_CPUIDLE_MAX_STATES	2

/* fields in am33xx_ops.flags */
#define AM33XX_CPUIDLE_FLAGS_DDR2_PWDN	BIT(0)

static void __iomem *emif_base;

static void __iomem * __init am33xx_get_mem_ctlr(void)
{
	void __iomem *am33xx_emif_base = ioremap(AM33XX_EMIF0_BASE, SZ_32K);

	if (!am33xx_emif_base)
		pr_warning("%s: Unable to map DDR3 controller", __func__);

	return am33xx_emif_base;
}

static void am33xx_save_ddr_power(int enter, bool pdown)
{
	u32 val;

	val = __raw_readl(emif_base + EMIF4_0_SDRAM_MGMT_CTRL);

	/* TODO: Choose the mode based on memory type */
	if (enter)
		val = SELF_REFRESH_ENABLE(64);
	else
		val = SELF_REFRESH_DISABLE;

	__raw_writel(val, emif_base + EMIF4_0_SDRAM_MGMT_CTRL);
}

static void am33xx_c2state_enter(u32 flags)
{
	am33xx_save_ddr_power(1, !!(flags & AM33XX_CPUIDLE_FLAGS_DDR2_PWDN));
}

static void am33xx_c2state_exit(u32 flags)
{
	am33xx_save_ddr_power(0, !!(flags & AM33XX_CPUIDLE_FLAGS_DDR2_PWDN));
}

/**
 * @dev: cpuidle device
 * @drv: cpuidle driver
 * @index: array index of target state to be programmed
 */
static int am33xx_enter_idle(struct cpuidle_device *dev,
			       struct cpuidle_driver *drv,
			       int index)
{
	local_irq_disable();

	if (index == 1 && emif_base)
		am33xx_c2state_enter(drv->states->flags);

	/* Wait for interrupt state */
	cpu_do_idle();

	if (index == 1 && emif_base)
		am33xx_c2state_exit(drv->states->flags);

	local_irq_enable();

	return 0;
}

static struct cpuidle_driver am33xx_idle_driver = {
	.name	= "cpuidle-am33xx",
	.owner	= THIS_MODULE,
	.states = {
		{
			.enter		  = am33xx_enter_idle,
			.exit_latency	  = 1,
			.target_residency = 10000,
			.flags		  = CPUIDLE_FLAG_TIME_VALID,
			.name		  = "WFI",
			.desc		  = "Wait for interrupt",
		},
		{
			.enter		  = am33xx_enter_idle,
			.exit_latency	  = 100,
			.target_residency = 10000,
			.flags		  = CPUIDLE_FLAG_TIME_VALID,
			.name		  = "DDR SR",
			.desc		  = "WFI and DDR Self Refresh",
		},
	},
	.state_count = AM33XX_CPUIDLE_MAX_STATES,
	.safe_state_index = 0,
};

int __init am33xx_idle_init(void)
{
	emif_base = am33xx_get_mem_ctlr();
	return cpuidle_register(&am33xx_idle_driver, NULL);
}
