#include <RTE_Components.h>
#include CMSIS_device_header
#include "peripheral_types.h"
#include "soc_clk.h"

/*----------------------------------------------------------------------------
  Core identification function (0 = RTSS_HP, 1 = RTSS_HE)
 *----------------------------------------------------------------------------*/
uint32_t CoreID()
{
    uint32_t coreID = *((volatile uint32_t *)0xE00FEFE0UL);
    return coreID & 1;
}

/*----------------------------------------------------------------------------
  Core clock update function
 *----------------------------------------------------------------------------*/
uint32_t CoreClockUpdate()
{
    uint32_t PLL_CLK_SEL = CGU->PLL_CLK_SEL;
    uint32_t ESCLK_SEL = CGU->ESCLK_SEL;

    uint32_t const pll_rtss_clocks[4] = {60000000UL, 80000000UL, 120000000UL, 160000000UL};
    uint32_t const osc_rtss_clocks[4] = {76800000UL, 38400000UL, 76800000UL, 38400000UL};

    if ((PLL_CLK_SEL >> 20) & 1) {
        ESCLK_SEL = (ESCLK_SEL >> 4) & 0x3;
        SystemCoreClock = pll_rtss_clocks[ESCLK_SEL];

        return SystemCoreClock;
    }
    else {
        ESCLK_SEL = (ESCLK_SEL >> 12) & 0x3;
    }

    uint32_t shift_val = 0;

    /* ESCLK = 0, using 76.8M HFRC. ESCLK = 1, using 76.8M HFRC/2
     * HFRC is further divided by 1/X or 1/Y dividers */
    if (ESCLK_SEL < 2) {
#if 0
        /* calculate hfrc_top_clock based on DCDC_REG1[1:0] */
        if ((ANA_REG->DCDC_REG1 & 3U) == 2U) {
            shift_val = (ANA_REG->VBAT_ANA_REG2 >> 19) & 7U;
            if (shift_val > 6) shift_val += 3;          // 2 ^ (7 + 3) = 1024 (75k)
            else if (shift_val > 3) shift_val += 2;     // 2 ^ (4-6 + 2) = 64-256 (300k-1.2M)
            else if (shift_val > 2) shift_val += 1;     // 2 ^ (3 + 1) = 16 (4.8M)
                                                        // 2 ^ (0-2) = 1-4 (19.2M-76.8M)
        }
        else
#endif
        {
            shift_val = (ANA_REG->VBAT_ANA_REG2 >> 11) & 7U;
        }
    }

    /* ESCLK == 2, using 76.8M HFXO. ESCLK == 3, using 38.4M HFXO
     * Only 38.4M HFXO option can be further divided by 1/Z divider */
    if (ESCLK_SEL == 3) {
        /* using HFXO 1/Z (xtal divider) (MISC_REG1) */
        shift_val = (*((volatile uint32_t *)0x1A604030) >> 17) & 15U;
        if (shift_val > 7) {
            shift_val -= 8;
            if (shift_val > 6) shift_val += 3;          // 2^(7   + 3) = 1024 (75k)
            else if (shift_val > 3) shift_val += 2;     // 2^(4-6 + 2) = 64-256 (1.2M-300k)
            else if (shift_val > 2) shift_val += 1;     // 2^(3   + 1) = 16 (4.8M)
                                                        // 2^(0-2 + 0) = 1-4 (76.8M-19.2M)
        }
    }

    SystemCoreClock = osc_rtss_clocks[ESCLK_SEL];
    SystemCoreClock >>= shift_val;
    return SystemCoreClock;
}

/*----------------------------------------------------------------------------
  SoC Top Clock update functions
 *----------------------------------------------------------------------------*/

uint32_t SocTopClockHFRC()
{
    /* fetch hfrc divider values */
    uint32_t reg_data = ANA_REG->VBAT_ANA_REG2;  // VBAT_ANA_REG2
    uint32_t hfrc_div;

#if 0
    /* calculate hfrc_top_clock based on DCDC_REG1[1:0] */
    if ((ANA_REG->DCDC_REG1 & 3U) == 2) {
        hfrc_div = (reg_data >> 19) & 7U;
        if (hfrc_div > 6) hfrc_div += 3;        // 2^(7   + 3) = 1024 (75k)
        else if (hfrc_div > 3) hfrc_div += 2;   // 2^(4-6 + 2) = 64-256 (1.2M-300k)
        else if (hfrc_div > 2) hfrc_div += 1;   // 2^(3   + 1) = 16 (4.8M)
                                                // 2^(0-2 + 0) = 1-4 (76.8M-19.2M)
    }
    else
#endif
    {
        hfrc_div = (reg_data >> 11) & 7U;
    }

    return 76800000 >> hfrc_div;
}

