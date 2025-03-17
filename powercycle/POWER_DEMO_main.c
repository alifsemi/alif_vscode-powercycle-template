#include <math.h>
#include <stdio.h>
#include <string.h>

#include <M55_HE.h>
#include <RTE_Components.h>
#include <se_services_port.h>
#include <retarget_config.h>
#include <Driver_GPIO.h>
#include <pinconf.h>
#include <aipm.h>
#include <pm.h>

#include "drv_counter.h"
#include "drv_lptimer.h"
#include "drv_pll.h"
#include "soc_clk.h"

#if defined(RTE_Compiler_IO_STDIN)
#include <retarget_stdin.h>
#endif

#if defined(RTE_Compiler_IO_STDOUT)
#include <retarget_stdout.h>
#endif

#include "POWER_DEMO_prints.h"
#include "debug_clks.h"
#include "debug_pwr.h"

#define TICKS_PER_SECOND        2000
volatile uint32_t ms_ticks;
void SysTick_Handler (void) { ms_ticks++; }
void delay_ms(uint32_t nticks) { nticks <<= 1; nticks += ms_ticks; while(ms_ticks < nticks); }
void delay_ms_wfi(uint32_t nticks) { nticks <<= 1; nticks += ms_ticks; while(ms_ticks < nticks) __WFI(); }
extern void npuTestStart(uint32_t test_count, uint32_t test_select);

#define BKRAM_INDEX_CYCLECNT    0
#define BKRAM_INDEX_WHILE1      1
#define BKRAM_INDEX_ETHOSU      2
#define BKRAM_INDEX_FIRSTBOOT   7

#define BKRAM_IN_TCM    defined(M55_HE)
#if BKRAM_IN_TCM
volatile uint32_t bk_ram_data[1000] __attribute__((section (".bss.noinit.bk_ram")));
#endif

#include "uart.h"
static void reconfigure_uart()
{
#if defined(RTE_Compiler_IO_STDIN_User) || defined(RTE_Compiler_IO_STDOUT_User)
    uart_set_baudrate((UART_Type*)LPUART_BASE, SystemCoreClock, PRINTF_UART_CONSOLE_BAUD_RATE);
#endif
}

static int32_t bk_ram_rd(uint32_t *data, uint32_t offset)
{
    if (offset < 1000) {
#if BKRAM_IN_TCM
        *data = bk_ram_data[offset];
#else
        volatile uint32_t *bk_ram = (uint32_t *)0x4902C000;
        *data = *(bk_ram + offset);
#endif
        return 0;
    }
    return -1;
}

static int32_t bk_ram_wr(uint32_t *data, uint32_t offset)
{
    if (offset < 1000) {
#if BKRAM_IN_TCM
        bk_ram_data[offset] = *data;
#else
        volatile uint32_t *bk_ram = (uint32_t *)0x4902C000;
        *(bk_ram + offset) = *data;
#endif
        return 0;
    }
    return -1;
}

void SERVICES_ret(uint32_t ret)
{
    printf("\r\n\nSERVICES function ret = %u\r\n\n", ret);
}

void SERVICES_response(uint32_t response)
{
    printf("\r\n\nSERVICES function response = %u\r\n\n", response);
}

