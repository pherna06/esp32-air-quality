#include "app_sensor_co2.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"


#include "time.h"

/* Transform private methods */
static const char *MS_TAG = "APP_SENSOR_CO2_TRANSFORM";

#define TRANSFORM_EV_NUM   0x80000000 // bit 32
#define TRANSFORM_EV_WAIT  0x40000000 // bit 31

static uint32_t transform_active_check(
		uint8_t  &sample_num  ,
		uint32_t &sample_wait )
{
	uint32_t notification_value = 0;
	if ( (xTaskNotifyWait(
			(uint32_t) 0x00000000 ,
			(uint32_t) 0xFFFFFFFF ,
			&notification_value   ,
			(TickType_t) 0        ))
			== pdTRUE)
	{
		if (notification_value && TRANSFORM_EV_NUM)
		{
			(*sample_num) = (uint8_t) (notification_value << 2);
			if (*sample_num)
				ESP_LOGI(MS_TAG, "No. of samples used set: %d", *sample_num);
			else
			{
				ESP_LOGI(MS_TAG, "Transform stopped.");
				return -1;
			}
		}
		else if (notification_value && TRANSFORM_EV_WAIT)
		{
			(*sample_wait) = (uint8_t) (notification_value << 2);
			ESP_LOGI(MS_TAG,
				"Maximum time to wait for sample set: %d",
				*sample_wait);
		}
		else
			ESP_LOGW(MS_TAG, "Unknown Transform event.");
	}

	return 0;
}

static uint32_t transform_passive_check(
		uint8_t  &sample_num  ,
		uint32_t &sample_wait )
{
	uint32_t notification_value = 0;
	if ( (xTaskNotifyWait(
			(uint32_t) 0x00000000      ,
			(uint32_t) 0xFFFFFFFF      ,
			&notification_value        ,
			(TickType_t) portMAX_DELAY ))
			== pdTRUE)
	{
		if (notification_value && TRANSFORM_EV_NUM)
		{
			(*sample_num) = (uint8_t) (notification_value << 2);
			if (*sample_num)
				return 0; // Restarting Transform.
			else
				ESP_LOGW(MS_TAG, "Transform already stopped.");
		}
		else if (notification_value && TRANSFORM_EV_WAIT)
		{
			(*sample_wait) = (uint8_t) (notification_value << 2);
			ESP_LOGI(MS_TAG,
				"Maximum time to wait for sample set: %d",
				*sample_wait);
		}
		else
			ESP_LOGW(MS_TAG, "Unknown Transform event.");
	}

	return -1;
}

static uint8_t sample_queue_receive(
		QueueHandle_t       sample_queue ,
		SensorCO2Sample_t  *sample_item  ,
		uint32_t            sample_wait  )
{
	if( (xQueueReceive(
			sample_queue        ,
			(void *)sample_item ,
			(TickType_t) sample_wait / portTICK_PERIOD_MS))
			== pdFALSE)
	{
		ESP_LOGE(MS_TAG,
			"Error receiving sample data. "
			"Check if sensor sampling is active or "
			"increment Transform wait time to receive data."
		);
		return -1;
	}

	return 0;
}

static uint8_t transform_queue_send(
		QueueHandle_t                 transform_queue  ,
		SensorCO2TransformedSample_t *transform_item )
{
	if( (xQueueSend(
			transform_queue        ,
			(void *)transform_item ,
			(TickType_t) 0           ))
			== errQUEUE_FULL)
	{
		ESP_LOGE(MS_TAG, "Transform queue full.");
		return -1;
	}

	return 0;
}

static uint8_t transform_internal_queue_send(
		QueueHandle_t      internal_queue ,
		uint16_t           co2_eq_ppm     )
{
	if( (xQueueSend(
			internal_queue       ,
			(void *)&co2_eq_ppm  ,
			(TickType_t) 0       ))
			== errQUEUE_FULL)
	{
		ESP_LOGE(MS_TAG,
			"Transform internal queue full. "
			"This error should never take place: "
			"It is recommended to delete and restart this task."
		);
		return -1;
	}

	return 0;
}

static uint8_t transform_internal_queue_receive(
		QueueHandle_t      internal_queue ,
		uint16_t          *co2_eq_ppm     )
{
	if( (xQueueReceive(
			internal_queue      ,
			(void *)co2_eq_ppm  ,
			(TickType_t) 0      ))
			== pdFALSE)
	{
		ESP_LOGE(MS_TAG,
			"Error reading from Transform internal queue. "
			"This error should never take place: "
			"It is recommended to delete and restart this task."
		);
		return -1;
	}

	return 0;
}
/////////////////////////



