#include "app_sensor.h"

#include "esp_log.h"
static const char *TAG = "APP_SENSOR";

#define APP_SENSOR_SGP30_MEASURE_PERIOD_MS 1000
#define APP_SENSOR_SGP30_BASELINE_VALUE    0x0000
#define APP_SENSOR_SI7021_AVAILABLE 1

#define APP_SENSOR_TASK_STACK_SIZE 2048
#define APP_SENSOR_TASK_PRIORITY   5

#define APP_SENSOR_TASK_EVENT_DELETE 0x0001

static uint16_t calculate_rh_abs_int(float rh_abs_f)
{
	float intpart, fracpart;
	fracpart = modff(rh_abs_f, &intpart);

	uint8_t rh_int = 0xFF;
	if (intpart < 255.0f)
		rh_int = (uint8_t) intpart;

	uint8_t rh_frac = 0x00;
	if (fracpart > 0.0f)
		rh_frac = (uint8_t) (256.0f * fracpart);

	return ( (uint16_t) rh_frac ) | ( ( (uint16_t) rh_int ) << 8 );
}

static void app_sensor_task(
		void *args )
{
	app_sensor_handle_t *sensor = (app_sensor_handle_t *)args;

	esp_err_t ret;
	BaseType_t xRet;

	// Initialize SGP30 sensor
	ESP_LOGI(TAG, "Initializing SGP30 air quality sensor.");
	ret = sgp30_iaq_init(sensor->sgp30);
	if (ret != ESP_OK)
		ESP_LOGW(TAG, "Error when initializing SGP30 operation.");

	// Check if available baseline
	bool early_phase = true;
	uint32_t baseline = 0x0000;
	xRet = xQueuePeek(sensor->baseline_queue, &baseline, 0);
	if (xRet == pdTRUE)
	{
		early_phase = false;
		sgp30_set_iaq_baseline(sensor->sgp30, baseline);
	}

	/* MEASUREMENT LOOP */

	uint16_t tvoc_ppb;
	uint16_t co2eq_ppm;

	float rh_percent;
	float celsius;
	float rh_abs_f;
	uint16_t rh_abs;

	uint32_t ulNotifiedValue;

	TickType_t period = APP_SENSOR_SGP30_MEASURE_PERIOD_MS / portTICK_PERIOD_MS;
	TickType_t timestamp = xTaskGetTickCount() - period;
	uint16_t secs = 0;
	while (1)
	{
		if (sensor->si7021)
		{
			// Read humidity and temperature
			ret = si7021_measure_and_read_converted(
				sensor->si7021 ,
				&rh_percent    ,
				&celsius       );
			if (ret != ESP_OK)
				ESP_LOGW(TAG, "Error while reading Si7021 measurements");
			else
			{
				// Calculate absolute humidty for SGP30
				rh_abs_f = exp( 17.62f * celsius / (243.12f + celsius) );
				rh_abs_f = (rh_percent / 100.0f) * 6.112f * rh_abs_f / (273.15f + celsius);
				rh_abs_f = 216.7f * rh_abs_f;

				// Set humidity in SGP30 sensor
				rh_abs = calculate_rh_abs_int(rh_abs_f);
				ret = sgp30_set_absolute_humidity(sensor->sgp30, rh_abs);
				if (ret != ESP_OK)
					ESP_LOGW(TAG, "Error setting absolute humidity for SGP30");
			}
		}

		// Read air quality from SGP30
		vTaskDelayUntil(&timestamp, period);
		ret = sgp30_measure_iaq_and_read(
			sensor->sgp30 ,
			&tvoc_ppb     ,
			&co2eq_ppm    );
		if (ret != ESP_OK)
			ESP_LOGW(TAG, "Error while reading SGP30 measurements.");
		timestamp = xTaskGetTickCount();

		// Insert data to queues (no need to check errQUEUE_FULL)
		xQueueOverwrite(sensor->tvoc_queue  , &tvoc_ppb  );
		xQueueOverwrite(sensor->co2eq_queue , &co2eq_ppm );
		if (sensor->si7021)
		{
			xQueueOverwrite(sensor->rh_queue      , &rh_percent );
			xQueueOverwrite(sensor->celsius_queue , &celsius    );
		}

		// Check for baseline retrieval
		secs++;
		if (early_phase)
		{
			if (secs == 43200) // 12h
			{
				early_phase = false;
				secs = 0;
			}
		}
		else if (secs == 3600) // 1h
			secs = 0;

		if (secs == 0)
		{
			ret = sgp30_get_iaq_baseline_and_read(sensor->sgp30, &baseline);
			xQueueOverwrite(sensor->baseline_queue , &baseline);
		}

		// Check for other ops instantly (max start time: period)
		// TODO (optional)
		xRet = xTaskNotifyWait(
			pdFALSE          ,  // Don't clear bits on entry
			ULONG_MAX        ,  // Clear all bits on exit
			&ulNotifiedValue ,  // Stores the notified value
			0                ); // Do not block
		if (xRet == pdPASS)
		{
			if (ulNotifiedValue & APP_SENSOR_TASK_EVENT_DELETE)
			{
				sensor->task = NULL; // for external check
				vTaskDelete(NULL);
			}
		}
	}
}



