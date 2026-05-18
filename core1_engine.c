#include <stdint.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include "app_config.h"
#if HST_TRIGGER_USE_HSTX
#include "hardware/regs/hstx_ctrl.h"
#include "hardware/structs/hstx_ctrl.h"
#include "hardware/structs/hstx_fifo.h"
#endif
#include "ws2812.pio.h"
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

static void hst_trigger_pwm_init(void) {
    const float period_hz = 1000.0f / (float)HST_PULSE_PERIOD_MS;
    const uint16_t top = 65535u;
    const float clk_hz = (float)clock_get_hz(clk_sys);
    float div = clk_hz / (period_hz * (float)(top + 1u));
    uint16_t level = (uint16_t)((((uint64_t)(top + 1u)) * HST_PULSE_HIGH_US) / ((uint64_t)HST_PULSE_PERIOD_MS * 1000u));

    if (div < 1.0f) {
        div = 1.0f;
    }
    if (div > 255.9375f) {
        div = 255.9375f;
    }
    if (level == 0) {
        level = 1;
    }

    gpio_set_function(HST_TRIGGER_PIN, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(HST_TRIGGER_PIN);
    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_wrap(&cfg, top);
    pwm_config_set_clkdiv(&cfg, div);
    pwm_init(slice, &cfg, false);
    pwm_set_gpio_level(HST_TRIGGER_PIN, level);
    pwm_set_enabled(slice, true);
}

static void hst_trigger_init(void) {
    // Apply the strongest pad settings on the trigger pin to maximize edge quality.
    gpio_disable_pulls(HST_TRIGGER_PIN);
    gpio_set_drive_strength(HST_TRIGGER_PIN, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_slew_rate(HST_TRIGGER_PIN, GPIO_SLEW_RATE_FAST);

#if HST_TRIGGER_USE_HSTX && (HST_TRIGGER_PIN >= 12) && (HST_TRIGGER_PIN <= 19)
    const uint lane = HST_TRIGGER_PIN & 0x7u;

    gpio_set_function(HST_TRIGGER_PIN, GPIO_FUNC_HSTX);

    // Halt HSTX while configuring lane routing.
    hstx_ctrl_hw->csr &= ~HSTX_CTRL_CSR_EN_BITS;
    hstx_ctrl_hw->bit[lane] =
        (0u << HSTX_CTRL_BIT0_SEL_N_LSB) |
        (0u << HSTX_CTRL_BIT0_SEL_P_LSB);
    hstx_ctrl_hw->csr |= HSTX_CTRL_CSR_EN_BITS;
#else
    hst_trigger_pwm_init();
#endif
}

static inline void hst_trigger_fire(void) {
#if HST_TRIGGER_USE_HSTX && (HST_TRIGGER_PIN >= 12) && (HST_TRIGGER_PIN <= 19)
    hstx_fifo_hw->fifo = HST_PULSE_PATTERN;
#endif
}

static void echo_input_init(void) {
    gpio_init(ECHO_INPUT_PIN);
    gpio_set_dir(ECHO_INPUT_PIN, GPIO_IN);
    gpio_disable_pulls(ECHO_INPUT_PIN); // No pull-up/down
    gpio_set_slew_rate(ECHO_INPUT_PIN, GPIO_SLEW_RATE_SLOW); // Default (no edge shaping)
    gpio_set_input_enabled(ECHO_INPUT_PIN, true); // Ensure input buffer is on
    gpio_set_inover(ECHO_INPUT_PIN, GPIO_OVERRIDE_NORMAL); // No override/filter
}

static void threshold_pwm_init(void) {
    const uint32_t clk_hz = clock_get_hz(clk_sys);
    uint32_t div16 = 16u;
    uint32_t top_plus_1 = 0u;
    uint32_t duty_percent = THRESHOLD_PWM_DUTY_PERCENT;

    if (THRESHOLD_PWM_FREQ_HZ == 0u) {
        return;
    }

    // Find the smallest divider that yields a valid 16-bit wrap value.
    for (div16 = 16u; div16 <= 4095u; div16++) {
        top_plus_1 = (uint32_t)((((uint64_t)clk_hz * 16u) + ((uint64_t)THRESHOLD_PWM_FREQ_HZ * div16) / 2u) /
                                ((uint64_t)THRESHOLD_PWM_FREQ_HZ * div16));
        if (top_plus_1 >= 2u && top_plus_1 <= 65536u) {
            break;
        }
    }

    if (div16 > 4095u) {
        div16 = 4095u;
        top_plus_1 = 65536u;
    }

    if (duty_percent > 100u) {
        duty_percent = 100u;
    }

    const uint16_t top = (uint16_t)(top_plus_1 - 1u);
    const uint16_t level = (uint16_t)(((uint64_t)top_plus_1 * duty_percent) / 100u);
    const float div = (float)div16 / 16.0f;

    gpio_set_function(THRESHOLD_PWM_PIN, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(THRESHOLD_PWM_PIN);
    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_wrap(&cfg, top);
    pwm_config_set_clkdiv(&cfg, div);
    pwm_init(slice, &cfg, false);
    pwm_set_gpio_level(THRESHOLD_PWM_PIN, level);
    pwm_set_enabled(slice, true);
}

void core1_engine_main(void) {
    PIO pio = pio0;
    const uint sm = 0;
    const uint offset = pio_add_program(pio, &ws2812_program);
    uint32_t cnt = 0;
    uint32_t trigger_seq = 0;
    uint32_t next_pixel_toggle_ms = to_ms_since_boot(get_absolute_time()) + CORE1_BLINK_MS;
    const uint32_t hst_period_us = HST_PULSE_PERIOD_MS * 1000u;
    uint32_t next_hst_log_us = time_us_32() + hst_period_us;
    bool pixel_on = false;

    ws2812_program_init(pio, sm, offset, WS2812_PIN, WS2812_FREQ_HZ, WS2812_IS_RGBW);
    hst_trigger_init();
    threshold_pwm_init();
    echo_input_init();

    multicore_fifo_push_blocking(CORE1_READY_TOKEN);
    while (multicore_fifo_pop_blocking() != CORE1_GO_TOKEN) {
    }

    while (true) {
        uint32_t now_ms = to_ms_since_boot(get_absolute_time());
        uint32_t now_us = time_us_32();

        if ((int32_t)(now_ms - next_pixel_toggle_ms) >= 0) {
            pixel_on = !pixel_on;
            put_pixel(pio, sm, pixel_on ? urgb_u32(0x00, 0x10, 0x00) : urgb_u32(0x00, 0x00, 0x00));
            next_pixel_toggle_ms += CORE1_BLINK_MS;

            if (!pixel_on && multicore_fifo_wready()) {
                cnt++;
                multicore_fifo_push_blocking(CORE1_HEARTBEAT_BASE + cnt);
            }
        }

        if ((int32_t)(now_us - next_hst_log_us) >= 0) {
            do {
                next_hst_log_us += hst_period_us;
                hst_trigger_fire();
                trigger_seq++;
                if (multicore_fifo_wready()) {
                    multicore_fifo_push_blocking(CORE1_TRIGGER_SEQ_BASE | (trigger_seq & CORE_MSG_VALUE_MASK));
                }
                if (multicore_fifo_wready()) {
                    uint32_t ts_ms = to_ms_since_boot(get_absolute_time());
                    multicore_fifo_push_blocking(CORE1_TRIGGER_TS_BASE | (ts_ms & CORE_MSG_VALUE_MASK));
                }
            } while ((int32_t)(time_us_32() - next_hst_log_us) >= 0);
        }

        sleep_us(200);
    }
}
