/*
 * Joystick Reader with Bluetooth for micro:bit v2
 * Zephyr SDK v2.5.1 - HOLD BUTTON VERSION
 * 
 * Hold knapp A for å sende joystick-data
 * Slipp knapp A for å stoppe sending
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

/* Konfigurer knapper (kun som input, ingen interrupts) */
static int configure_buttons(void)
{
	int ret;

	printk("Configuring buttons...\n");

	if (!device_is_ready(button_a.port)) {
		printk("ERROR: Button A device not ready\n");
		return -1;
	}

	if (!device_is_ready(button_b.port)) {
		printk("ERROR: Button B device not ready\n");
		return -1;
	}

	/* Configure button A with pull-up (active low) */
	ret = gpio_pin_configure_dt(&button_a, GPIO_INPUT | GPIO_PULL_UP);
	if (ret < 0) {
		printk("ERROR: Failed to configure button A GPIO (err %d)\n", ret);
		return ret;
	}

	/* Configure button B with pull-up (active low) */
	ret = gpio_pin_configure_dt(&button_b, GPIO_INPUT | GPIO_PULL_UP);
	if (ret < 0) {
		printk("ERROR: Failed to configure button B GPIO (err %d)\n", ret);
		return ret;
	}

	printk("Buttons configured successfully!\n");
	printk("  - Hold Button A to enable joystick control\n");
	printk("  - Release Button A to disable joystick control\n");
	return 0;
}

/* Sjekk om knapp A er holdt inne */
static bool is_button_a_pressed(void)
{
	return gpio_pin_get_dt(&button_a) != 0;
}

int main(void)
{
	int ret;
	int count = 0;
	int mv_y, mv_x;
	int16_t raw_y, raw_x;
	bool button_a_held;
	bool prev_button_state = false;

	printk("\n\n===========================================\n");
	printk("Joystick Reader with Bluetooth\n");
	printk("micro:bit v2 - Zephyr SDK v2.5.1\n");
	printk("HOLD BUTTON VERSION\n");
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
	printk("Hold Button A to send joystick data\n");
	printk("Release Button A to stop sending\n\n");
	
	ble_connect();

	printk("Starting joystick readings...\n\n");

	/* Main loop */
	while (1) {
		/* Sjekk om knapp A er holdt inne */
		button_a_held = is_button_a_pressed();

		/* Print statusendring */
		if (button_a_held != prev_button_state) {
			if (button_a_held) {
				printk("\n>>> Button A HELD - Joystick control ENABLED <<<\n");
			} else {
				printk("\n>>> Button A RELEASED - Joystick control DISABLED <<<\n");
			}
			prev_button_state = button_a_held;
		}

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

			/* Send via Bluetooth KUN hvis knapp A holdes inne OG tilkoblet */
			if (ble_is_ready() && button_a_held) {
				ble_send_joystick_data(raw_x, raw_y);
				printk(" [BLE SENT]");
			}
			
			printk("\r\n");
		}

		k_msleep(100);
	}

	return 0;
}