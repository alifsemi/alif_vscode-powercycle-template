#include <stdio.h>
#include <string.h>
#include <stdint.h>

void app_init (void)
{
    *(volatile uint32_t *)(0x49005000UL) = 1;     /* set GPIOx pin 0 high */
    *(volatile uint32_t *)(0x49005004UL) = 0xFF;  /* set all GPIOx pins as output */
}