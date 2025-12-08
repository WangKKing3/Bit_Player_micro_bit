//main.c

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/sys/printk.h>
#include <nrfx_saadc.h>

#include "joystick_ble.h"

#define ADC_NODE DT_NODELABEL(adc)
#define RESOLUTION 10 
#define VREF_MV 3000

/* Knapper */
static const struct gpio_dt_spec button_a = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static const struct gpio_dt_spec button_b = GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios);

static const struct device *adc_dev;

/* Buffer for ADC reading */
static int16_t sample_buffer[1];

/* Siste gyldige verdier (for feilhåndtering) */
static int16_t last_valid_x = 512;
static int16_t last_valid_y = 512;

/* ADC channel configuration */
static struct adc_channel_cfg channel_cfg_y = {
	.gain = ADC_GAIN_1_5,
	.reference = ADC_REF_INTERNAL,
	.acquisition_time = ADC_ACQ_TIME_DEFAULT,
	.channel_id = 0,
	.differential = 0,
#ifdef CONFIG_ADC_NRFX_SAADC
	.input_positive = NRF_SAADC_INPUT_AIN2,  /* P0.04 = AIN2 = P2 */
#endif
};

static struct adc_channel_cfg channel_cfg_x = {
	.gain = ADC_GAIN_1_5,
	.reference = ADC_REF_INTERNAL,
	.acquisition_time = ADC_ACQ_TIME_DEFAULT,
	.channel_id = 1,
	.differential = 0,
#ifdef CONFIG_ADC_NRFX_SAADC
	.input_positive = NRF_SAADC_INPUT_AIN1,  /* P0.03 = AIN1 = P1 */
#endif
};

/* Status variabler */
static bool ble_advertising = false;
static struct gpio_callback button_cb_data;

/* Les ADC-kanal og returner råverdi (0-1023) */
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
	if (ret < 0) {
		return ret;
	}

	/* Clamp til gyldig område (0-1023) */
	int16_t val = sample_buffer[0];
	if (val < 0) val = 0;
	if (val > 1023) val = 1023;
	
	*result = val;
	return 0;
}

/* Button interrupt handler */
static void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	if (pins & BIT(button_a.pin)) {
		printk("Button A pressed\n");
		if (!ble_advertising) {
			printk("Starting Bluetooth advertising...\n");
			ble_connect();
			ble_advertising = true;
		}
	} else if (pins & BIT(button_b.pin)) {
		printk("Button B pressed\n");
		if (ble_advertising) {
			printk("Stopping Bluetooth...\n");
			ble_cancel_connect();
			ble_advertising = false;
		}
	}
}

/* Konfigurer knapper */
static int configure_buttons(void)
{
	int ret;

	if (!device_is_ready(button_a.port)) {
		printk("ERROR: Button device not ready\n");
		return -1;
	}

	ret = gpio_pin_configure_dt(&button_a, GPIO_INPUT | GPIO_PULL_UP);
	if (ret < 0) return ret;

	ret = gpio_pin_configure_dt(&button_b, GPIO_INPUT | GPIO_PULL_UP);
	if (ret < 0) return ret;

	ret = gpio_pin_interrupt_configure_dt(&button_a, GPIO_INT_EDGE_FALLING);
	if (ret < 0) return ret;

	ret = gpio_pin_interrupt_configure_dt(&button_b, GPIO_INT_EDGE_FALLING);
	if (ret < 0) return ret;

	gpio_init_callback(&button_cb_data, button_pressed,
	                   BIT(button_a.pin) | BIT(button_b.pin));
	
	ret = gpio_add_callback(button_a.port, &button_cb_data);
	if (ret < 0) return ret;

	printk("Buttons configured\n");
	return 0;
}

int main(void)
{
	int ret;
	int count = 0;
	int16_t raw_y, raw_x;
	uint8_t buttons;

	printk("\n===========================================\n");
	printk("Joystick BLE Sender\n");
	printk("Hold A on receiver to control car\n");
	printk("===========================================\n\n");

	/* Konfigurer knapper */
	ret = configure_buttons();
	if (ret < 0) {
		printk("WARNING: Button config failed\n");
	}

	/* Initialiser Bluetooth */
	ble_init();

	/* Get ADC device */
	adc_dev = DEVICE_DT_GET(ADC_NODE);
	if (!device_is_ready(adc_dev)) {
		printk("ERROR: ADC device not ready\n");
		return -1;
	}

	/* Configure ADC channels */
	ret = adc_channel_setup(adc_dev, &channel_cfg_y);
	if (ret < 0) {
		printk("ERROR: ADC Y channel setup failed\n");
		return -1;
	}

	ret = adc_channel_setup(adc_dev, &channel_cfg_x);
	if (ret < 0) {
		printk("ERROR: ADC X channel setup failed\n");
		return -1;
	}

	printk("ADC ready\n");

	/* Auto-start advertising */
	printk("Auto-starting Bluetooth...\n");
	ble_connect();
	ble_advertising = true;

	/* Main loop */
	while (1) {
		/* Les knapp A (aktiv lav) */
		buttons = 0;
		if (gpio_pin_get_dt(&button_a) == 0) {
			buttons |= BTN_A_MASK;
		}
		if (gpio_pin_get_dt(&button_b) == 0) {
			buttons |= BTN_B_MASK;
		}

		/* Les Y-akse */
		ret = read_channel_raw(channel_cfg_y.channel_id, &raw_y);
		if (ret < 0) {
			raw_y = last_valid_y;  /* Bruk siste gyldige */
		} else {
			last_valid_y = raw_y;
		}

		/* Les X-akse */
		ret = read_channel_raw(channel_cfg_x.channel_id, &raw_x);
		if (ret < 0) {
			raw_x = last_valid_x;  /* Bruk siste gyldige */
		} else {
			last_valid_x = raw_x;
		}

		/* Print til konsoll */
		printk("[%04d] X:%4d Y:%4d", count++, raw_x, raw_y);
		
		if (buttons & BTN_A_MASK) {
			printk(" [A]");
		}

		/* Send via Bluetooth */
		if (ble_is_ready()) {
			ble_send_joystick_data(raw_x, raw_y, buttons);
			printk(" [BLE]");
		}
		
		printk("\n");

		k_msleep(50);  /* 20 Hz oppdatering */
	}

	return 0;
}