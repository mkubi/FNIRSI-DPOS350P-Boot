#include <stddef.h>
#include <stdint.h>
#include "gt911.h"
#include "gpio.h"
#include "timer.h"

#define GT911_ADDR_WRITE       0xBAU
#define GT911_ADDR_READ        0xBBU
#define GT911_I2C_ADDRESS      0x5DU

#define GT911_REG_PRODUCT_ID   0x8140U
#define GT911_REG_STATUS       0x814EU
#define GT911_REG_POINT1       0x8150U

/*
 * DPOS350P-specific GT911 reset/address-selection timing.
 *
 * A long setup interval between driving INT high and releasing RESET
 * is required for reliable coordinate reporting on this board.
 *
 * Shorter intervals allowed Product ID reads but did not produce
 * DATA_READY touch events.
 */
#define GT911_RESET_LOW_MS    5U
#define GT911_INT_SETUP_MS   100U
#define GT911_POST_RESET_MS   5U


/* FNIRSI DPOS350P PIN routing. */
#define GT_INT_PORT GPIO_PORT_E
#define GT_INT_PIN  22U
#define GT_RST_PORT GPIO_PORT_E
#define GT_RST_PIN  24U
#define GT_SDA_PORT GPIO_PORT_G
#define GT_SDA_PIN  6U
#define GT_SCL_PORT GPIO_PORT_G
#define GT_SCL_PIN  7U

/* Ignore small coordinate jitter before declaring a drag gesture. */
#define GT911_DRAG_THRESHOLD 4U

typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t start_x;
    uint16_t start_y;
    uint8_t pressed;
    uint8_t dragging;
} gt911_runtime_state_t;

static gt911_runtime_state_t gt911_runtime_state;

static inline void i2c_delay(void)
{
    volatile unsigned i;

    for (i = 0U; i < 32U; i++)
        __asm__ volatile ("nop");
}

static void scl(int high)
{
    gpio_write(GT_SCL_PORT, GT_SCL_PIN, high);
    i2c_delay();
}

static void sda_output(void)
{
    gpio_set_output(GT_SDA_PORT, GT_SDA_PIN);
}

static void sda_input(void)
{
    gpio_set_input(GT_SDA_PORT, GT_SDA_PIN);
}

static void sda(int high)
{
    gpio_write(GT_SDA_PORT, GT_SDA_PIN, high);
    i2c_delay();
}

static void i2c_start(void)
{
    sda_output();
    sda(1);
    scl(1);
    sda(0);
    scl(0);
}

static void i2c_stop(void)
{
    sda_output();
    sda(0);
    scl(1);
    sda(1);
}

static int i2c_write_byte(uint8_t value)
{
    unsigned bit;
    unsigned nack;

    sda_output();
    for (bit = 0U; bit < 8U; bit++) {
        sda((value & 0x80U) != 0U);
        scl(1);
        scl(0);
        value <<= 1;
    }

    sda_input();
    scl(1);
    nack = (unsigned)gpio_read(GT_SDA_PORT, GT_SDA_PIN);
    scl(0);
    sda_output();
    return nack == 0U ? 0 : -1;
}

static uint8_t i2c_read_byte(int acknowledge)
{
    unsigned bit;
    uint8_t value = 0U;

    sda_input();
    for (bit = 0U; bit < 8U; bit++) {
        value <<= 1;
        scl(1);
        if (gpio_read(GT_SDA_PORT, GT_SDA_PIN))
            value |= 1U;
        scl(0);
    }

    sda_output();
    sda(acknowledge ? 0 : 1);
    scl(1);
    scl(0);
    sda(1);
    return value;
}

static int gt911_write_reg(uint16_t reg, const uint8_t *data,
                           unsigned length)
{
    unsigned i;

    i2c_start();
    if (i2c_write_byte(GT911_ADDR_WRITE) ||
        i2c_write_byte((uint8_t)(reg >> 8)) ||
        i2c_write_byte((uint8_t)reg)) {
        i2c_stop();
        return -1;
    }

    for (i = 0U; i < length; i++) {
        if (i2c_write_byte(data[i])) {
            i2c_stop();
            return -1;
        }
    }

    i2c_stop();
    return 0;
}

