/*
 * Joystick Reader with Bluetooth for micro:bit v2
 * Zephyr SDK v2.5.1 - AUTO-START VERSION
 */

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

/* Knapper for kontroll */
static const struct gpio_dt_spec button_a = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static const struct gpio_dt_spec button_b = GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios);

static const struct device *adc_dev;

/* Buffer for ADC reading */
static int16_t sample_buffer[1];

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

/* Les ADC-kanal og returner verdi i mV */
static int read_channel_mv(const struct adc_channel_cfg *cfg, int channel_id)
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

	return (sample_buffer[0] * VREF_MV) / (1 << RESOLUTION); 
}

/* Button interrupt handler */
static void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	printk("\n!!! BUTTON INTERRUPT TRIGGERED - pins: 0x%08x !!!\n", pins);
	printk("Button A pin: %d, Button B pin: %d\n", button_a.pin, button_b.pin);
	
	if (pins & BIT(button_a.pin)) {
		printk("==> Button A pressed\n");
		if (!ble_advertising) {
			printk("==> Starting Bluetooth advertising...\n");
			ble_connect();
			ble_advertising = true;
		} else {
			printk("==> Already advertising\n");
		}
	} else if (pins & BIT(button_b.pin)) {
		printk("==> Button B pressed\n");
		if (ble_advertising) {
			printk("==> Stopping Bluetooth...\n");
			ble_cancel_connect();
			ble_advertising = false;
		} else {
			printk("==> Not advertising\n");
		}
	} else {
		printk("==> Unknown pin triggered\n");
	}
	printk("\n");
}

/* Konfigurer knapper */
static int configure_buttons(void)
{
	int ret;

	printk("Configuring buttons...\n");
	printk("Button A GPIO controller: %s, pin: %d\n", button_a.port->name, button_a.pin);
	printk("Button B GPIO controller: %s, pin: %d\n", button_b.port->name, button_b.pin);

	if (!device_is_ready(button_a.port)) {
		printk("ERROR: Button A device not ready\n");
		return -1;
	}
	printk("Button A device is ready\n");

	/* Configure with pull-up resistor */
	ret = gpio_pin_configure_dt(&button_a, GPIO_INPUT | GPIO_PULL_UP);
	if (ret < 0) {
		printk("ERROR: Failed to configure button A GPIO (err %d)\n", ret);
		return ret;
	}
	printk("Button A GPIO configured with pull-up\n");

	ret = gpio_pin_configure_dt(&button_b, GPIO_INPUT | GPIO_PULL_UP);
	if (ret < 0) {
		printk("ERROR: Failed to configure button B GPIO (err %d)\n", ret);
		return ret;
	}
	printk("Button B GPIO configured with pull-up\n");

	/* Try EDGE_FALLING instead of EDGE_TO_ACTIVE */
	ret = gpio_pin_interrupt_configure_dt(&button_a, GPIO_INT_EDGE_FALLING);
	if (ret < 0) {
		printk("ERROR: Failed to configure button A interrupt (err %d)\n", ret);
		return ret;
	}
	printk("Button A interrupt configured (EDGE_FALLING)\n");

	ret = gpio_pin_interrupt_configure_dt(&button_b, GPIO_INT_EDGE_FALLING);
	if (ret < 0) {
		printk("ERROR: Failed to configure button B interrupt (err %d)\n", ret);
		return ret;
	}
	printk("Button B interrupt configured (EDGE_FALLING)\n");

	gpio_init_callback(&button_cb_data, button_pressed,
	                   BIT(button_a.pin) | BIT(button_b.pin));
	
	ret = gpio_add_callback(button_a.port, &button_cb_data);
	if (ret < 0) {
		printk("ERROR: Failed to add button callback (err %d)\n", ret);
		return ret;
	}

	printk("Buttons configured successfully!\n");
	return 0;
}

int main(void)
{
	int ret;
	int count = 0;
	int mv_y, mv_x;
	int16_t raw_y, raw_x;

	printk("\n\n===========================================\n");
	printk("Joystick Reader with Bluetooth\n");
	printk("micro:bit v2 - Zephyr SDK v2.5.1\n");
	printk("AUTO-START VERSION\n");
	printk("===========================================\n\n");

	/* Konfigurer knapper */
	ret = configure_buttons();
	if (ret < 0) {
		printk("WARNING: Button configuration failed (continuing anyway)\n");
	}

	/* Initialiser Bluetooth */
	ble_init();

	/* Get ADC device */
	adc_dev = DEVICE_DT_GET(ADC_NODE);
	if (!device_is_ready(adc_dev)) {
		printk("ERROR: ADC device not ready\n");
		return -1;
	}

	printk("ADC device is ready\n");

	/* Configure ADC channels */
	ret = adc_channel_setup(adc_dev, &channel_cfg_y);
	if (ret < 0) {
		printk("ERROR: ADC Y channel setup failed (%d)\n", ret);
		return -1;
	}

	ret = adc_channel_setup(adc_dev, &channel_cfg_x);
	if (ret < 0) {
		printk("ERROR: ADC X channel setup failed (%d)\n", ret);
		return -1;
	}

	printk("ADC channels configured\n");

	/* AUTO-START BLUETOOTH ADVERTISING */
	printk("\n*** AUTO-STARTING BLUETOOTH ADVERTISING ***\n");
	printk("No need to press buttons!\n");
	printk("Press Button B to stop Bluetooth if needed\n\n");
	
	ble_connect();
	ble_advertising = true;

	printk("Starting joystick readings...\n\n");

	/* Main loop */
	while (1) {
		/* Les Y-akse (P2) */
		mv_y = read_channel_mv(&channel_cfg_y, channel_cfg_y.channel_id);
		raw_y = sample_buffer[0];

		/* Les X-akse (P1) */
		mv_x = read_channel_mv(&channel_cfg_x, channel_cfg_x.channel_id);
		raw_x = sample_buffer[0];

		if (mv_y < 0 || mv_x < 0) {
			printk("ADC read failed!\r\n");
		} else {
			/* Print til konsoll */
			printk("[%04d] Y(P2): Raw=%4d/%4dmV | X(P1): Raw=%4d/%4dmV", 
			       count++, raw_y, mv_y, raw_x, mv_x);

			/* Send via Bluetooth hvis tilkoblet */
			if (ble_is_ready()) {
				ble_send_joystick_data(raw_x, raw_y);
				printk(" [BLE SENT]");
			}
			
			printk("\r\n");
		}

		k_msleep(100);
	}

	return 0;
}