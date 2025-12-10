//main.c - SIMPLIFIED VERSION
// Fast 150ms send interval, no delta logic

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <nrfx_saadc.h>

#include "joystick_ble.h"

#define ADC_NODE DT_NODELABEL(adc)
#define RESOLUTION 10 

/* FAST FIXED INTERVAL - ingen delta-logikk */
#define SEND_INTERVAL_MS 150   /* Send hver 150ms = ~6.7 pakker/sek */
#define POLL_INTERVAL_MS 30    /* Poll joystick hver 30ms for responsiv input */

/* Knapper */
static const struct gpio_dt_spec button_a = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static const struct gpio_dt_spec button_b = GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios);

static const struct device *adc_dev;
static int16_t sample_buffer[1];

/* Siste gyldige verdier */
static int16_t last_valid_x = 512;
static int16_t last_valid_y = 512;

/* Tidsstyring */
static int64_t last_send_time = 0;

/* ADC channel configuration */
static struct adc_channel_cfg channel_cfg_y = {
	.gain = ADC_GAIN_1_5,
	.reference = ADC_REF_INTERNAL,
	.acquisition_time = ADC_ACQ_TIME_DEFAULT,
	.channel_id = 0,
	.differential = 0,
#ifdef CONFIG_ADC_NRFX_SAADC
	.input_positive = NRF_SAADC_INPUT_AIN2,
#endif
};

static struct adc_channel_cfg channel_cfg_x = {
	.gain = ADC_GAIN_1_5,
	.reference = ADC_REF_INTERNAL,
	.acquisition_time = ADC_ACQ_TIME_DEFAULT,
	.channel_id = 1,
	.differential = 0,
#ifdef CONFIG_ADC_NRFX_SAADC
	.input_positive = NRF_SAADC_INPUT_AIN1,
#endif
};

static int read_channel_raw(int channel_id, int16_t *result)
{
	int ret;
	struct adc_sequence sequence = {
		.buffer = sample_buffer,
		.buffer_size = sizeof(sample_buffer),
		.resolution = RESOLUTION,
		.oversampling = 0,
		.calibrate = 0,
		.channels = BIT(channel_id),
	};

	ret = adc_read(adc_dev, &sequence);
	if (ret < 0) return ret;

	int16_t val = sample_buffer[0];
	if (val < 0) val = 0;
	if (val > 1023) val = 1023;
	*result = val;
	return 0;
}

int main(void)
{
	int ret;
	int count = 0;
	int16_t raw_y, raw_x;
	uint8_t buttons;

	printk("\n===========================================\n");
	printk("Joystick BLE Sender v3 (Fixed Interval)\n");
	printk("Send interval: %d ms (~%.1f pkts/sec)\n", 
	       SEND_INTERVAL_MS, 1000.0/SEND_INTERVAL_MS);
	printk("===========================================\n\n");

	/* Konfigurer knapper */
	if (device_is_ready(button_a.port)) {
		gpio_pin_configure_dt(&button_a, GPIO_INPUT | GPIO_PULL_UP);
		gpio_pin_configure_dt(&button_b, GPIO_INPUT | GPIO_PULL_UP);
		printk("Buttons ready\n");
	}

	/* Initialiser Bluetooth */
	ble_init();

	/* Get ADC device */
	adc_dev = DEVICE_DT_GET(ADC_NODE);
	if (!device_is_ready(adc_dev)) {
		printk("ERROR: ADC not ready\n");
		return -1;
	}

	adc_channel_setup(adc_dev, &channel_cfg_y);
	adc_channel_setup(adc_dev, &channel_cfg_x);
	printk("ADC ready\n");

	/* Auto-start */
	printk("Starting Bluetooth...\n");
	ble_connect();

	/* Main loop */
	while (1) {
		/* Les knapper */
		buttons = 0;
		if (gpio_pin_get_dt(&button_a) == 0) {
			buttons |= BTN_A_MASK;
		}
		if (gpio_pin_get_dt(&button_b) == 0) {
			buttons |= BTN_B_MASK;
		}

		/* Les joystick */
		ret = read_channel_raw(channel_cfg_y.channel_id, &raw_y);
		if (ret < 0) raw_y = last_valid_y;
		else last_valid_y = raw_y;

		ret = read_channel_raw(channel_cfg_x.channel_id, &raw_x);
		if (ret < 0) raw_x = last_valid_x;
		else last_valid_x = raw_x;

		/* Print */
		printk("[%04d] X:%4d Y:%4d", count++, raw_x, raw_y);
		if (buttons & BTN_A_MASK) printk(" [A]");

		/* Send med FAST intervall - ingen delta-sjekk */
		int64_t now = k_uptime_get();
		if (ble_is_ready() && (now - last_send_time) >= SEND_INTERVAL_MS) {
			ble_send_joystick_data(raw_x, raw_y, buttons);
			last_send_time = now;
			printk(" [BLE]");
		}
		
		printk("\n");
		k_msleep(POLL_INTERVAL_MS);
	}

	return 0;
}