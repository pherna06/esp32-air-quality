#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "sgp30.h"
#include "defines.h"

const char *TAG = "APP";

static const uint8_t _hours = 20;

static int16_t read_set_baseline()
{
	int16_t res;
	uint32_t baseline;

	if ((res = sgp30_get_iaq_baseline(&baseline)) != 0)
	{
		ESP_LOGE(TAG, "Could not get baseline");
		return res;
	}
	ESP_LOGI(TAG, "Baseline is %d. Setting baseline...", baseline);
	if ((res = sgp30_set_iaq_baseline(baseline)) != 0)
	{
		ESP_LOGE(TAG, "Could not set baseline");
		return res;
	}
	ESP_LOGI(TAG, "Baseline set");

	return res;
}

static void work_sgp30()
{
	int16_t res;

	if ((res = sgp30_probe()) != 0)
	{
		ESP_LOGE(TAG, "Error probing sensor.");
		return;
	}
	ESP_LOGI(TAG, "Sensor detected. Waiting 60 seconds.");
	vTaskDelay(60000 / portTICK_PERIOD_MS);

	if ((res = read_set_baseline()) != 0)
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
				if ((res = sgp30_measure_iaq()) != 0)
				{
					ESP_LOGE(TAG, "Error querying to start measuring.");
					continue;
				}
				vTaskDelay(1000 / portTICK_PERIOD_MS); // 1 Hz

				if ((res = sgp30_read_iaq(&tvoc_ppb, &co2_eq_ppm)) != 0)
				{
					ESP_LOGE(TAG, "Error reading values.");
					continue;
				}
				ESP_LOGI(TAG,
					"Readings %dh %dm %ds. TVOC: %d ppb,   CO2-eq: %d ppm",
					h, m, s, tvoc_ppb, co2_eq_ppm
				);
			}

		if ((res = read_set_baseline()) != 0)
		{
			ESP_LOGE(TAG, "Error setting baseline.");
		}
	}
}

void app_main(void)
{
	work_sgp30();
	vTaskDelay(2000 / portTICK_PERIOD_MS);
}
