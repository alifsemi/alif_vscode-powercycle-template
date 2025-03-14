#include <stdio.h>
#include <stdint.h>
#include "peripheral_types.h"
#include "debug_clks.h"

void DEBUG_frequencies() {
    uint32_t hfrc_top_clock = 76800000;
    uint32_t hfxo_top_clock = 38400000;
    uint32_t osc_mix_clk;   // result of sys_xtal_sel[0] in hf_osc_sel register
    uint32_t hfosc_clk;     // result of periph_xtal_sel[4] in hf_osc_sel register
    uint32_t syspll_clk, a32_cpuclk, gic_clk, ctrl_clk, dbg_clk;
    uint32_t syst_refclk, syst_aclk, syst_hclk, syst_pclk;
    uint32_t rtss_hp_clk, rtss_he_clk, pd4_clk;

    uint32_t hfrc_div_select = 0;
    uint32_t hfrc_div_active;
    uint32_t hfrc_div_standby;
    uint32_t hfxo_div;
    uint32_t bus_clk_div;
    uint32_t hf_osc_sel;
    uint32_t pll_clk_sel;
    uint32_t es_clk_sel;
    uint32_t clk_ena;

    uint32_t reg_data;

    reg_data = *((volatile uint32_t *)0x1A60A030);  // DCDC_REG1
    hfrc_div_select &= 3U;

    /* value of 0, 1, or 3 means HFRC ACTIVE divider is used */
    if (hfrc_div_select == 2)
        hfrc_div_select = 1;    // HFRC STANDBY is selected
    else
        hfrc_div_select = 0;    // HFRC ACTIVE is selected

    reg_data = *((volatile uint32_t *)0x1A60A03C);  // VBAT_ANA_REG2
    hfrc_div_active = (reg_data >> 11) & 7U;
    hfrc_div_standby = (reg_data >> 19) & 7U;

    if (hfrc_div_standby > 6) hfrc_div_standby += 3;        // 2^(7   + 3) = 1024 (75k)
    else if (hfrc_div_standby > 3) hfrc_div_standby += 2;   // 2^(4-6 + 2) = 64-256 (1.2M-300k)
    else if (hfrc_div_standby > 2) hfrc_div_standby += 1;   // 2^(3   + 1) = 16 (4.8M)
                                                            // 2^(0-2 + 0) = 1-4 (76.8M-19.2M)

    reg_data = *((volatile uint32_t *)0x1A604030);  // MISC_REG1
    hfxo_div = (reg_data >> 17) & 15U;
    if (hfxo_div > 7) {
        hfxo_div -= 8;
        if (hfxo_div > 6) hfxo_div += 3;            // 2^(7   + 3) = 1024 (75k)
        else if (hfxo_div > 3) hfxo_div += 2;       // 2^(4-6 + 2) = 64-256 (1.2M-300k)
        else if (hfxo_div > 2) hfxo_div += 1;       // 2^(3   + 1) = 16 (4.8M)
                                                    // 2^(0-2 + 0) = 1-4 (76.8M-19.2M)
    }
    hfxo_top_clock >>= hfxo_div;

    printf("Top Level Clock Sources\n\r");
    reg_data = *((volatile uint32_t *)0x1A60A000);  // MISC_CTRL
    printf("LF CLK SRC   %s\n\r", (reg_data & 1) ? "LFXO" : "LFRC");
    printf("HFRC ACTIVE  %8d Hz%s\n\r", hfrc_top_clock >> hfrc_div_active, hfrc_div_select ? "[ ]" : "[x]");
    printf("HFRC STANDBY %8d Hz%s\n\r", hfrc_top_clock >> hfrc_div_standby, hfrc_div_select ? "[x]" : "[ ]");
    printf("HFXO CLK     %8d Hz\n\r", hfxo_top_clock);
    printf("[x] means clock is selected\n\n\r");

    /* calculate hfrc_top_clock based on dcdc pfm enable bit, for B2 only */
    if (hfrc_div_select) {
        hfrc_top_clock >>= hfrc_div_standby;
    } else {
        hfrc_top_clock >>= hfrc_div_active;
    }

    hf_osc_sel = *((volatile uint32_t *)(0x1A602000));
    pll_clk_sel = *((volatile uint32_t *)(0x1A602008));
    es_clk_sel = *((volatile uint32_t *)(0x1A602010));
    clk_ena = *((volatile uint32_t *)(0x1A602014));
    bus_clk_div = *((volatile uint32_t *)(0x1A604020));

    /* calculate the osc_mix_clk */
    if (hf_osc_sel & 1) {
        osc_mix_clk = 76800000;
    }
    else {
        osc_mix_clk = hfrc_top_clock;
    }

    /* calculate the hfosc_clk */
    if (hf_osc_sel & 16) {
        hfosc_clk = hfxo_top_clock;
    }
    else {
        hfosc_clk = hfrc_top_clock >> 1;
    }
    printf("HFXO_OUT     %8d Hz%s\n\r", osc_mix_clk, (clk_ena & (1U << 18)) == 0 ? "*" : "");
    printf("HFOSC_CLK    %8d Hz%s\n\r", hfosc_clk, (clk_ena & (1U << 23)) == 0 ? "*" : "");

    /* calculate the REFCLK */
    if (pll_clk_sel & 1) {
        syst_refclk = 80000000;
    }
    else {
        syst_refclk = osc_mix_clk;
    }

    /* calculate the CPUPLL_CLK and SYSPLL_CLK */
    if (pll_clk_sel & 16) {
        syspll_clk = 160000000;
    }
    else {
        syspll_clk = osc_mix_clk;
    }
    printf("SYSPLL_CLK  %9d Hz%s\n\r", syspll_clk, (clk_ena & (1U << 0)) == 0 ? "*" : "");

    /* calculate the CTRL_CLK */
    /* CTRLCLK_CTRL value at [7:0] and
     * CTRLCLK_DIV0 value at [15:8] */
    uint32_t ctrlclk_status = 0;
    reg_data = *((volatile uint32_t *)(0x1A010830));
    ctrlclk_status |= (reg_data >> 8) & 0xFF;
    reg_data = *((volatile uint32_t *)(0x1A010834));
    ctrlclk_status |= ((reg_data >> 16) & 0xFF) << 8;
    if ((ctrlclk_status & 0xF) == 2) {
        ctrl_clk = syspll_clk;
        ctrl_clk /= ((ctrlclk_status >> 8) & 0xFF) + 1;      // ctrlclk divider is (n + 1)
    }
    else if ((ctrlclk_status & 0xF) == 1) {
        ctrl_clk = syst_refclk;
    }
    else {
        ctrl_clk = 0;
    }
    printf("CTRL_CLK    %9d Hz%s\n\r", ctrl_clk, (ctrlclk_status & 0xF) == 0 ? "*" : "");

    /* calculate the DBG_CLK */
    /* DBGCLK_CTRL value at [7:0] and
     * DBGCLK_DIV0 value at [15:8] */
    uint32_t dbgclk_status = 0;
    reg_data = *((volatile uint32_t *)(0x1A010840));
    dbgclk_status |= (reg_data >> 8) & 0xFF;
    reg_data = *((volatile uint32_t *)(0x1A010844));
    dbgclk_status |= ((reg_data >> 16) & 0xFF) << 8;
    if ((dbgclk_status & 0xF) == 2) {
        dbg_clk = syspll_clk;
        dbg_clk /= ((dbgclk_status >> 8) & 0xFF) + 1;        // dbgclk divider is (n + 1)
    }
    else if ((dbgclk_status & 0xF) == 1) {
        dbg_clk = syst_refclk;
    }
    else {
        dbg_clk = 0;
    }
    printf("DBG_CLK     %9d Hz%s\n\r", dbg_clk, (dbgclk_status & 0xF) == 0 ? "*" : "");

    /* calculate the SYST_ACLK, HCLK, PCLK */
    /* ACLK_CTRL value at [7:0] and
     * ACLK_DIV0 value at [15:8] */
    uint32_t aclk_status = 0;
    reg_data = *((volatile uint32_t *)(0x1A010820));
    aclk_status |= (reg_data >> 8) & 0xFF;
    reg_data = *((volatile uint32_t *)(0x1A010824));
    aclk_status |= ((reg_data >> 16) & 0xFF) << 8;
    if ((aclk_status & 0xF) == 2) {
        syst_aclk = syspll_clk;
        syst_aclk /= ((aclk_status >> 8) & 0xFF) + 1;        // aclk divider is (n + 1)
    }
    else if ((aclk_status & 0xF) == 1) {
        syst_aclk = syst_refclk;
    }
    else {
        syst_aclk = 0;
    }

    syst_hclk = syspll_clk >> ((bus_clk_div >> 8) & 0xF);    // hclk & pclk divider is (2^n)
    syst_pclk = syspll_clk >> (bus_clk_div & 0xF);

    printf("SYST_ACLK   %9d Hz%s\n\r", syst_aclk, (aclk_status & 0xF) == 0 ? "*" : "");
    printf("SYST_HCLK   %9d Hz\n\r", syst_hclk);
    printf("SYST_PCLK   %9d Hz\n\r", syst_pclk);
    printf("SYST_REFCLK %9d Hz\n\r", syst_refclk);

    const uint32_t pll_rtss_he_clocks[4] = {60000000UL, 80000000UL, 120000000UL, 160000000UL};

    /* calculate the RTSS_HE_CLK */
    if (pll_clk_sel & (1U << 20)) {
        rtss_he_clk = pll_rtss_he_clocks[(es_clk_sel >> 4) & 3U];
    }
    else {
        switch((es_clk_sel >> 12) & 3U) {
        case 0:
            rtss_he_clk = hfrc_top_clock;
            break;
        case 1:
            rtss_he_clk = hfrc_top_clock >> 1;
            break;
        case 2:
            rtss_he_clk = 76800000;
            break;
        case 3:
            rtss_he_clk = hfxo_top_clock;
            break;
        }
    }
    printf("RTSS_HE_CLK %9d Hz%s\n\r", rtss_he_clk, (clk_ena & (1U << 13)) == 0 ? "*" : "");
    printf("160M_CLK    %9d Hz%s\n\r", 160000000, (clk_ena & (1U << 20)) == 0 ? "*" : "");
    printf("20M/10M_CLK %9d Hz%s\n\r", 20000000, (clk_ena & (1U << 22)) == 0 ? "*" : "");
    printf("* means clock is gated\n\n\r");
}

