#include "app_sensor_co2.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "time.h"
#include "cJSON.h"

static uint8_t transform_queue_receive(
		QueueHandle_t         transform_queue ,
		SensorCO2Transform_t *transform_item  ,
		uint32_t              transform_wait  )
{
	if( (xQueueReceive(
			transform_queue        ,
			(void *)transform_item ,
			(TickType_t) transform_wait / portTICK_PERIOD_MS))
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

static char *sensor_co2_json(
	SensorCO2Transform_t *transform_item ,
	size_t                buffer_len     )
{
	char *buffer = NULL;
	cJSON *root;
	cJSON *timestamp;
	cJSON *co2_eq_ppm_mean;

	// JSON generation.
	if ( (root = cJSON_CreateObject()) == NULL)
		goto json_failed;

	if ( (timestamp = cJSON_CreateNumber(
			transform_item->timestamp))
			== NULL)
		goto json_failed;
	cJSON_AddItemToObject(root, "timestamp", timestamp);

	if ( (co2_eq_ppm_mean = cJSON_CreateNumber(
			transform_item->co2_eq_ppm_mean))
			== NULL)
		goto json_failed;
	cJSON_AddItemToObject(root, "co2_eq_ppm_mean", co2_eq_ppm_mean);

	// Buffer generation
	if ( (buffer = cJSON_Print(root)) == NULL )
		ESP_LOGE(TAG, "Failed to print (allocate) JSON Object into buffer.");


json_failed:
	cJSON_Delete(root);
	return buffer;
}

static void sensor_co2_encode(
		QueueHandle_t transform_queue ,
		QueueHandle_t encode_queue    ,
		TaskHandle_t  control_task    )
{
	while (1)
	{

	}
}

static void app_sensor_co2_encode_task(void *args)
{
	(SensorEncodeTaskArgs_t *)args;
	QueueHandle_t transform_queue = args->transform_queue;
	QueueHandle_t encode_queue    = args->encode_queue;
	TaskHandle_t  control_task    = args->control_task;
	free(args);

	sensor_co2_encode(
		transform_queue ,
		encode_queue    ,
		control_task   	);
}