static int gt911_read_reg(uint16_t reg, uint8_t *data, unsigned length)
{
    unsigned i;

    i2c_start();
    if (i2c_write_byte(GT911_ADDR_WRITE) ||
        i2c_write_byte((uint8_t)(reg >> 8)) ||
        i2c_write_byte((uint8_t)reg)) {
        i2c_stop();
        return -1;
    }

    i2c_start();
    if (i2c_write_byte(GT911_ADDR_READ)) {
        i2c_stop();
        return -1;
    }

    for (i = 0U; i < length; i++)
        data[i] = i2c_read_byte(i + 1U < length);

    i2c_stop();
    return 0;
}

int gt911_init(void)
{
    gt911_runtime_state.x = 0U;
    gt911_runtime_state.y = 0U;
    gt911_runtime_state.start_x = 0U;
    gt911_runtime_state.start_y = 0U;
    gt911_runtime_state.pressed = 0U;
    gt911_runtime_state.dragging = 0U;

    /* Stock 0x40049788: software-I2C pins are outputs and idle high. */
    gpio_set_output(GT_SDA_PORT, GT_SDA_PIN);
    gpio_set_output(GT_SCL_PORT, GT_SCL_PIN);
    gpio_write(GT_SDA_PORT, GT_SDA_PIN, 1);
    gpio_write(GT_SCL_PORT, GT_SCL_PIN, 1);

    /* Stock 0x400497D4: INT and RESET initially idle high as outputs. */
    gpio_set_output(GT_INT_PORT, GT_INT_PIN);
    gpio_set_output(GT_RST_PORT, GT_RST_PIN);
    gpio_write(GT_INT_PORT, GT_INT_PIN, 1);
    gpio_write(GT_RST_PORT, GT_RST_PIN, 1);

    /* Stock 0x40049834 reset/address-selection sequence. */
    gpio_write(GT_INT_PORT, GT_INT_PIN, 0);
    gpio_write(GT_RST_PORT, GT_RST_PIN, 0);
    timer_avs_delay_units(40U);

    gpio_write(GT_INT_PORT, GT_INT_PIN, 1);
    gpio_write(GT_RST_PORT, GT_RST_PIN, 0);
    timer_avs_delay_units(40U);

    gpio_write(GT_RST_PORT, GT_RST_PIN, 0);
    timer_avs_delay_units(40U);

    gpio_write(GT_RST_PORT, GT_RST_PIN, 1);

    /* Stock 0x40049820 releases RESET, not INT, to input mode. */
    gpio_set_input(GT_RST_PORT, GT_RST_PIN);
    timer_avs_delay_units(20U);

    /* INT deliberately remains an output driven high, matching stock. */
    return 0;
}

int gt911_probe(gt911_info_t *info)
{
    uint8_t product_id[4];
    unsigned i;

    if (info) {
        for (i = 0U; i < 5U; ++i)
            info->product_id[i] = 0;

        info->address = GT911_I2C_ADDRESS;
        info->available = 0U;
    }

    if (gt911_read_reg(GT911_REG_PRODUCT_ID, product_id, 4U))
        return -1;

    if (info) {
        for (i = 0U; i < 4U; ++i)
            info->product_id[i] = (char)product_id[i];

        info->product_id[4] = 0;
        info->available = 1U;
    }

    return 0;
}

int gt911_debug_sample(gt911_debug_sample_t *sample, int clear_status)
{
    uint8_t point[4];
    uint8_t clear = 0U;

    if (!sample)
        return -1;

    sample->touch.x = 0U;
    sample->touch.y = 0U;
    sample->touch.start_x = 0U;
    sample->touch.start_y = 0U;
    sample->touch.dx = 0;
    sample->touch.dy = 0;
    sample->touch.pressed = 0U;
    sample->touch.points = 0U;
    sample->touch.state = GT911_TOUCH_NONE;
    sample->raw_status = 0U;
    sample->data_ready = 0U;
    sample->int_level = (uint8_t)gpio_read(GT_INT_PORT, GT_INT_PIN);

    if (gt911_read_reg(GT911_REG_STATUS, &sample->raw_status, 1U))
        return -1;

    sample->data_ready = (sample->raw_status & 0x80U) != 0U;
    sample->touch.points = sample->raw_status & 0x0FU;

    if (sample->data_ready && sample->touch.points != 0U) {
        if (gt911_read_reg(GT911_REG_POINT1, point, 4U))
            return -2;

        sample->touch.x = (uint16_t)point[0] |
                          ((uint16_t)point[1] << 8);
        sample->touch.y = (uint16_t)point[2] |
                          ((uint16_t)point[3] << 8);
        sample->touch.pressed = 1U;
    }

    if (clear_status && sample->data_ready) {
        if (gt911_write_reg(GT911_REG_STATUS, &clear, 1U))
            return -3;
    }

    return sample->data_ready ? 1 : 0;
}