esp_err_t app_sensor_init(
		app_sensor_handle_t *sensor )
{
	esp_err_t ret;

	// SGP30 handle
	sensor->sgp30 = malloc( sizeof(sgp30_handle_t) );
	sgp30_config_args_t sgp30_args = {
		.scl_gpio_pin = SGP30_GPIO_SCL ,
		.sda_gpio_pin = SGP30_GPIO_SDA };
	ret = sgp30_create(
		"App IAQ sensor: SGP30" ,
		&sgp30_args             ,
		sensor->sgp30       );

	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG, "Error creating SGP30 handle.");
		return ESP_FAIL;
	}

	// SGP30 data queues
	sensor->co2eq_queue    = xQueueCreate( 1, sizeof( uint16_t ) );
	sensor->tvoc_queue     = xQueueCreate( 1, sizeof( uint16_t ) );
	sensor->baseline_queue = xQueueCreate( 1, sizeof( uint32_t ) );
	/*
	if (sensor->co2eq_queue == NULL)
		// TODO
	if (sensor->tvoc == NULL)
		// TODO
	if (sensor->baseline_queue == NULL)
		// TODO
	*/

	// Available HW baseline?
	uint32_t baseline = APP_SENSOR_SGP30_BASELINE_VALUE;
	if (baseline)
		xQueueOverwrite(sensor->baseline_queue, &baseline);

	// Si7021 handle
	if (APP_SENSOR_SI7021_AVAILABLE)
	{
		sensor->si7021 = malloc( sizeof(si7021_handle_t) );
		si7021_config_args_t si7021_args = {
			.scl_gpio_pin = SI7021_GPIO_SCL ,
			.sda_gpio_pin = SI7021_GPIO_SDA };
		ret = si7021_create(
			"App RH sensor: Si7021" ,
			&si7021_args            ,
			sensor->si7021          );

		if (ret != ESP_OK)
		{
			ESP_LOGE(TAG, "Error creating Si7021 handle.");
			return ESP_FAIL;
		}

		// Si7021 data queues
		sensor->rh_queue      = xQueueCreate( 1, sizeof(float) );
		sensor->celsius_queue = xQueueCreate( 1, sizeof(float) );
		/*
		if (sensor->rh_queue == NULL)
			// TODO
		if (sensor->celsius_queue == NULL)
			// TODO
		*/
	}
	else
		sensor->si7021 = NULL;

	return ESP_OK;
}

esp_err_t app_sensor_start(
		app_sensor_handle_t *sensor )
{
	BaseType_t xRet;

	xRet = xTaskCreate(
		app_sensor_task            ,
		"App sensor: task"         ,
		APP_SENSOR_TASK_STACK_SIZE ,
		sensor                     ,
		APP_SENSOR_TASK_PRIORITY   ,
		&(sensor->task)            );

	if (xRet != pdPASS)
	{
		ESP_LOGE(TAG, "Error creating operation task.");
		return ESP_FAIL;
	}
	
	return ESP_OK;
}

