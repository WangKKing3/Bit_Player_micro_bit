
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/sys/printk.h>
#include <nrfx_saadc.h>  /* For NRF_SAADC defines */

#define ADC_NODE DT_NODELABEL(adc)
#define RESOLUTION 10 
#define VREF_MV 3000

static const struct device *adc_dev;

/* Buffer for ADC reading */
static int16_t sample_buffer[1];

/* ADC channel configuration for P0.03 (AIN1) */
static struct adc_channel_cfg channel_cfg_y = {
	.gain = ADC_GAIN_1_5,
	.reference = ADC_REF_INTERNAL,
	.acquisition_time = ADC_ACQ_TIME_DEFAULT,
	.channel_id = 0,  /* Use channel 0 in the sequence */
	.differential = 0,
#ifdef CONFIG_ADC_NRFX_SAADC
	.input_positive = NRF_SAADC_INPUT_AIN2,  /* P0.044 = AIN2 */
#endif
};

static struct adc_channel_cfg channel_cfg_x = {
	.gain = ADC_GAIN_1_5,
	.reference = ADC_REF_INTERNAL,
	.acquisition_time = ADC_ACQ_TIME_DEFAULT,
	.channel_id = 1,  /* Use channel 1 in the sequence */
	.differential = 0,
#ifdef CONFIG_ADC_NRFX_SAADC
	.input_positive = NRF_SAADC_INPUT_AIN1,  /* P0.03 = AIN1 */
#endif
};

static int read_channel_mv(const struct adc_channel_cfg *cfg, int channel_id)
{
	int ret;

	struct adc_sequence sequence = {
		.buffer = sample_buffer,
		.buffer_size = sizeof(sample_buffer),
		.resolution = RESOLUTION,  /* 10-bit resolution */
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


int main(void)
{
	int ret;
	int count = 0;
	int mv_y, mv_x;
	int raw_y, raw_x;

	printk("\n\nADC Test for micro:bit v2 (P1/P0.03/AIN1)\n");
	printk("==========================================\n");

	/* Get ADC device */
	adc_dev = DEVICE_DT_GET(ADC_NODE);
	if (!device_is_ready(adc_dev)) {
		printk("Error: ADC device not ready\n");
		return -1;
	}

	printk("ADC device is ready\n");

	/* Configure ADC channel */
	ret = adc_channel_setup(adc_dev, &channel_cfg_y);
	if (ret < 0) {
		printk("Error: ADC channel setup failed (%d)\n", ret);
		return -1;
	}

	ret = adc_channel_setup(adc_dev, &channel_cfg_x);
	if (ret < 0) {
		printk("Error: ADC channel setup failed (%d)\n", ret);
		return -1;
	}


	printk("Starting ADC readings (press joystick to see values change)...\n\n");

while (1) {

		mv_y = read_channel_mv(&channel_cfg_y, channel_cfg_y.channel_id);
        raw_y = sample_buffer[0]; // Lagre rådata

		mv_x = read_channel_mv(&channel_cfg_x, channel_cfg_x.channel_id);
        raw_x = sample_buffer[0]; // Lagre rådata


		if (mv_y < 0 || mv_x < 0) {
			printk("ADC read failed!\r\n");
		} else {
			printk("[%04d] Y (P2): Raw=%4d / %4dmV | X (P1): Raw=%4d / %4dmV\r\n", 
			count++, raw_y, mv_y, raw_x, mv_x);
		}		 
		k_msleep(100);
	}

	return 0;
}