int gt911_poll(gt911_touch_t *touch)
{
    gt911_debug_sample_t sample;
    int32_t dx;
    int32_t dy;
    uint32_t abs_dx;
    uint32_t abs_dy;
    int result;

    if (!touch)
        return -1;

    /*
     * Always return a complete event object. When there is no DATA_READY
     * sample, state remains NONE and the current physical state is retained.
     */
    touch->x = gt911_runtime_state.x;
    touch->y = gt911_runtime_state.y;
    touch->start_x = gt911_runtime_state.start_x;
    touch->start_y = gt911_runtime_state.start_y;
    touch->dx = 0;
    touch->dy = 0;
    touch->pressed = gt911_runtime_state.pressed;
    touch->points = 0U;
    touch->state = GT911_TOUCH_NONE;

    result = gt911_debug_sample(&sample, 1);
    if (result <= 0)
        return result;

    if (sample.touch.pressed != 0U && sample.touch.points != 0U) {
        touch->points = sample.touch.points;
        touch->pressed = 1U;
        touch->x = sample.touch.x;
        touch->y = sample.touch.y;

        if (gt911_runtime_state.pressed == 0U) {
            gt911_runtime_state.pressed = 1U;
            gt911_runtime_state.dragging = 0U;
            gt911_runtime_state.x = touch->x;
            gt911_runtime_state.y = touch->y;
            gt911_runtime_state.start_x = touch->x;
            gt911_runtime_state.start_y = touch->y;

            touch->start_x = touch->x;
            touch->start_y = touch->y;
            touch->state = GT911_TOUCH_PRESS;
            return result;
        }

        dx = (int32_t)touch->x - (int32_t)gt911_runtime_state.x;
        dy = (int32_t)touch->y - (int32_t)gt911_runtime_state.y;

        touch->dx = (int16_t)dx;
        touch->dy = (int16_t)dy;
        touch->start_x = gt911_runtime_state.start_x;
        touch->start_y = gt911_runtime_state.start_y;

        abs_dx = dx < 0 ? (uint32_t)(-dx) : (uint32_t)dx;
        abs_dy = dy < 0 ? (uint32_t)(-dy) : (uint32_t)dy;

        if (!gt911_runtime_state.dragging &&
            (abs_dx >= GT911_DRAG_THRESHOLD ||
             abs_dy >= GT911_DRAG_THRESHOLD)) {
            gt911_runtime_state.dragging = 1U;
        }

        touch->state = gt911_runtime_state.dragging
                     ? GT911_TOUCH_DRAG
                     : GT911_TOUCH_HELD;

        gt911_runtime_state.x = touch->x;
        gt911_runtime_state.y = touch->y;
        return result;
    }

    /*
     * DATA_READY with zero active points is the release edge when a gesture
     * was previously active. Preserve the last coordinate for hit/drag users.
     */
    if (gt911_runtime_state.pressed != 0U) {
        touch->x = gt911_runtime_state.x;
        touch->y = gt911_runtime_state.y;
        touch->start_x = gt911_runtime_state.start_x;
        touch->start_y = gt911_runtime_state.start_y;
        touch->pressed = 0U;
        touch->points = 0U;
        touch->state = GT911_TOUCH_RELEASE;

        gt911_runtime_state.pressed = 0U;
        gt911_runtime_state.dragging = 0U;
        return result;
    }

    touch->pressed = 0U;
    touch->points = 0U;
    touch->state = GT911_TOUCH_NONE;
    return result;
}

