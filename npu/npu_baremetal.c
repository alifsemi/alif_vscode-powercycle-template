#include <RTE_Components.h>
#include CMSIS_device_header
#include <drv_counter.h>
#include <pm.h>

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define ENABLE_NPU_KWS          1

#define TYPICAL_MEMORYMAP_SIZE_IN_BYTES (TYPICAL_MEMORYMAP_SIZE * 4)
#define TYPICAL_REFERENCE_SIZE_IN_BYTES (TYPICAL_REFERENCE_SIZE * 4)
extern const uint32_t TYPICAL_REFERENCE_SIZE;
extern const uint32_t TYPICAL_MEMORYMAP_SIZE;
extern const uint32_t TYPICAL_MEMORYMAP_BASEP0_offset;
extern const uint32_t typical_mem_map[];
extern const uint32_t typical_reference[];

#define NPU_BASE_ADDRESS                LOCAL_NPU_BASE

#ifdef M55_HE
#define M55_CFG_BASE_ADDRESS            M55HE_CFG_BASE
#define NPU_IRQn                        NPU_HE_IRQ_IRQn
#elif defined M55_HP
#define M55_CFG_BASE_ADDRESS            M55HP_CFG_BASE
#define NPU_IRQn                        NPU_HP_IRQ_IRQn
#else
#error "NPU only usable with HE or HP core"
#endif

#if ENABLE_NPU_KWS
extern const void *get_kws_model(void);
extern size_t get_kws_model_len(void);
extern size_t get_kws_qsize(void);
extern size_t get_kws_qbase_offset(void);
extern size_t get_kws_basep0_offset(void);
extern const uint8_t kws_reference_input[];
extern const uint8_t kws_reference_output[];
#endif

#define HW_REG32(base, offset) *((volatile uint32_t *)(base + offset))

static const void *npu_readonly_addr;
static const void *reference_data_addr;
static uint32_t reference_data_size;
static uint32_t npu_workspace_addr[32768] __attribute__((aligned(16), section(".bss.noinit")));
static uint32_t npu_workspace_size;
static uint32_t npu_output_offset;
static uint32_t npu_register_QBASE_offset;
static uint32_t npu_register_BASEP0_offset;
static uint32_t npu_command_length;

static volatile int continue_npu = 0;
static volatile int npu_running = 0;
static volatile uint32_t max_inferences = 0;
static volatile uint32_t npu_inferences = 0;

void LOCAL_NPU_IRQHandler(void)
{
    // clear irq with bit 1
    // (take care not to re-poke the run bit - this is how the Ethos driver does it)
    HW_REG32(NPU_BASE_ADDRESS, 0x08) = (HW_REG32(NPU_BASE_ADDRESS, 0x08) & 0xC) | 0x2;

    npu_inferences++;

    if (continue_npu) {
        // restart npu by settings Command Register bit 0 to 1 (Write 1 to transition the NPU to running state.)
        HW_REG32(NPU_BASE_ADDRESS, 0x08) = (HW_REG32(NPU_BASE_ADDRESS, 0x08) & 0xC) | 0x1;
    } else {
        HW_REG32(NPU_BASE_ADDRESS, 0x08)  = 0x0000000C; // Set default value, enables off for clocks and power.

        uint32_t data = HW_REG32(NPU_BASE_ADDRESS, 0x04); // Channel0 Status Registers
        if ((data & 0x20) || (data & 0x10)) {
            // NPU Command stream end reached or Command-stream parsing error detected.
            npu_running = 0;
        }
    }
}

