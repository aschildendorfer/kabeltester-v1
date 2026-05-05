#ifndef CORE0_UI_H
#define CORE0_UI_H

#include <stdint.h>

void core0_ui_init(void);
void core0_ui_print_boot(void);
uint32_t core0_ui_step(uint32_t core1_last_cnt);

#endif
