#include "esp_err.h"
#include "sgp30_i2c.h"

typedef struct {
	app_i2c_handle_t i2c;
} sgp30_handle_t;



/**
 * @brief Creates an SGP30 handle with given configuration.
 * 
 * Configuration options for SCL/SDA GPIO pins and name.
 * 
 * Memory allocation! Handle should be deleted after use. See sgp30_delete().
 * 
 * @param[in]  name  string with SGP30 name identification.
 * @param[in]  scl   number for SCL GPIO pin.
 * @param[in]  sda   number for SDA GPIO pin.
 * @param[out] sgp30 handle generated with memory allocation.
 * 
 * @return ESP_OK on success.
 */
esp_err_t sgp30_create(
		const char     *name  ,
		uint8_t         scl   ,
		uint8_t         sda   ,
		sgp30_handle_t *sgp30 );

/**
 * @brief Deletes an SGP30handle.
 * 
 * @param[out] sgp30 SGP30 handle to be deleted
 * 
 * @return ESP_OK on success.
 */
esp_err_t sgp30_delete(
		sgp30_handle_t *sgp30 );


// TODO doc
esp_err_t sgp30_measure_test(
		sgp30_handle_t *sgp30       ,
		uint16_t       *test_result );

esp_err_t sgp30_measure_iaq(
		sgp30_handle_t *sgp30 );

esp_err_t sgp30_read_iaq(
		sgp30_handle_t *sgp30      ,
		uint16_t       *tvoc_ppc   ,
		uint16_t       *co2_eq_ppn );

esp_err_t sgp30_measure_iaq_blocking_read(
		sgp30_handle_t *sgp30      ,
		uint16_t       *tvoc_ppb   ,
		uint16_t       *co2_eq_ppm );

esp_err_t sgp30_measure_tvoc(
		sgp30_handle_t *sgp30 );

esp_err_t sgp30_measure_co2_eq(
		sgp30_handle_t *sgp30 );

esp_err_t sgp30_read_tvoc(
		sgp30_handle_t *sgp30    ,
		uint16_t       *tvoc_ppb );

esp_err_t sgp30_read_co2_eq(
		sgp30_handle_t *sgp30      ,
		uint16_t       *co2_eq_ppm );

esp_err_t sgp30_measure_tvoc_blocking_read(
		sgp30_handle_t *sgp30    ,
		uint16_t       *tvoc_ppb );

esp_err_t sgp30_measure_co2_eq_blocking_read(
		sgp30_handle_t *sgp30      ,
		uint16_t       *co2_eq_ppm );

esp_err_t sgp30_measure_raw(
		sgp30_handle_t *sgp30 );

esp_err_t sgp30_read_raw(
		sgp30_handle_t *sgp30              ,
		uint16_t       *ethanol_raw_signal ,
		uint16_t       *h2_raw_signal      );

esp_err_t sgp30_measure_raw_blocking_read(
		sgp30_handle_t *sgp30              ,
		uint16_t       *ethanol_raw_signal ,
		uint16_t       *h2_raw_signal      );

esp_err_t sgp30_get_iaq_baseline(
		sgp30_handle_t *sgp30    ,
		uint32_t       *baseline );

esp_err_t sgp30_set_iaq_baseline(
		sgp30_handle_t *sgp30    ,
		uint32_t        baseline );

esp_err_t sgp30_get_tvoc_inceptive_baseline(
		sgp30_handle_t *sgp30                   ,
		uint16_t       *tvoc_inceptive_baseline );

esp_err_t sgp30_set_tvoc_baseline(
		sgp30_handle_t *sgp30         ,
		uint16_t        tvoc_baseline );

esp_err_t sgp30_set_absolute_humidity(
		sgp30_handle_t *sgp30             ,
		uint32_t        absolute_humidity );

esp_err_t sgp30_iaq_init(
		sgp30_handle_t *sgp30 );

esp_err_t sgp30_probe(
		sgp30_handle_t *sgp30 );

uint8_t sgp30_get_configured_address();