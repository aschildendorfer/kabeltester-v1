#include <stdio.h>
#include "pico/stdlib.h"
#include "app_config.h"
#include "core0_ui.h"

void core0_ui_init(void) {
    stdio_init_all();
    sleep_ms(1500);
    setvbuf(stdout, NULL, _IONBF, 0);

    gpio_init(CORE0_LED_PIN);
    gpio_set_dir(CORE0_LED_PIN, GPIO_OUT);
    gpio_put(CORE0_LED_PIN, 0);
}

void core0_ui_print_boot(void) {
    printf("\r\n[boot] step0 multicore test\r\n");
    printf("[core0] LED blink on GPIO%u\r\n", CORE0_LED_PIN);
    printf("[core1] NeoPixel blink on GPIO%u\r\n", WS2812_PIN);
}

uint32_t core0_ui_step(uint32_t core1_last_cnt) {
    static uint32_t core0_cnt = 0;

    gpio_put(CORE0_LED_PIN, 1);
    sleep_ms(CORE0_BLINK_MS);
    gpio_put(CORE0_LED_PIN, 0);
    sleep_ms(CORE0_BLINK_MS);

    core0_cnt++;
    printf("[hb] core0=%lu core1=%lu\r\n", (unsigned long)core0_cnt, (unsigned long)core1_last_cnt);
    return core0_cnt;
}
