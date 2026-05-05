#include <stdio.h>
#include "pico/multicore.h"
#include <stdint.h>
#include "app_config.h"
#include "core0_ui.h"
#include "core1_engine.h"

int main(void) {
    uint32_t msg = 0;
    uint32_t core1_last_cnt = 0;

    core0_ui_init();
    core0_ui_print_boot();

    multicore_launch_core1(core1_engine_main);

    while ((msg = multicore_fifo_pop_blocking()) != CORE1_READY_TOKEN) {
    }
    printf("[sync] both cores go\r\n");
    multicore_fifo_push_blocking(CORE1_GO_TOKEN);

    while (true) {
        while (multicore_fifo_rvalid()) {
            msg = multicore_fifo_pop_blocking();
            if (msg >= CORE1_HEARTBEAT_BASE) {
                core1_last_cnt = msg - CORE1_HEARTBEAT_BASE;
            }
        }
        core0_ui_step(core1_last_cnt);
    }
}
