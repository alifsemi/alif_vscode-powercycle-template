#include <stdio.h>
#include <stdint.h>
#include <cmsis_compiler.h>

#include "debug_pwr.h"

void DEBUG_power_decoded() {
    uint32_t reg_data;

    /* power domains list */
    printf("Power Domains:\r\n");
    reg_data = *((uint32_t *)0x1A60A004);// VBATSEC PWR_CTRL
    printf("PD0=AON_STOP\r\n");
    printf("PD1=%u\r\n",                  (reg_data & 1U));
    printf("PD2=AON_STBY\r\n");
    printf("PD3=%u\r\n",                  (*((uint32_t *)0x1A601008) & 15U) > 0 ? 1 : 0); // PPU-HE
    printf("PD4=n/a\r\n");
    printf("PD5=1\r\n");
    printf("MRAM=%u\r\n",                 (reg_data >> 16) & 3U);
    /* insert a delay to let PD6 idle before getting the power state */
    for(uint32_t loop_cnt = 0; loop_cnt < 10000; loop_cnt++) __NOP();
    reg_data = *((uint32_t *)0x1A010404);
    printf("PD6=%u\r\n",                  (reg_data >> 3) & 7U);
    printf("PD7=%u\r\n",                  (*((uint32_t *)0x1A600008) & 15U) > 0 ? 1 : 0); // PPU-HP
    printf("PD8=%u\r\n",                  (reg_data >> 2) & 1U);
    printf("PD9=n/a\r\n\n");

    /* retention settings & stop mode wakeup source enables */
    printf("RET Settings:\r\n");
    reg_data = *((uint32_t *)0x1A60A038);   // VBAT_ANA_REG1
    printf("RET LDO VOUT=%d\r\n",               (reg_data >> 4) & 15U);
    printf("RET LDO VBAT_EN=%u MAIN_EN=%u\r\n", (reg_data >> 8) & 1U, (reg_data >> 10) & 1U);
    reg_data = *((uint32_t *)0x1A60900C);   // VBATALL RET_CTRL
    printf("BK RAM=0x%X HE TCM=0x%X BLE MEM=0x%X\r\n", (reg_data & 1U), (reg_data >> 1) & 63U, (reg_data >> 7) & 15U);
    reg_data = *((uint32_t *)0x1A60A018);   // VBATSEC RET_CTRL
    printf("FW RAM=0x%X SE RAM=0x%X\r\n",   (reg_data & 1U), (reg_data >> 4) & 15U);
    printf("VBAT_WKUP =0x%08X\r\n\n",       *((uint32_t *)0x1A60A008));// VBATSEC WKUP_CTRL

    printf("PHY Power:\r\n");
    reg_data = *((uint32_t *)0x1A609008);   // VBATALL PWR_CTRL
    printf("PWR_CTRL=0x%08X\r\n\n", reg_data);

    /* bandgap trim settings */
    printf("TRIM Settings:\r\n");
    reg_data = *((uint32_t *)0x1A60A038);   // VBAT_ANA_REG1
    printf("RC OSC 32.768k=%u\r\n",       (reg_data & 15U));
    reg_data = *((uint32_t *)0x1A60A03C);   // VBAT_ANA_REG2
    printf("RC OSC 76.800M=%u.%u\r\n",    (reg_data >> 14) & 31U, (reg_data >> 10) & 1U);
    printf("RC OSC 76.800M=0x%02X\r\n",   ((reg_data >> 13) & 62U) | ((reg_data >> 10) & 1U));
    printf("DIG LDO VOUT=%u EN=%u\r\n",   (reg_data >> 6) & 15U, (reg_data >> 5) & 1U);
    printf("Bandgap PMU=%u\r\n",          (reg_data >> 1) & 15U);
    reg_data = *((uint32_t *)0x1A60A040);   // VBAT_ANA_REG2
    printf("Bandgap AON=%u\r\n",          (reg_data >> 23) & 15U);
    printf("AON LDO VOUT=%u\r\n",         (reg_data >> 27) & 15U);
    printf("MAIN LDO VOUT=%u\r\n\n",      (reg_data & 7U));

    /* miscellaneous */
    printf("MISC Settings:\r\n");
    printf("GPIO_FLEX=%s\r\n",            ((uint32_t *)0x1A609000) ? "1.8V" : "3.3V");// VBATALL GPIO_CTRL
    reg_data = *((uint32_t *)0x1A60A030);
    printf("DCDC_TRIM[8:3]=0x%03X (%u)\r\n\n",       (reg_data >> 3) & 63U, (reg_data >> 3) & 63U);
}