uint32_t SocTopClockHFXO()
{
    uint32_t hfxo_div = *((volatile uint32_t *)0x1A604030);  // MISC_REG1
    hfxo_div = (hfxo_div >> 17) & 15U;
    if (hfxo_div > 7) {
        hfxo_div -= 8;
        if (hfxo_div > 6) hfxo_div += 3;        // 2^(7   + 3) = 1024 (75k)
        else if (hfxo_div > 3) hfxo_div += 2;   // 2^(4-6 + 2) = 64-256 (1.2M-300k)
        else if (hfxo_div > 2) hfxo_div += 1;   // 2^(3   + 1) = 16 (4.8M)
                                                // 2^(0-2 + 0) = 1-4 (76.8M-19.2M)
    }
    return 38400000 >> hfxo_div;
}

/*----------------------------------------------------------------------------
  HFOSC clock update function
 *----------------------------------------------------------------------------*/
uint32_t SocTopClockHFOSC()
{
    /* calculate the HFOSC_CLK */
    if (CGU->OSC_CTRL & 16) {
        return SocTopClockHFXO();
    }
    else {
        return SocTopClockHFRC() >> 1;
    }
}

/*----------------------------------------------------------------------------
  SYST_REFCLK update function
 *----------------------------------------------------------------------------*/
uint32_t SystRefclkUpdate()
{
    /* calculate the SYST_REFCLK */
    if (CGU->PLL_CLK_SEL & 1) {
        return 80000000;
    }
    else {
        if (CGU->OSC_CTRL & 1) {
            return 76800000;
        }
        else {
            return SocTopClockHFRC();
        }
    }
}

/*----------------------------------------------------------------------------
  HOSTUARTCLK update function
 *----------------------------------------------------------------------------*/
uint32_t SystHostUartClkUpdate()
{
    /* calculate the HOSTUARTCLK */
    if (CGU->PLL_CLK_SEL & (1U << 8)) {
        /* PLL option is 10MHz */
        return 10000000;
    }
    else {
        /* either HFXO or HFRC option is 9.6MHz */
        if (CGU->OSC_CTRL & 16) {
            return SocTopClockHFXO() >> 2;  // HFXO div by 4
        }
        else {
            return SocTopClockHFRC() >> 3;  // HFRC div by 8
        }
    }
}

/*----------------------------------------------------------------------------
  Core clock config function
 *----------------------------------------------------------------------------*/
int32_t CoreClockConfig(uint32_t esclk_osc_sel, uint32_t esclk_pll_sel)
{
    /* Configured divider values should be 0 to 3
     */
    if ((esclk_osc_sel > 3) || (esclk_pll_sel > 3)) return -1;

    /* refer to CoreClockUpdate() function for how to set esclk_sel value */
    uint32_t reg_data = CGU->ESCLK_SEL;

    /* RTSS clock will be pll_rtss_clocks[esclk_sel] */
    reg_data &= ~(3U << 4);
    reg_data |= esclk_pll_sel << 4;

    /* RTSS clock will be osc_rtss_clocks[esclk_sel] */
    reg_data &= ~(3U << 12);
    reg_data |= esclk_osc_sel << 12;

    CGU->ESCLK_SEL = reg_data;

    return 0;
}

/*----------------------------------------------------------------------------
  Top level clock divider config function, refer to "OSC_76M_DIV_CTRL" Registers in HWRM
 *----------------------------------------------------------------------------*/
