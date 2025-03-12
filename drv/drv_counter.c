#include "drv_counter.h"
#include "soc_clk.h"
#include "clk.h"

void refclk_cntr_init() {
    *((volatile uint32_t *)0x1A200000) = 1;
}

uint32_t refclk_cntr_val() {
    return *((volatile uint32_t *)0x1A200008);
}

uint64_t refclk_cntr_val64() {
    return *((volatile uint64_t *)0x1A200008);
}

void s32k_cntr_init() {
    *((volatile uint32_t *)0x1A400000) = 1;
}

uint32_t s32k_cntr_val() {
    return *((volatile uint32_t *)0x1A400008);
}

uint64_t s32k_cntr_val64() {
    return *((volatile uint64_t *)0x1A400008);
}

void delay_ms_s32k(uint32_t nticks) {
    uint32_t s32k_cntr_value = s32k_cntr_val();
    nticks = (float) 32768 * nticks / 1000.0f;
    nticks = s32k_cntr_value + nticks;
    while (s32k_cntr_val() < nticks);
}

void delay_us_refclk(uint32_t nticks) {
    uint32_t refclk_cntr_value = refclk_cntr_val();
    nticks = (float) SystemREFClock * nticks / 1000000.0f;
    nticks = refclk_cntr_value + nticks;
    while (refclk_cntr_val() < nticks);
}

void refclk_cntr_enable_cntbase(uint32_t cntbase) {
    if (cntbase > 3) return;
    *((volatile uint32_t *)(0x1A220040 + (0x4 * cntbase))) = 0x21;
    *((volatile uint32_t *)(0x1A23002C + (0x10000 * cntbase))) = 0;
}

void refclk_cntr_disable_cntbase(uint32_t cntbase) {
    if (cntbase > 3) return;
    *((volatile uint32_t *)(0x1A23002C + (0x10000 * cntbase))) = 0;
    *((volatile uint32_t *)(0x1A220040 + (0x4 * cntbase))) = 0;
}

void refclk_cntr_enable_cntbase_intr(uint32_t cntbase, uint64_t compare_val) {
    if (cntbase > 3) return;
    *((volatile uint64_t *)(0x1A230020 + (0x10000 * cntbase))) = compare_val;
    *((volatile uint32_t *)(0x1A23002C + (0x10000 * cntbase))) = 1;
}

uint32_t refclk_cntr_disable_cntbase_intr(uint32_t cntbase) {
    if (cntbase > 3) return 0;
    *((volatile uint32_t *)(0x1A23002C + (0x10000 * cntbase))) = 0;

    uint32_t read_count = 1;
    while (*((volatile uint32_t *)(0x1A23002C + (0x10000 * cntbase))) != 0) read_count++;
    return read_count;
}

void s32k_cntr_enable_cntbase(uint32_t cntbase) {
    if (cntbase > 1) return;
    *((volatile uint32_t *)(0x1A420040 + (0x4 * cntbase))) = 0x21;
    *((volatile uint32_t *)(0x1A43002C + (0x10000 * cntbase))) = 0;
}

void s32k_cntr_disable_cntbase(uint32_t cntbase) {
    if (cntbase > 1) return;
    *((volatile uint32_t *)(0x1A43002C + (0x10000 * cntbase))) = 0;
    *((volatile uint32_t *)(0x1A420040 + (0x4 * cntbase))) = 0;
}

void s32k_cntr_enable_cntbase_intr(uint32_t cntbase, uint32_t compare_val) {
    if (cntbase > 1) return;
    *((volatile uint32_t *)(0x1A430020 + (0x10000 * cntbase))) = compare_val;
    *((volatile uint32_t *)(0x1A43002C + (0x10000 * cntbase))) = 1;
}

uint32_t s32k_cntr_disable_cntbase_intr(uint32_t cntbase) {
    if (cntbase > 1) return 0;
    *((volatile uint32_t *)(0x1A43002C + (0x10000 * cntbase))) = 0;

    uint32_t read_count = 1;
    while (*((volatile uint32_t *)(0x1A43002C + (0x10000 * cntbase))) != 0) read_count++;
    return read_count;
}