esp_err_t app_sensor_stop(
		app_sensor_handle_t *sensor )
{
	if (sensor->task == NULL)
	{
		ESP_LOGW(TAG, "Task already deleted.");
		return ESP_OK;
	}

	xTaskNotify(
		sensor->task                 ,
		APP_SENSOR_TASK_EVENT_DELETE ,
		eSetBits                     );

	// Wait for deletion
	vTaskDelay(1000 / portTICK_PERIOD_MS);

	if (sensor->task)
	{
		ESP_LOGE(TAG, "Could not assert task deletion.");
		return ESP_FAIL;
	}

	return ESP_OK;
}

esp_err_t app_sensor_delete(
		app_sensor_handle_t * sensor )
{
	esp_err_t ret;
	
	if (sensor->task)
	{
		ESP_LOGE(TAG, "Sensor must be stopped prior deletion.");
		return ESP_FAIL;
	}

	// SGP30 handle
	ret = sgp30_delete(sensor->sgp30);
	if (ret != ESP_OK)
		ESP_LOGW(TAG, "Error deleting SGP30 handle. Possible momory leak.");
	free(sensor->sgp30);
	
	// SGP30 data queues
	vQueueDelete(sensor->co2eq_queue);
	vQueueDelete(sensor->tvoc_queue);
	vQueueDelete(sensor->baseline_queue);

	// Si7021 handle
	if (APP_SENSOR_SI7021_AVAILABLE)
	{
		ret = si7021_delete(sensor->si7021);
		if (ret != ESP_OK)
			ESP_LOGW(TAG, "Error deleting Si7021 handle. Possible memory leak.");
		free(sensor->si7021);
		
		// Si7021 data queues
		vQueueDelete(sensor->rh_queue);
		vQueueDelete(sensor->celsius_queue);
	}

	return ESP_OK;
}

esp_err_t app_sensor_read_tvoc(
		app_sensor_handle_t *sensor   ,
		uint16_t            *tvoc_ppb )
{
	BaseType_t xRet;
	xRet = xQueuePeek(sensor->tvoc_queue, tvoc_ppb, 0);
	if (xRet == pdFALSE)
	{
		ESP_LOGE(TAG, "No value found for TVOC.");
		return ESP_FAIL;
	}

	return ESP_OK;
}

esp_err_t app_sensor_read_co2eq(
		app_sensor_handle_t *sensor    ,
		uint16_t            *co2eq_ppm )
{
	BaseType_t xRet;
	xRet = xQueuePeek(sensor->co2eq_queue, co2eq_ppm, 0);
	if (xRet == pdFALSE)
	{
		ESP_LOGE(TAG, "No value found for CO2.");
		return ESP_FAIL;
	}

	return ESP_OK;
}

esp_err_t app_sensor_read_baseline(
		app_sensor_handle_t *sensor   ,
		uint32_t            *baseline )
{
	BaseType_t xRet;
	xRet = xQueuePeek(sensor->baseline_queue, baseline, 0);
	if (xRet == pdFALSE)
	{
		ESP_LOGE(TAG, "No value found for SGP30 baseline.");
		return ESP_FAIL;
	}

	return ESP_OK;
}

esp_err_t app_sensor_read_rh(
		app_sensor_handle_t *sensor     ,
		float               *rh_percent )
{
	BaseType_t xRet;
	xRet = xQueuePeek(sensor->rh_queue, rh_percent, 0);
	if (xRet == pdFALSE)
	{
		ESP_LOGE(TAG, "No value found for relative humidity.");
		return ESP_FAIL;
	}

	return ESP_OK;
}

esp_err_t app_sensor_read_temperature(
		app_sensor_handle_t *sensor  ,
		float               *celsius )
{
	BaseType_t xRet;
	xRet = xQueuePeek(sensor->celsius_queue, celsius, 0);
	if (xRet == pdFALSE)
	{
		ESP_LOGE(TAG, "No value found for temperature.");
		return ESP_FAIL;
	}

	return ESP_OK;
}