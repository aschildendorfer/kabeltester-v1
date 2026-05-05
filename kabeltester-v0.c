#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "ws2812.pio.h"

#define CORE0_LED_PIN 7
#define WS2812_PIN 21
#define WS2812_IS_RGBW false
#define CORE0_BLINK_MS 250
#define CORE1_BLINK_MS 300
#define WS2812_FREQ_HZ 800000

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 8) | ((uint32_t)g << 16) | (uint32_t)b;
}

static inline void put_pixel(PIO pio, uint sm, uint32_t pixel_grb) {
    pio_sm_put_blocking(pio, sm, pixel_grb << 8u);
}

static void ws2812_program_init(PIO pio, uint sm, uint offset, uint pin, float freq, bool rgbw) {
    pio_sm_config c = ws2812_program_get_default_config(offset);

    sm_config_set_sideset_pins(&c, pin);
    sm_config_set_out_shift(&c, false, true, rgbw ? 32 : 24);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    float div = (float)clock_get_hz(clk_sys) / (freq * (float)(ws2812_T1 + ws2812_T2 + ws2812_T3));
    sm_config_set_clkdiv(&c, div);

    pio_gpio_init(pio, pin);
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}

static void core1_main(void) {
    PIO pio = pio0;
    const uint sm = 0;
    const uint offset = pio_add_program(pio, &ws2812_program);
    uint32_t cnt = 0;

    ws2812_program_init(pio, sm, offset, WS2812_PIN, WS2812_FREQ_HZ, WS2812_IS_RGBW);

    // Signal ready to Core0 and wait for go
    multicore_fifo_push_blocking(0x1);
    multicore_fifo_pop_blocking();

    while (true) {
        put_pixel(pio, sm, urgb_u32(0x00, 0x10, 0x00));
        sleep_ms(CORE1_BLINK_MS);
        put_pixel(pio, sm, urgb_u32(0x00, 0x00, 0x00));
        sleep_ms(CORE1_BLINK_MS);

        cnt++;
        if (multicore_fifo_wready()) {
            multicore_fifo_push_blocking(cnt);
        }
    }
}

int main(void) {
    stdio_init_all();
    sleep_ms(1500);
    setvbuf(stdout, NULL, _IONBF, 0);

    gpio_init(CORE0_LED_PIN);
    gpio_set_dir(CORE0_LED_PIN, GPIO_OUT);
    gpio_put(CORE0_LED_PIN, 0);

    printf("\r\n[boot] step0 multicore test\r\n");
    printf("[core0] LED blink on GPIO%u\r\n", CORE0_LED_PIN);
    printf("[core1] NeoPixel blink on GPIO%u\r\n", WS2812_PIN);

    multicore_launch_core1(core1_main);

    // Wait for Core1 ready, then release it
    multicore_fifo_pop_blocking();
    printf("[sync] both cores go\r\n");
    multicore_fifo_push_blocking(0x1);

    uint32_t core0_cnt = 0;
    uint32_t core1_last_cnt = 0;

    while (true) {
        gpio_put(CORE0_LED_PIN, 1);
        sleep_ms(CORE0_BLINK_MS);
        gpio_put(CORE0_LED_PIN, 0);
        sleep_ms(CORE0_BLINK_MS);

        core0_cnt++;
        while (multicore_fifo_rvalid()) {
            core1_last_cnt = multicore_fifo_pop_blocking();
        }

        printf("[hb] core0=%lu core1=%lu\r\n", (unsigned long)core0_cnt, (unsigned long)core1_last_cnt);
    }
}