static void boot_from_por()
{
    printf("RTSS-HE first boot\r\n\n");

    /* Initialize the SE services */
    uint32_t ret = 0;
    uint32_t service_response = 0;

    se_services_port_init();

    run_profile_t runp = {0};
    ret = SERVICES_get_run_cfg(se_services_s_handle, &runp, &service_response);
    if (ret != 0) {
        SERVICES_ret(ret);
    }
    if (service_response != 0) {
        SERVICES_response(service_response);
    }

    runp.aon_clk_src = CLK_SRC_LFXO;
    runp.run_clk_src = CLK_SRC_HFRC;
    runp.power_domains = 0;
    runp.memory_blocks = MRAM_MASK | BACKUP4K_MASK;
    runp.memory_blocks |= SERAM_1_MASK | SERAM_2_MASK | SERAM_3_MASK | SERAM_4_MASK;
    runp.memory_blocks |= SRAM4_1_MASK | SRAM4_2_MASK | SRAM4_3_MASK | SRAM4_4_MASK;
    runp.memory_blocks |= SRAM5_1_MASK | SRAM5_2_MASK | SRAM5_3_MASK | SRAM5_4_MASK | SRAM5_5_MASK;
    runp.scaled_clk_freq = SCALED_FREQ_RC_STDBY_1_2_MHZ;
    runp.dcdc_voltage = 750;
    runp.dcdc_mode = DCDC_MODE_PFM_FORCED;
    runp.vdd_ioflex_3V3 = IOFLEX_LEVEL_1V8;

    ret = SERVICES_set_run_cfg(se_services_s_handle, &runp, &service_response);
    if (ret != 0) {
        SERVICES_ret(ret);
    }
    if (service_response != 0) {
        SERVICES_response(service_response);
    }

    off_profile_t offp = {0};
    ret = SERVICES_get_off_cfg(se_services_s_handle, &offp, &service_response);
    if (ret != 0) {
        SERVICES_ret(ret);
    }
    if (service_response != 0) {
        SERVICES_response(service_response);
    }

    offp.aon_clk_src = CLK_SRC_LFXO;
    offp.power_domains = 0;
    offp.stby_clk_freq = SCALED_FREQ_RC_STDBY_1_2_MHZ;
    offp.memory_blocks  = MRAM_MASK | BACKUP4K_MASK;
    offp.memory_blocks |= SERAM_1_MASK | SERAM_2_MASK | SERAM_3_MASK | SERAM_4_MASK;
    offp.memory_blocks |= SRAM4_1_MASK | SRAM4_2_MASK | SRAM4_3_MASK | SRAM4_4_MASK;
    offp.memory_blocks |= SRAM5_1_MASK | SRAM5_2_MASK | SRAM5_3_MASK | SRAM5_4_MASK | SRAM5_5_MASK;
    offp.vdd_ioflex_3V3 = IOFLEX_LEVEL_1V8;
    offp.wakeup_events = WE_LPTIMER0;
    offp.ewic_cfg = EWIC_VBAT_TIMER0;
    offp.vtor_address = SCB->VTOR;

    ret = SERVICES_set_off_cfg(se_services_s_handle, &offp, &service_response);
    if (ret != 0) {
        SERVICES_ret(ret);
    }
    if (service_response != 0) {
        SERVICES_response(service_response);
    }

    *(volatile uint32_t *)(0x1A60A008UL) = 0x100;
    *(volatile uint32_t *)(0x1A60A020UL) = SCB->VTOR;
    *(volatile uint32_t *)(0x1A60A024UL) = SCB->VTOR;
    *(volatile uint32_t *)(0x1A60A030UL) |= (1UL << 30);

    ret = SERVICES_power_ldo_voltage_control(se_services_s_handle, 3, 8, &service_response);
    if (ret != 0) {
        SERVICES_ret(ret);
    }
    if (service_response != 0) {
        SERVICES_response(service_response);
    }

    /* Clear the Cycle Counter */
    int32_t number_in = 0;
    bk_ram_wr(&number_in, BKRAM_INDEX_CYCLECNT);

    printf("Application overall duty cycle in milliseconds\r\n");
    printf("> ");
    number_in = get_int_input();

    LPTIMER_ll_Initialize(0);
    LPTIMER_ll_InterruptClear(0);
    LPTIMER_ll_Set_Count_Value(0, ((number_in * LPTIMER_TICK_RATE)/1000) - 1);

    printf("\r\nTime spent running while(1)\r\n");
    printf("> ");
    number_in = get_int_input();
    bk_ram_wr(&number_in, BKRAM_INDEX_WHILE1);

    printf("\r\nNumber of Inferences on Ethos NPU\r\n");
    printf("> ");
    number_in = get_int_input();
    bk_ram_wr(&number_in, BKRAM_INDEX_ETHOSU);

    printf("\r\nStarting Power cycle demo\r\n");
    delay_ms_wfi(100);

    LPTIMER_ll_Start(0);
}

static void enter_stop()
{
    *(volatile uint32_t *)(0x1A60F000UL) = 1;
    while(1) pm_core_enter_deep_sleep();
}

static void boot_from_stop()
{
    uint32_t bk_data;
    bk_ram_rd(&bk_data, BKRAM_INDEX_CYCLECNT);
    bk_data++;
    bk_ram_wr(&bk_data, BKRAM_INDEX_CYCLECNT);

    LPTIMER_ll_InterruptClear(0);
}

static void execute_while1()
{
    uint32_t bk_data;
    bk_ram_rd(&bk_data, BKRAM_INDEX_WHILE1);
    delay_ms(bk_data);
}

static void execute_ethos()
{
    uint32_t bk_data;
    bk_ram_rd(&bk_data, BKRAM_INDEX_CYCLECNT);
    if ((bk_data % 10) == 0) {
        refclk_cntr_init();
        OSC_xtal_start(true, true);
        PLL_clkpll_start(true);
        *(volatile uint32_t *)(0x1A602008) = (1U << 20);
        bk_ram_rd(&bk_data, BKRAM_INDEX_ETHOSU);
        npuTestStart(bk_data, 3);
        *(volatile uint32_t *)(0x1A602008) = 0;
        PLL_clkpll_stop();
        OSC_xtal_stop();
    }
}

void uart_init()
{
#if defined(RTE_Compiler_IO_STDIN_User)
    stdin_init();
    pinconf_set(PORT_2, PIN_0, PINMUX_ALTERNATE_FUNCTION_2, 1);
#endif
#if defined(RTE_Compiler_IO_STDOUT_User)
    stdout_init();
    pinconf_set(PORT_9, PIN_1, PINMUX_ALTERNATE_FUNCTION_1, 0);
#endif
}

void main (void)
{
    SystemREFClock = 76800000UL;
    SystemCoreClock = 76800000UL;
    SysTick_Config(SystemCoreClock/TICKS_PER_SECOND);
    
    *(volatile uint32_t *)(0x49005000UL) = 0;     /* set GPIOx pin 0 low */
    uint32_t bk_data;
    bk_ram_rd(&bk_data, BKRAM_INDEX_FIRSTBOOT);
    if (bk_data != 0xB007ED) {
        uart_init();
        boot_from_por();

        bk_data = 0xB007ED;
        bk_ram_wr(&bk_data, BKRAM_INDEX_FIRSTBOOT);
        enter_stop();
    }
    else {
        boot_from_stop();
        execute_while1();
        execute_ethos();
        enter_stop();
    }
}
