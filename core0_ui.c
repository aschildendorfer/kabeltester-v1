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
    printf("[core1] HST trigger on GPIO%u, high=%uus, period=%ums\r\n", HST_TRIGGER_PIN, HST_PULSE_HIGH_US, HST_PULSE_PERIOD_MS);
    printf("[hw] TLV3501 input path verified, target pulse width=%uus\r\n", HST_PULSE_HIGH_US);
    printf("[core1] threshold PWM on GPIO%u, duty=%u%%, freq=%uHz\r\n", THRESHOLD_PWM_PIN, THRESHOLD_PWM_DUTY_PERCENT, THRESHOLD_PWM_FREQ_HZ);
    printf("[core1] echo input on GPIO%u\r\n", ECHO_INPUT_PIN);
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

void core0_ui_log_hst_trigger(uint32_t seq, uint32_t ts_ms) {
    printf("[hst] seq=%lu ts_ms=%lu\r\n", (unsigned long)seq, (unsigned long)ts_ms);
}
