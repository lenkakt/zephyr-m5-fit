#include "display_direct.h"
#include "colors.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/dt-bindings/pwm/pwm.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(display_direct, LOG_LEVEL_ERR);

#define LCD_PWM_FREQ_HZ 1000U
#define LCD_BG_NODE     DT_ALIAS(lcd_bg)

static const struct device *s_display;

/* Single row buffer — 320 pixels x 2 bytes = 640 bytes total. */
static uint16_t s_row[SCREEN_WIDTH];

/* ── internal helper ──────────────────────────────────────────────── */
static void write_span(int x, int y, int len, const uint16_t *pixels)
{
    if (!s_display || len <= 0) return;

    struct display_buffer_descriptor d = {
        .width    = len,
        .height   = 1,
        .pitch    = len,
        .buf_size = len * sizeof(uint16_t),
    };
    display_write(s_display, x, y, &d, pixels);
}

/* ── public API ───────────────────────────────────────────────────── */

void display_write_row_span(int x, int y, int len, const uint16_t *pixels)
{
    write_span(x, y, len, pixels);
}

void display_fill_rect(int x, int y, int w, int h, uint16_t color)
{
    if (!s_display || w <= 0 || h <= 0) return;

    /* Clamp */
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SCREEN_WIDTH)  w = SCREEN_WIDTH  - x;
    if (y + h > SCREEN_HEIGHT) h = SCREEN_HEIGHT - y;
    if (w <= 0 || h <= 0) return;

    uint16_t c = sys_cpu_to_be16(color);
    for (int i = 0; i < w; i++) s_row[i] = c;

    for (int row = y; row < y + h; row++) {
        write_span(x, row, w, s_row);
    }
}

void screen_clear(void)
{
    display_fill_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BLACK);
}

/*
 * Same backlight story as 05-display: the ili9342c driver only pushes
 * pixel data, the backlight is a separate PWM-driven LED that this code
 * doesn't know or care about unless told.
 */
void display_direct_init(void)
{
    static const struct pwm_dt_spec lcd_pwm = PWM_DT_SPEC_GET(LCD_BG_NODE);
    if (!pwm_is_ready_dt(&lcd_pwm)) { LOG_ERR("PWM not ready"); return; }

    uint32_t period_ns = 1000000000U / LCD_PWM_FREQ_HZ;
    if (pwm_set_dt(&lcd_pwm, period_ns, period_ns / 2U) < 0) {
        LOG_ERR("PWM set failed"); return;
    }

    const struct device *dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (!device_is_ready(dev)) { LOG_ERR("Display not ready"); return; }

    display_blanking_off(dev);
    s_display = dev;

    screen_clear();
}
