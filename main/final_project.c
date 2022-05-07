#include <stdio.h>
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "sgp30.h"
#include "si7021.h"

#include "app_sensor.h"
#include "defines.h"

const char *TAG = "APP";


/*
static const uint8_t _hours = 20;

static int16_t read_set_baseline(sgp30_handle_t *sgp30)
{
	int16_t res;
	uint32_t baseline;

	if ((res = sgp30_get_iaq_baseline_and_read(sgp30, &baseline)) != 0)
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

	if ((res = sgp30_iaq_init(sgp30)) != 0)
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
				if ((res = sgp30_measure_iaq_and_read(sgp30, &tvoc_ppb, &co2_eq_ppm)) != 0)
				{
					ESP_LOGE(TAG, "Error querying to start measuring.");
					continue;
				}
				vTaskDelay(1000 / portTICK_PERIOD_MS); // 1 Hz

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

static void test_sgp30()
{
	sgp30_handle_t sgp30;
	sgp30_config_args_t args = {
		.scl_gpio_pin = SGP30_GPIO_SCL,
		.sda_gpio_pin = SGP30_GPIO_SDA,
	};
	sgp30_create(
		"SGP30 sensor" ,
		&args          ,
		&sgp30         );

	work_sgp30(&sgp30);

	sgp30_delete(&sgp30);

	vTaskDelay(2000 / portTICK_PERIOD_MS);
}
*/

/*
static void initialize_sgp30_baseline()
{
	esp_err_t ret;

	sgp30_handle_t sgp30;
	sgp30_config_args_t args = {
		.scl_gpio_pin = SGP30_GPIO_SCL,
		.sda_gpio_pin = SGP30_GPIO_SDA,
	};
	sgp30_create(
		"SGP30 sensor" ,
		&args          ,
		&sgp30         );

	sgp30_iaq_init(&sgp30);

	ESP_LOGI(TAG, "Waiting for 15 seconds after SGP30 init...");
	vTaskDelay(15000 / portTICK_PERIOD_MS);

	ESP_LOGI(TAG, "Starting measurement cycle up to 12 hours...");
	uint16_t co2eq_ppm;
	uint16_t tvoc_ppb;

	TickType_t delay_point;

	uint8_t h = 0;
	uint8_t m = 0;
	uint8_t s = 0;
	for (; h < 12; ++h)
		for (m = 0; m < 60; ++m)
			for (s = 0; s < 60; ++s)
			{
				ret = sgp30_measure_iaq_and_read(
					&sgp30     ,
					&tvoc_ppb  ,
					&co2eq_ppm );
				if (ret != ESP_OK)
					ESP_LOGW(TAG, "Error while reading SGP30 measurements.");
				delay_point = xTaskGetTickCount();

				ESP_LOGI(TAG,
					"Readings at %dh %dm %ds. TVOC: %d ppb,   CO2-eq: %d ppm",
					h, m, s, tvoc_ppb, co2eq_ppm
				);

				vTaskDelayUntil(&delay_point, 1000 / portTICK_PERIOD_MS);
			}

	uint32_t baseline;
	ret = sgp30_get_iaq_baseline_and_read(
		&sgp30    ,
		&baseline );
	if (ret != ESP_OK)
		ESP_LOGW(TAG, "Error retrieving IAQ baseline value.");
	ESP_LOGI(
		TAG,
		"Baseline at %dh %dm %ds: %d %X",
		h, m, s, baseline, baseline
	);

	for (; h < 24; ++h)
	{
		for (m = 0; m < 60; ++m)
			for (s = 0; s < 60; ++s)
			{
				ret = sgp30_measure_iaq_and_read(
					&sgp30     ,
					&tvoc_ppb  ,
					&co2eq_ppm );
				if (ret != ESP_OK)
					ESP_LOGW(TAG, "Error while reading SGP30 measurements.");
				delay_point = xTaskGetTickCount();

				ESP_LOGI(TAG,
					"Readings at %dh %dm %ds. TVOC: %d ppb,   CO2-eq: %d ppm",
					h, m, s, tvoc_ppb, co2eq_ppm
				);

				vTaskDelayUntil(&delay_point, 1000 / portTICK_PERIOD_MS);
			}

		ret = sgp30_get_iaq_baseline_and_read(
			&sgp30    ,
			&baseline );
		if (ret != ESP_OK)
			ESP_LOGW(TAG, "Error retrieving IAQ baseline value.");
		ESP_LOGI(
			TAG,
			"Baseline at %dh %dm %ds: %d %X",
			h, m, s, baseline, baseline
		);
	}

	ret = sgp30_delete(&sgp30);
	if (ret != ESP_OK)
		ESP_LOGW(TAG, "Error deleting the SGP30 handle.");

	// Delay for 24 hours.
	vTaskDelay(24 * 60 * 60 * 1000 / portTICK_PERIOD_MS);
}*/

void app_main(void)
{
	nvs_flash_init();
	app_sensor_handle_t sensor;
	app_sensor_init(&sensor);

	app_sensor_start(&sensor);

	uint16_t tvoc_ppb, co2eq_ppm;
	float rh_percent = 0.0f;
	float celsius = 0.0f;

	vTaskDelay(1000 / portTICK_PERIOD_MS);

	for (int m = 0; m < 2; ++m)
		for (int s = 0; s < 60; ++s)
		{
			app_sensor_read_tvoc(&sensor, &tvoc_ppb);
			app_sensor_read_co2eq(&sensor, &co2eq_ppm);
			app_sensor_read_rh(&sensor, &rh_percent);
			app_sensor_read_temperature(&sensor, &celsius);

			ESP_LOGI(TAG, "TVOC: %d ppb\tCO2: %d ppm\tRH: %.2f %%\tºC: %.2f",
				tvoc_ppb, co2eq_ppm, rh_percent, celsius);

			vTaskDelay(1000 / portTICK_PERIOD_MS);
		}
	app_sensor_stop(&sensor);
	app_sensor_delete(&sensor);

	}