/* Transform task */
static void sensor_co2_transform(
		uint8_t       sample_num      ,
		uint32_t      sample_wait     ,
		QueueHandle_t sample_queue    ,
		QueueHandle_t transform_queue ,
		TaskHandle_t  control_task    )
{
	QueueHandle_t     internal_queue      = NULL;
	UBaseType_t       first_readings      = 0;

	SensorCO2Sample_t            sample_item       = {0, 0};
	SensorCO2TransformedSample_t transform_item  = {0, 0.0f};

	ESP_LOGI(MS_TAG, "Transform task created successfully.");

	while (1)
	{
		if (sample_num > 1) // if initial value is 0, Transform not started.
		{
			// Queue for calculations.
			internal_queue = xQueueCreate(
				(UBaseType_t) (sample_num + 1), // +1 -> First in, then out.
				(UBaseType_t) sizeof(uint16_t)
			);
			if (internal_queue == NULL)
			{
				ESP_LOGE(MS_TAG, "Error creating Transform internal queue.");
				sample_num = 0;
			}

			first_readings = sample_num;
		}

		while (sample_num)
		{
			ESP_LOGI(MS_TAG, "Transforming started. Samples: %d", sample_num);
			ESP_LOGD(MS_TAG, "Transforming started. Wait: %d ms", sample_wait);

			// Check for external notifications.
			if ( transform_active_check(&sample_num, &sample_wait) == -1)
				break;

			// Receive sample data.
			if ( sample_queue_receive(
					sample_queue ,
					&sample_item ,
					sample_wait  ))
					== -1)
				goto transform_shutdown;

			// Check if no multisampling (sample_num == 1)
			if (sample_num == 1)
			{
				transform_item = {
					sample_item.timestamp,
					(float) sample_item.co2_eq_ppm
				};
				goto transform_only_send;
			}
			else // Store value for multisampling
			{
				// Store sample internally.
				if ( (transform_internal_queue_send(
						internal_queue         ,
						sample_item.co2_eq_ppm ))
						== -1)
					goto transform_shutdown;
			}

			if (!first_readings) // No first readings.
			{
				// Remove oldest data from mean.
				if ( (transform_internal_queue_receive(
						internal_queue ,
						&co2_eq_ppm    ))
						== -1)
					goto transform_shutdown;
				
				transform_item.co2_eq_ppm_mean +=
					(transform_item.co2_eq_ppm_mean - (float) co2_eq_ppm) /
					(float) (sample_num - 1);

				// Add new sample to mean.
				transform_item.timestamp = sample_item.timestamp;
				transform_item.co2_eq_ppm_mean +=
					((float) sample_item.co2_eq_ppm -
						transform_item.co2_eq_ppm_mean) /
					(float) (sample_num + 1);
			}
			else // First readings
			{
				first_readings -= 1;

				transform_item.co2_eq_ppm_mean +=
					(float) sample_item.co2_eq_ppm;

				if (first_readings) continue; // More first readings left.

				// Mean available.
				transform_item.timestamp = sample_item.timestamp;
				transform_item.co2_eq_ppm_mean /= (float) sample_num;
			}

		transform_only_send:
			// Send Transform value.
			if ( (transform_queue_send(
					transform_queue ,
					&transform_item ))
					== -1)
				goto transform_shutdown;

			continue;

		transform_shutdown:
			// TODO (¿general notify?)
			ESP_LOGW(MS_TAG, "Transform stopped due to error.");
			break;
		}

		// Deallocate internal queue.
		xQueueDelete(internal_queue);

		// Wait for external start.
		while ( transform_passive_check(&sample_num, &sample_wait) );
	}
}

static void app_sensor_co2_transform_task(void *args)
{
	// !! USES FLOAT CALCULUS -> RESTRICT TO ONE CORE.

	(SensorTransformTaskArgs_t *)args;
	uint8_t       sample_num   = args->sample_num;
	uint32_t      sample_wait  = args->sample_wait;
	QueueHandle_t sample_queue = args->sample_queue;

	QueueHandle_t transform_queue = args->transform_queue;

	TaskHandle_t  control_task = args->control_task;
	free(args);

	sensor_co2_transform(
		sample_num      ,
		sample_wait     ,
		sample_queue    ,
		transform_queue ,
		control_task    );
}
/////////////////////////
/////////////////////////
/////////////////////////