static int32_t set_global_attributes(uint32_t test_case)
{
    const void *model_data_addr;
    size_t model_data_size;

#if ENABLE_NPU_KWS
    if (test_case > 0) {
        npu_workspace_size = 128*1024;
        reference_data_size = 12;
        reference_data_addr = kws_reference_output;
        model_data_addr = get_kws_model();
        model_data_size = get_kws_model_len();
        npu_register_QBASE_offset = get_kws_qbase_offset();
        npu_command_length = get_kws_qsize();
        npu_register_BASEP0_offset = get_kws_basep0_offset();
        npu_output_offset = 0xd0;
        npu_readonly_addr = model_data_addr;
    }
    else
#endif
    {
        npu_workspace_size = TYPICAL_REFERENCE_SIZE_IN_BYTES;
        reference_data_size = TYPICAL_REFERENCE_SIZE_IN_BYTES;
        reference_data_addr = typical_reference;
        model_data_addr = typical_mem_map;
        model_data_size = TYPICAL_MEMORYMAP_SIZE_IN_BYTES;
        npu_register_QBASE_offset = 0x1cb0;
        npu_command_length = 0xec;
        npu_register_BASEP0_offset = TYPICAL_MEMORYMAP_BASEP0_offset;
        npu_output_offset = 0;
        npu_readonly_addr = model_data_addr;
    }

    RTSS_CleanInvalidateDCache_by_Addr((void *) npu_readonly_addr, model_data_size);

    return 0;
}

static void fill_input_data(uint32_t test_case)
{
#if ENABLE_NPU_KWS
    if (test_case > 0) {
        memcpy((uint8_t *) npu_workspace_addr + 0x113a0, kws_reference_input, 49 * 10);
    }
#endif
}

static void stop_npu(void)
{
    continue_npu = 0;

    while (npu_running) __WFE();
    HW_REG32(NPU_BASE_ADDRESS, 0x10) = 0x00000000; // Base address of Command-queue. Set to default.
    HW_REG32(NPU_BASE_ADDRESS, 0x1C) = 0x00000000; // AXI configuration for the command stream in the range 0-3. Set to default.
    HW_REG32(NPU_BASE_ADDRESS, 0x14) = 0x00000000; // Address extension bits[47:32] for queue base. Set to default.
    HW_REG32(NPU_BASE_ADDRESS, 0x20) = 0x00000000; // Size of command stream in bytes. Set to default.
    HW_REG32(NPU_BASE_ADDRESS, 0x3C) = 0x00000000; // Base pointer configuration. Bits[2*k+1:2*k] give the memory type for REGION[k]. Set to default.
    HW_REG32(NPU_BASE_ADDRESS, 0x40) = 0x00000000; // AXI limits for port 0 counter 0. Set to default.
    HW_REG32(NPU_BASE_ADDRESS, 0x44) = 0x00000000; // AXI limits for port 0 counter 1. Set to default.
    HW_REG32(NPU_BASE_ADDRESS, 0x48) = 0x00000000; // AXI limits for port 1 counter 2. Set to default.
    HW_REG32(NPU_BASE_ADDRESS, 0x4C) = 0x00000000; // AXI limits for port 1 counter 3. Set to default.

    // Disable NPU interrupt
    NVIC_DisableIRQ(NPU_IRQn);
    NVIC_ClearPendingIRQ(NPU_IRQn);

    // Reset NPU
    HW_REG32(NPU_BASE_ADDRESS, 0x0c) = 0x00000000;

    // Wait until resetted
    uint32_t data = 0;
    do {
        data = HW_REG32(NPU_BASE_ADDRESS, 0x04); // Channel0 Status Registers
    } while (data != 0x00000000);

    // Set default value, enables off for clocks and power.
    HW_REG32(NPU_BASE_ADDRESS, 0x08)  = 0x0000000C;
}