int32_t DivClockConfig(uint32_t div_active, uint32_t div_standby, uint32_t div_xtal)
{
    /* Configured divider values should be 0 to 7
     */
    if ((div_active > 7) || (div_standby > 7) || (div_xtal > 7)) return -1;

    /* VBAT_ANA_REG2 Register (0x1A60A03C)
     *
     * OSC_76M_DIV_CTRL_ACTIVE[13:11]
     *      in "active" mode, HFRC is divided by 2^x, where x = 0 to 7
     *
     * OSC_76M_DIV_CTRL_STBY[21:19]
     *      in "standby" mode, HFRC is divided
     *          by 2^x, where x = 0 to 2    (divide by 1 to 4)
     *          by 2^(x+1), where x = 3     (divide by 16)
     *          by 2^(x+2), where x = 4 to 6(divide by 64 to 256)
     *          by 2^(x+3), where x = 7     (divide by 1024)
     */

    uint32_t reg_data = ANA_REG->VBAT_ANA_REG2;
    reg_data &= ~((7U << 11) | (7U << 19));
    reg_data |= (div_active << 11) | (div_standby << 19);
    ANA_REG->VBAT_ANA_REG2 = reg_data;

    /* MISC_REG1 Register (0x1A604030)
     *
     * cont_clkDiv[19:17]
     *      HF XTAL is divided by 2^x, where x = 0 to 7
     * sel_clkDivHi[20]
     *      simply should be set to 0
     */
    reg_data = *((volatile uint32_t *)0x1A604030);
    reg_data &= ~(15U << 17);
    reg_data |= (div_xtal << 17);
    *((volatile uint32_t *)0x1A604030) = reg_data;

    return 0;
}

/*----------------------------------------------------------------------------
  Oscillator clock select function, refer to "OSC_CTRL Register" in HWRM
 *----------------------------------------------------------------------------*/
void OscClockConfig(uint32_t xtal_sel)
{
    /* OSC Control Register (0x00)
     *
     * sys_xtal_sel[0] 0: 76.8M HFRC, 1: 76.8M HFXOx2
     *      used by SYST_REFCLK, SYSPLL_CLK, CPUPLL_CLK
     *
     * periph_xtal_sel[4] 0: 38.4M HFRC, 1: 38.4M HFXO
     *      used by peripherals as HFOSC_CLK
     */
    uint32_t reg_data = CGU->OSC_CTRL;
    reg_data &= ~(0x11);
    reg_data |=  (0x11 & xtal_sel);
    CGU->OSC_CTRL = reg_data;
}

/*----------------------------------------------------------------------------
  PLL clock select function, refer to "PLL_CLK_SEL Register" in HWRM
 *----------------------------------------------------------------------------*/
void PllClockConfig(uint32_t pll_sel)
{
    /* Switch from non-PLL to PLL clock
     *  osc_76p8M_clk is result of sys_xtal_sel bit
     *  osc_9p6M_clk is result of periph_xtal_sel bit
     *  ESx HFRC/HFXO clk is result of ESCLK_SEL bits
     *
     *  SYST_REFCLK[0]  - osc_76p8M_clk or PLL- 80M
     *  SYST_ACLK[4]    - osc_76p8M_clk or PLL-160M (AXI)
     *  UART[8]         - osc_9p6M_clk  or PLL- 10M
     *  ES0[16]         - HFRC/HFXO or PLL, refer to ESCLK_SEL
     *  ES1[20]         - HFRC/HFXO or PLL, refer to ESCLK_SEL
     */
    uint32_t reg_data = CGU->PLL_CLK_SEL;
    reg_data &= ~(0x110111);
    reg_data |=  (0x110111 & pll_sel);
    CGU->PLL_CLK_SEL = reg_data;
}

/*----------------------------------------------------------------------------
  Bus clock select function, refer to "CLKCTL_SYS Registers" and "SYSTOP_CLK_DIV Register" in HWRM
 *----------------------------------------------------------------------------*/
int32_t BusClockConfig(uint32_t aclk_ctrl, uint32_t aclk_div, uint32_t hclk_div, uint32_t pclk_div)
{
    /* ACLK select should be 1 (SYST_REFCLK) or 2 (SYSPLL_CLK)
     * Note: ACLK divider is n + 1, where n is up to 31
     * Note: divider is only valid on 2 (SYSPLL_CLK)
     */
    if ((aclk_ctrl != 1) && (aclk_ctrl != 2)) return -1;
    if ((aclk_ctrl == 1) && (aclk_div != 0)) return -1;

    /* SYSPLL_CLK is further divided to create HCLK and PCLK
     * HCLK and PCLK divider should be 0 to 2
     * Note: divider is 2^n
     */
    if ((hclk_div > 2) || (pclk_div > 2)) return - 1;

    /* Refer to "ACLK_CTRL" and "ACLK_DIV0" Registers in the HWRM */
    *((volatile uint32_t *)0x1A010820) = aclk_ctrl;
    *((volatile uint32_t *)0x1A010824) = aclk_div;

    /* Refer to "SYSTOP_CLK_DIV Register" in the HWRM */
    AON->SYSTOP_CLK_DIV = hclk_div << 8 | pclk_div;

    return 0;
}
