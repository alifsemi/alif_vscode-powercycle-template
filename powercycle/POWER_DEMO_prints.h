#include <stdint.h>

#define EWIC_VBAT_TIMER0             (0x00020)   // bit5
#define EWIC_VBAT_TIMER1             (0x00040)   // bit6
#define EWIC_VBAT_GPIO0              (0x00200)   // bit9
#define EWIC_VBAT_GPIO1              (0x00400)   // bit10

int32_t get_int_input();
