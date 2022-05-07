#include "sgp30.h"
#include "si7021.h"
#include "defines.h"
#include "esp_err.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "math.h"

typedef struct {
	sgp30_handle_t *sgp30          ;
	QueueHandle_t   co2eq_queue    ;
	QueueHandle_t   tvoc_queue     ;
	QueueHandle_t   baseline_queue ;

	si7021_handle_t *si7021        ;
	QueueHandle_t    rh_queue      ;
	QueueHandle_t    celsius_queue ;

	TaskHandle_t   task ;
} app_sensor_handle_t;

esp_err_t app_sensor_init(
		app_sensor_handle_t *sensor );

esp_err_t app_sensor_start(
		app_sensor_handle_t *sensor );

esp_err_t app_sensor_stop(
		app_sensor_handle_t *sensor );

esp_err_t app_sensor_delete(
		app_sensor_handle_t * sensor );

esp_err_t app_sensor_read_tvoc(
		app_sensor_handle_t *sensor   ,
		uint16_t            *tvoc_ppb );

esp_err_t app_sensor_read_co2eq(
		app_sensor_handle_t *sensor    ,
		uint16_t            *co2eq_ppm );

esp_err_t app_sensor_read_baseline(
		app_sensor_handle_t *sensor   ,
		uint32_t            *baseline );

esp_err_t app_sensor_read_rh(
		app_sensor_handle_t *sensor     ,
		float               *rh_percent );

esp_err_t app_sensor_read_temperature(
		app_sensor_handle_t *sensor  ,
		float               *celsius );