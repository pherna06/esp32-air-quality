#include "app_sensor_co2.h"
#include "sgp30.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"


#include "time.h"

/* Sampling private methods */
static const char *S_TAG  = "APP_SENSOR_CO2_SAMPLE";

static uint32_t sample_active_check(
		uint32_t &sample_freq )
{
	if ( (xTaskNotifyWait(
			(uint32_t) 0x00000000 ,
			(uint32_t) 0xFFFFFFFF ,
			sample_freq           ,
			(TickType_t) 0        ))
			== pdTRUE)
	{
		if ( !(*sample_freq) )
		{
			ESP_LOGE(S_TAG, "Sampling stopped.");
			return -1;
		}
		
		ESP_LOGI(S_TAG, "Sampling frequency set: %d ms", *sample_freq);
	}
	
	return 0;
}

static uint32_t sample_passive_check(
		uint32_t &sample_freq )
{
	if ( (xTaskNotifyWait(
			(uint32_t) 0x00000000      ,
			(uint32_t) 0xFFFFFFFF      ,
			sample_freq                ,
			(TickType_t) portMAX_DELAY ))
			== pdTRUE)
	{
		if ( *sample_freq )
			return 0;
		
		ESP_LOGW(S_TAG, "Sampling already stopped.");
	}

	return -1;
}

static uint8_t sample_queue_send(
		QueueHandle_t  sample_queue ,
		SensorCO2Sample_t  *sample_item  )
{
	if( (xQueueSend(
			sample_queue        ,
			(void *)sample_item ,
			(TickType_t) 0))
			== errQUEUE_FULL)
	{
		ESP_LOGE(S_TAG, "Sample queue full.");
		return -1;
	}

	return 0;
}
/////////////////////////





/* Sampling Task */
static void sensor_co2_sample(
		uint32_t      sample_freq  ,
		QueueHandle_t sample_queue ,
		TaskHandle_t  control_task )
{
	SensorCO2Sample_t sample_item = {0, 0};
	uint32_t notification_value = 0;
	uint16_t tvoc_ppb = 0;

	ESP_LOGI(MS_TAG, "Sampling task created successfully.");

	while(1)
	{
		while (sample_freq)
		{
			ESP_LOGI(S_TAG, "Sampling started. Frequency: %d ms", sample_freq);

			// Check for external notifications.
			if ( sample_active_check(&sample_freq) == -1)
				break;

			// Tell sensor to start measuring.
			if ( sgp30_measure_iaq() )
			{
				// TODO (notify)
				ESP_LOGE(S_TAG, "Error starting SGP30 measure.");
				goto sample_shutdown;
			}

			// Wait for sample frequency.
			vTaskDelay(sample_freq / portTICK_PERIOD_MS);

			// Read sample from sensor.
			sgp30_flag = sgp30_read_iaq(&tvoc_ppb, &(sample_item.co2_eq_ppm));
			if ( sgp30_read_iaq(
					&tvoc_ppb                 ,
					&(sample_item.co2_eq_ppm) ))
			{
				// TODO (notify)
				ESP_LOGE(S_TAG, "Error reading SGP30 measure.");
				goto sample_shutdown;
			}

			// Set sample timestamp.
			if ( (sample_item.timestamp = time(NULL)) == -1)
			{
				ESP_LOGE(S_TAG, "Error reading current time.");
				goto sample_shutdown;
			}

			// Send sample data to queue.
			if ( (sample_queue_send(
					sample_queue ,
					&sample_item ))
					== -1)
				goto sample_shutdown;

			continue;

		sample_shutdown:
			// TODO (¿general notify?)
			ESP_LOGW(S_TAG, "Sampling stopped due to error.");
			break;
		}

		// Wait for external start.
		while( !(sample_passive_check(&sample_freq)) );
	}
}

static void app_sensor_co2_sample_task(void *args)
{
	(SensorSampleTaskArgs_t *)args;
	uint32_t      sample_freq  = args->sample_freq;
	QueueHandle_t sample_queue = args->sample_queue;

	TaskHandle_t  control_task = args->control_task;
	free(args);

	sensor_co2_sample(
		sample_freq  ,
		sample_queue ,
		control_task );	
}
/////////////////////////