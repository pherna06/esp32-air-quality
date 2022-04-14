#include <stdio.h>
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/i2c.h"

#include "sgp30.h"
#include "si7021.h"
#include "defines.h"

const char *TAG = "APP";

static const uint8_t _hours = 20;

static int16_t read_set_baseline(sgp30_handle_t *sgp30)
{
	int16_t res;
	uint32_t baseline;

	if ((res = sgp30_get_iaq_baseline(sgp30, &baseline)) != 0)
	{
		ESP_LOGE(TAG, "Could not get baseline");
		return res;
	}
	ESP_LOGI(TAG, "Baseline is %d. Setting baseline...", baseline);
	if ((res = sgp30_set_iaq_baseline(sgp30, baseline)) != 0)
	{
		ESP_LOGE(TAG, "Could not set baseline");
		return res;
	}
	ESP_LOGI(TAG, "Baseline set");

	return res;
}

static void work_sgp30(sgp30_handle_t *sgp30)
{
	int16_t res;

	ESP_LOGI(TAG, "sgp30 ptr: %d", (uint32_t) sgp30);

	if ((res = sgp30_probe(sgp30)) != 0)
	{
		ESP_LOGE(TAG, "Error probing sensor.");
		return;
	}
	ESP_LOGI(TAG, "Sensor detected. Waiting 60 seconds.");
	vTaskDelay(60000 / portTICK_PERIOD_MS);

	if ((res = read_set_baseline(sgp30)) != 0)
	{
		ESP_LOGE(TAG, "Error setting baseline.");
	}

	uint16_t tvoc_ppb = 0;
	uint16_t co2_eq_ppm = 0;
	for (uint8_t h = 0; h < _hours; ++h)
	{
		for (uint8_t m = 0; m < 60; ++m)
			for (uint8_t s = 0; s < 60; ++s)
			{
				if ((res = sgp30_measure_iaq(sgp30)) != 0)
				{
					ESP_LOGE(TAG, "Error querying to start measuring.");
					continue;
				}
				vTaskDelay(1000 / portTICK_PERIOD_MS); // 1 Hz

				if ((res = sgp30_read_iaq(sgp30, &tvoc_ppb, &co2_eq_ppm)) != 0)
				{
					ESP_LOGE(TAG, "Error reading values.");
					continue;
				}
				ESP_LOGI(TAG,
					"Readings %dh %dm %ds. TVOC: %d ppb,   CO2-eq: %d ppm",
					h, m, s, tvoc_ppb, co2_eq_ppm
				);
			}

		if ((res = read_set_baseline(sgp30)) != 0)
		{
			ESP_LOGE(TAG, "Error setting baseline.");
		}
	}
}

static void work_si7021(i2c_dev_t *si7021)
{
	esp_err_t res = ESP_OK;

	float t  = 0;
	float rh = 0;
	for (uint8_t h = 0; h < _hours; ++h)
		for (uint8_t m = 0; m < 60; ++m)
			for (uint8_t s = 0; s < 60; ++s)
			{
				// 100 ms
				if ((res = si7021_measure_temperature(si7021, &t)) != ESP_OK)
					ESP_LOGE(TAG, "Error reading temperature.");

				// 100 ms
				if ((res = si7021_measure_humidity(si7021, &rh)) != ESP_OK)
					ESP_LOGE(TAG, "Error reading relative humidity.");

				ESP_LOGI(TAG,
					"Readings %dh %dm %ds. Temperature: %.3f ºC,   RH: %.2f %%",
					h, m, s, t, rh
				);

				vTaskDelay(800 / portTICK_PERIOD_MS); // make it 1 sec
			}
}

static void test_sgp30()
{
	sgp30_handle_t sgp30;
	sgp30_create(
		"SGP30 sensor" ,
		SGP30_GPIO_SCL ,
		SGP30_GPIO_SDA ,
		&sgp30          );

	work_sgp30(&sgp30);

	sgp30_delete(&sgp30);

	vTaskDelay(2000 / portTICK_PERIOD_MS);
}

static void test_si7021()
{
	i2c_dev_t si7021;

	// Init i2c device descriptor
	ESP_ERROR_CHECK( si7021_init_desc(
			&si7021         ,
			I2C_NUM_0       ,
			SI7021_GPIO_SCL ,
			SI7021_GPIO_SDA )
	);

	si7021_set_heater(&si7021, 0);
	si7021_set_resolution(&si7021, 0);

	work_si7021(&si7021);

	si7021_free_desc(&si7021);
}

void app_main(void)
{
	test_sgp30();
}
