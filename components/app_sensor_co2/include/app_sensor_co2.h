typedef struct
{
	uint32_t      sample_freq;  // Frequency (in ms) to sample sensor.
	QueueHandle_t sample_queue; // Queue to send raw samples.

	TaskHandle_t  control_task; // Creator task
} SensorSampleTaskArgs_t;

typedef struct
{
	uint8_t       sample_num;   // No. of samples for transform logic.
	uint32_t      sample_wait;  // Max time to wait for a raw sample.
	QueueHandle_t sample_queue; // Queue to receive raw samples.

	QueueHandle_t transform_queue; // Queue to send transformed data.

	TaskHandle_t  control_task; // Creator task.
} SensorTransformTaskArgs_t;

typedef struct
{
	QueueHandle_t transform_queue; // Queue to receive transformed data.

	QueueHandle_t encode_queue; // Queue to send encoded (buffer) data.

	TaskHandle_t  control_task; // Creator task.
} SensorEncodeTaskArgs_t;

typedef struct
{
	time_t   timestamp;
	uint16_t co2_eq_ppm;
} SensorCO2Sample_t;

typedef struct
{
	time_t   timestamp;
	float    co2_eq_ppm_mean;
} SensorCO2TransformedSample_t;

static void app_sensor_co2_sample_task(void *args);
static void app_sensor_co2_transform_task(void *args);
static void app_sensor_co2_encode_task(void *args);