void DEBUG_peripherals() {
    printf("EXPMST0_CTRL       = 0x%08X\r\n", CLKCTL_PER_SLV->EXPMST0_CTRL);
    printf("UART_CTRL          = 0x%08X\r\n", CLKCTL_PER_SLV->UART_CTRL);
    printf("CANFD_CTRL         = 0x%08X\r\n", CLKCTL_PER_SLV->CANFD_CTRL);
    printf("I2S0_CTRL          = 0x%08X\r\n", CLKCTL_PER_SLV->I2S0_CTRL);
    printf("I2S1_CTRL          = 0x%08X\r\n", CLKCTL_PER_SLV->I2S1_CTRL);
    printf("I3C_CTRL           = 0x%08X\r\n", CLKCTL_PER_SLV->I3C_CTRL);
    printf("SSI_CTRL           = 0x%08X\r\n", CLKCTL_PER_SLV->SSI_CTRL);
    printf("ADC_CTRL           = 0x%08X\r\n", CLKCTL_PER_SLV->ADC_CTRL);
    printf("DAC_CTRL           = 0x%08X\r\n", CLKCTL_PER_SLV->DAC_CTRL);
    printf("CMP_CTRL           = 0x%08X\r\n", CLKCTL_PER_SLV->CMP_CTRL);
    printf("CDC200_PIXCLK_CTRL = 0x%08X\r\n", CLKCTL_PER_MST->CDC200_PIXCLK_CTRL);
    printf("PERIPH_CLK_ENA     = 0x%08X\r\n", CLKCTL_PER_MST->PERIPH_CLK_ENA);
    printf("MIPI_CKEN          = 0x%08X\r\n", CLKCTL_PER_MST->MIPI_CKEN);
    printf("M55LOCAL_CLK_ENA   = 0x%08X\r\n", M55LOCAL_CFG->CLK_ENA);
    printf("M55LOCAL_I2S_CTRL  = 0x%08X\r\n\n", *((&M55LOCAL_CFG->CLK_ENA)+1));
}
