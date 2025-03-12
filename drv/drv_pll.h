#include <stdbool.h>

void OSC_xtal_start(bool faststart, bool boost);
void OSC_rc_to_xtal();
void OSC_initialize();

void OSC_xtal_stop();
void OSC_xtal_to_rc();
void OSC_uninitialize();

void PLL_clkpll_start(bool faststart);
void PLL_clkpll_stop();
void PLL_initialize();
void PLL_uninitialize();