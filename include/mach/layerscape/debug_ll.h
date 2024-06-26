/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __MACH_LAYERSCAPE_DEBUG_LL_H__
#define __MACH_LAYERSCAPE_DEBUG_LL_H__

#include <io.h>
#include <soc/fsl/immap_lsch2.h>
#include <soc/fsl/immap_lsch3.h>

#define __LS_UART_BASE(num)	LSCH2_NS16550_COM##num
#define LS_UART_BASE(num) __LS_UART_BASE(num)

#define __LSCH3_UART_BASE(num)	LSCH3_NS16550_COM##num
#define LSCH3_UART_BASE(num) __LSCH3_UART_BASE(num)

#ifdef CONFIG_DEBUG_LAYERSCAPE_UART

static inline uint8_t debug_ll_read_reg(void __iomem *base, int reg)
{
	return readb(base + reg);
}

static inline void debug_ll_write_reg(void __iomem *base, int reg, uint8_t val)
{
	writeb(val, base + reg);
}

#include <debug_ll/ns16550.h>

static inline void ls1046a_uart_setup(void *base)
{
	uint16_t divisor;

	divisor = debug_ll_ns16550_calc_divisor(300000000);
	debug_ll_ns16550_init(base, divisor);
}

static inline void ls1046a_debug_ll_init(void)
{
	void __iomem *base = IOMEM(LSCH3_UART_BASE(CONFIG_DEBUG_LAYERSCAPE_UART_PORT));

	ls1046a_uart_setup(base);
}

static inline void ls1028a_uart_setup(void *base)
{
	uint16_t divisor;

	divisor = debug_ll_ns16550_calc_divisor(200000000);
	debug_ll_ns16550_init(base, divisor);
}

static inline void ls1028a_debug_ll_init(void)
{
	void __iomem *base = IOMEM(LS_UART_BASE(CONFIG_DEBUG_LAYERSCAPE_UART_PORT));

	ls1028a_uart_setup(base);
}

static inline void ls102xa_uart_setup(void *base)
{
	uint16_t divisor;

	divisor = debug_ll_ns16550_calc_divisor(150000000);
	debug_ll_ns16550_init(base, divisor);
}

static inline void ls102xa_debug_ll_init(void)
{
	void __iomem *base = IOMEM(LS_UART_BASE(CONFIG_DEBUG_LAYERSCAPE_UART_PORT));

	ls102xa_uart_setup(base);
}

static inline void PUTC_LL(int c)
{
	void __iomem *base = IOMEM(LS_UART_BASE(CONFIG_DEBUG_LAYERSCAPE_UART_PORT));

	debug_ll_ns16550_putc(base, c);
}

static inline void layerscape_uart_putc(void *base, int c)
{
	debug_ll_ns16550_putc(base, c);
}

#else

static inline void ls1046a_uart_setup(void *base) { }
static inline void ls1046a_debug_ll_init(void) { }
static inline void ls1028a_uart_setup(void *base) { }
static inline void ls1028a_debug_ll_init(void) { }
static inline void ls102xa_uart_setup(void *base) { }
static inline void ls102xa_debug_ll_init(void) { }
static inline void layerscape_uart_putc(void *base, int c) { }

#endif

#endif /* __MACH_LAYERSCAPE_DEBUG_LL_H__ */
