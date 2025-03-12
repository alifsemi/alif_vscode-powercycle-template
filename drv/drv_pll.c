#include <stdint.h>
#include "RTE_Components.h"
#include CMSIS_device_header
#include "drv_counter.h"
#include "drv_pll.h"

#ifndef HW_REG32
#define HW_REG32(u,v) (*((volatile uint32_t *)(u + v)))
#endif

#define SE_CGU_OSC_CTRL     0x1A602000
#define SE_CGU_PLL_LOCK     0x1A602004
#define SE_CGU_PLL_SEL      0x1A602008
#define SE_CGU_ESCLK_SEL    0x1A602010
#define SE_ANATOP_REG1      0x1A604034
#define SE_XO_REG1          0x1A605020
#define SE_MCU_CLKPLL_REG1  0x1A605024
#define SE_MCU_CLKPLL_REG2  0x1A605028
#define SE_MCU_CLKPLL_REG3  0x1A60502C

static bool s_xtal_started = false;
static bool s_clkpll_started = false;

void OSC_xtal_start(bool faststart, bool boost)
{
    if (s_xtal_started == true) return;

    /* Enable bandgap */
    HW_REG32(SE_ANATOP_REG1, 0) = 0x11;

    /* Enable HFXO */
    uint32_t xo_reg1_default = 0x11D08439;
    uint32_t val = xo_reg1_default;
    if (faststart) {
      val |= 1U << 1;
    }
    if (boost) {
      val |= 1U << 6;
    }
    HW_REG32(SE_XO_REG1, 0) = val;
    delay_us_refclk(500);

    HW_REG32(SE_XO_REG1, 0) = xo_reg1_default;

    s_xtal_started = true;
}

void OSC_xtal_stop()
{
    if (s_xtal_started == false) return;

    HW_REG32(SE_ANATOP_REG1, 0) = 0x10;
    HW_REG32(SE_XO_REG1, 0) = 0x11D08400;
    s_xtal_started = false;
}

void PLL_clkpll_start(bool faststart)
{
    if (s_xtal_started == false) return;
    if (s_clkpll_started == true) return;

    uint32_t reg1_val = 0x19 << 20;
    uint32_t reg2_val = 0x85967BFE;

    /* set fast start bit if needed */
    if (faststart) {
        reg1_val |= 1U << 30;
    }

    /* apply initial config to PLL, optionally add faststart */
    HW_REG32(SE_MCU_CLKPLL_REG1, 0) = reg1_val;
    HW_REG32(SE_MCU_CLKPLL_REG2, 0) = reg2_val;

    /* release reset to PLL, wait to settle */
    reg1_val |=  (1U << 31);
    HW_REG32(SE_MCU_CLKPLL_REG1, 0) = reg1_val;
    delay_us_refclk(50);

    /* clear fast start bit if needed */
    if (faststart) {
        reg1_val &= ~(1U << 30);
        HW_REG32(SE_MCU_CLKPLL_REG1, 0) = reg1_val;
    }

    /* set PLL LOCK bit */
    HW_REG32(SE_CGU_PLL_LOCK, 0) = 1;
    s_clkpll_started = true;
}

void PLL_clkpll_stop()
{
    if (s_clkpll_started == false) return;

    /* clear PLL LOCK bit */
    s_clkpll_started = false;
    HW_REG32(SE_CGU_PLL_LOCK, 0) = 0;

    HW_REG32(SE_MCU_CLKPLL_REG1, 0) = 0;
    HW_REG32(SE_MCU_CLKPLL_REG2, 0) = 0;
}

void OSC_rc_to_xtal()
{
    /* CGU Registers */
    /* OSC Control Register (0x00)
     * sys_xtal_sel[0] 0: 76.8M HFRC/X, 1: 76.8M HFXOx2
     * SYS_REFCLK, SYSPLL, CPUPLL
     *
     * periph_xtal_sel[4] 0: 38.4M HFRC/X/2, 1: 38.4M HFXO/Z
     * SYS_UART, MRAM_EFUSE, I2S, CAN_FD, MIPI
     *
     * clkmon_ena[16] 0: disable 1: enable HFXO clock monitor
     * xtal_dead[20] read-only status of HFXO clock monitor
     */
    HW_REG32(SE_CGU_OSC_CTRL, 0) |= 0x11;

    /* ESCLK Select Register (0x10)
     * es0_pll_sel[1:0]     0: 100M PLL, 1: 200M PLL, 2: 300M PLL, 3: 400M PLL
     * es1_pll_sel[5:4]     0:  60M PLL, 1:  80M PLL, 2: 120M PLL, 3: 160M PLL
     * es0_osc_sel[9:8]     0: 76.8M HFRC, 1: 38.4M HFRC, 2: 76.8M HFXO, 3: 38.4M HFXO
     * es1_osc_sel[13:12]   0: 76.8M HFRC, 1: 38.4M HFRC, 2: 76.8M HFXO, 3: 38.4M HFXO
     */
    HW_REG32(SE_CGU_ESCLK_SEL, 0) = (2U << 12) | (0U << 8) | (3U << 4) | (0U << 0);
}

void OSC_xtal_to_rc()
{
    /* Switch from HFRC to 38.4M HFXO */
    HW_REG32(SE_CGU_OSC_CTRL, 0) &= ~0x11;

    /* Switch from HFRC to 38.4M HFXO
     * esclk_sel register
     *  es0_osc_sel[9:8]:   0: 76.8M HFRC, 1: 38.4M HFRC, 2: 76.8M HFXO, 3: 38.4M HFXO
     *  es1_osc_sel[13:12]: 0: 76.8M HFRC, 1: 38.4M HFRC, 2: 76.8M HFXO, 3: 38.4M HFXO
     */
    HW_REG32(SE_CGU_ESCLK_SEL, 0) &= ~(0xFFU << 8);
}

static void PLL_clock_mux_select(uint32_t mux_select)
{
    /* Switch from non-PLL to PLL clock
     *  osc_mix_clk is result of sys_xtal_sel bit
     *  osc_9p6M_clk is result of periph_xtal_sel
     *  ESx HFRC clk is result of
     *
     *  REFCLK[0] - osc_76p8M_clk  or PLL_80M
     *  SYS[4]    - osc_76p8M_clk  or PLL_160M (AXI)
     *  UART[8]   - osc_9p6M_clk or PLL_10M
     *  ES0[16]   - HFRC/HFXO or PLL
     *  ES1[20]   - HFRC/HFXO or PLL
     */
    if (mux_select) {
        HW_REG32(SE_CGU_PLL_SEL, 0) = 0x100010;
    }
    else {
        HW_REG32(SE_CGU_PLL_SEL, 0) = 0;
    }
}

int32_t PLL_status() {
    int32_t ret = 0;

    if (s_xtal_started == true)
        ret |= 1U << 0;

    if (s_clkpll_started == true)
        ret |= 1U << 1;

    return ret;
}

void OSC_initialize()
{
    refclk_cntr_init();
    OSC_xtal_start(true, true);
    OSC_rc_to_xtal();
}

void OSC_uninitialize()
{
    OSC_xtal_to_rc();
    OSC_xtal_stop();
}

void PLL_initialize()
{
    OSC_xtal_start(true, true);
    PLL_clkpll_start(true);

    OSC_rc_to_xtal();
    PLL_clock_mux_select(1);
}

void PLL_uninitialize()
{
    PLL_clock_mux_select(0);
    PLL_clkpll_stop();
}
