#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <nrfx_saadc.h>

#include "joystick_state.h"
#include "sender_ble.h"
#include "motor_types.h"

#define ADC_NODE DT_NODELABEL(adc)
#define RESOLUTION 10 

#define SEND_INTERVAL_MS 100
#define POLL_INTERVAL_MS 20

static const struct gpio_dt_spec btn_a = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static const struct gpio_dt_spec btn_b = GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios);

static const struct device *adc_dev;
static int16_t adc_buffer[1];
static int16_t last_x = 512, last_y = 512;
static int64_t last_send = 0;

static struct adc_channel_cfg cfg_y = {
    .gain = ADC_GAIN_1_5,
    .reference = ADC_REF_INTERNAL,
    .acquisition_time = ADC_ACQ_TIME_DEFAULT,
    .channel_id = 0,
#ifdef CONFIG_ADC_NRFX_SAADC
    .input_positive = NRF_SAADC_INPUT_AIN2,
#endif
};

static struct adc_channel_cfg cfg_x = {
    .gain = ADC_GAIN_1_5,
    .reference = ADC_REF_INTERNAL,
    .acquisition_time = ADC_ACQ_TIME_DEFAULT,
    .channel_id = 1,
#ifdef CONFIG_ADC_NRFX_SAADC
    .input_positive = NRF_SAADC_INPUT_AIN1,
#endif
};

static int read_adc(int ch, int16_t *val)
{
    struct adc_sequence seq = {
        .buffer = adc_buffer,
        .buffer_size = sizeof(adc_buffer),
        .resolution = RESOLUTION,
        .channels = BIT(ch),
    };
    int ret = adc_read(adc_dev, &seq);
    if (ret < 0) return ret;
    *val = (adc_buffer[0] < 0) ? 0 : (adc_buffer[0] > 1023) ? 1023 : adc_buffer[0];
    return 0;
}

static const char *dir_name(Motor_direction d)
{
    switch (d) {
        case Forward: return "FWD";
        case Backward: return "BWD";
        case Rotate_Right: return "ROT_R";
        case Rotate_Left: return "ROT_L";
        case Right: return "RIGHT";
        case Left: return "LEFT";
        case Stop: return "STOP";
        case Idle: return "IDLE";
        default: return "?";
    }
}

int main(void)
{
    int16_t y, x;
    Motor_direction dir; // Current direction
    uint8_t speed;

    printk("\n=== Joystick Sender (DEBUG) ===\n");
    printk("Sends Motor_direction directly\n\n");

    if (device_is_ready(btn_a.port)) {
        gpio_pin_configure_dt(&btn_a, GPIO_INPUT | GPIO_PULL_UP);
        gpio_pin_configure_dt(&btn_b, GPIO_INPUT | GPIO_PULL_UP);
    }

    adc_dev = DEVICE_DT_GET(ADC_NODE);
    if (!device_is_ready(adc_dev)) {
        printk("ADC not ready\n");
        return -1;
    }
    adc_channel_setup(adc_dev, &cfg_y);
    adc_channel_setup(adc_dev, &cfg_x);

    joystick_state_init();

    ble_init();
    ble_connect();

    printk("Hold A to drive\n\n");

    while (1) {
        /* Read joystick */
        if (read_adc(cfg_y.channel_id, &y) < 0) y = last_y;
        else last_y = y;

        if (read_adc(cfg_x.channel_id, &x) < 0) x = last_x;
        else last_x = x;

        bool btn = (gpio_pin_get_dt(&btn_a) != 0);
        bool btn_b_state = (gpio_pin_get_dt(&btn_b) != 0);

        /* Convert to direction */
        bool changed = joystick_to_direction(x, y, btn, btn_b_state, &dir, &speed);

        /* DEBUG: Always print raw values */
        printk("X:%4d Y:%4d btn:%d -> %-5s %3d%%", x, y, btn ? 1 : 0, dir_name(dir), speed);

        /* Send if changed or interval passed */
        int64_t now = k_uptime_get();
        if (ble_is_ready() && (changed || (now - last_send) >= SEND_INTERVAL_MS)) {
            ble_send_direction(dir, speed);
            last_send = now;
            printk(" [TX]");
        }
        printk("\n");

        k_msleep(POLL_INTERVAL_MS);
    }
    return 0;
}