#include <stdint.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "ws2812.pio.h"
#include "app_config.h"
#include "core1_engine.h"

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

void core1_engine_main(void) {
    PIO pio = pio0;
    const uint sm = 0;
    const uint offset = pio_add_program(pio, &ws2812_program);
    uint32_t cnt = 0;

    ws2812_program_init(pio, sm, offset, WS2812_PIN, WS2812_FREQ_HZ, WS2812_IS_RGBW);

    multicore_fifo_push_blocking(CORE1_READY_TOKEN);
    while (multicore_fifo_pop_blocking() != CORE1_GO_TOKEN) {
    }

    while (true) {
        put_pixel(pio, sm, urgb_u32(0x00, 0x10, 0x00));
        sleep_ms(CORE1_BLINK_MS);
        put_pixel(pio, sm, urgb_u32(0x00, 0x00, 0x00));
        sleep_ms(CORE1_BLINK_MS);

        cnt++;
        if (multicore_fifo_wready()) {
            multicore_fifo_push_blocking(CORE1_HEARTBEAT_BASE + cnt);
        }
    }
}