static int32_t start_npu(uint32_t test_case)
{
    uint64_t elapsed_time;
    uint32_t data = 0;
    int32_t ret = 0;

    ret = set_global_attributes(test_case);
    if (ret) {
        return ret;
    }

    // Clock gating enable to Ethos-U55
    HW_REG32(M55_CFG_BASE_ADDRESS, 0x10) |= 0x01;

    fill_input_data(test_case);

    RTSS_CleanInvalidateDCache_by_Addr(npu_workspace_addr, npu_workspace_size);

    // Enable NPU interrupt
    NVIC_ClearPendingIRQ(NPU_IRQn);
    NVIC_EnableIRQ(NPU_IRQn);

    // Reset NPU
    HW_REG32(NPU_BASE_ADDRESS, 0x0c) = 0x00000000;

    // Wait until resetted
    do {
        data = HW_REG32(NPU_BASE_ADDRESS, 0x04); // Channel0 Status Registers
    } while (data != 0x00000000);

    HW_REG32(NPU_BASE_ADDRESS, 0x1C) = 0x00000000;
    HW_REG32(NPU_BASE_ADDRESS, 0x3C) = 0x00000000;
    HW_REG32(NPU_BASE_ADDRESS, 0x40) = 0x0f1f0000;
    HW_REG32(NPU_BASE_ADDRESS, 0x44) = 0x0f1f0000;
    HW_REG32(NPU_BASE_ADDRESS, 0x48) = 0xff1f0000;
    HW_REG32(NPU_BASE_ADDRESS, 0x4C) = 0xff1f0000;
    HW_REG32(NPU_BASE_ADDRESS, 0x84) = 0x00000000;
    HW_REG32(NPU_BASE_ADDRESS, 0x8C) = 0x00000000;
    HW_REG32(NPU_BASE_ADDRESS, 0x94) = 0x00000000;
    HW_REG32(NPU_BASE_ADDRESS, 0x9C) = 0x00000000;
    HW_REG32(NPU_BASE_ADDRESS, 0xA4) = 0x00000000;
    HW_REG32(NPU_BASE_ADDRESS, 0x14) = 0x00000000;
    HW_REG32(NPU_BASE_ADDRESS, 0x10) = LocalToGlobal(npu_readonly_addr) + npu_register_QBASE_offset; // Address for Command-queue.
    HW_REG32(NPU_BASE_ADDRESS, 0x80) = LocalToGlobal(npu_readonly_addr) + npu_register_BASEP0_offset; // Region 0 = Weights
    HW_REG32(NPU_BASE_ADDRESS, 0x20) = npu_command_length; // Size of command stream in bytes
    if (test_case > 0) {
        HW_REG32(NPU_BASE_ADDRESS, 0x88) = LocalToGlobal(npu_workspace_addr); // Region 1 = Arena
        HW_REG32(NPU_BASE_ADDRESS, 0x90) = LocalToGlobal(npu_workspace_addr); // Region 2 = Scratch
        HW_REG32(NPU_BASE_ADDRESS, 0x98) = LocalToGlobal(npu_workspace_addr) + 0x113a0; // Region 3 = Input
        HW_REG32(NPU_BASE_ADDRESS, 0xA0) = LocalToGlobal(npu_workspace_addr) + 0xd0; // Region 4 = Output
    }
    else {
        HW_REG32(NPU_BASE_ADDRESS, 0x88) = 0x00000000;
        HW_REG32(NPU_BASE_ADDRESS, 0x90) = LocalToGlobal(npu_readonly_addr) + 0x1da0; // Region 2 = Input
        HW_REG32(NPU_BASE_ADDRESS, 0x98) = LocalToGlobal(npu_workspace_addr); // Region 3 = Output
    }

    // start NPU again and do interferences until stop command
    continue_npu = 1;
    npu_running = 1;
    npu_inferences = 0;
    HW_REG32(NPU_BASE_ADDRESS, 0x08) = 0x00000001;

    while (npu_inferences < max_inferences) __WFE();
    stop_npu();

    return ret;
}

void npuTestStart(uint32_t test_count, uint32_t test_case)
{
    max_inferences = test_count < 1 ? 0 : test_count - 1;
    printf("NPU: start test\n");
    int32_t ret = start_npu(test_case);
    
    if (ret != 0) {
        printf("NPU: start failed: %" PRIi32 "\r\n", ret);
        return;
    }
}
