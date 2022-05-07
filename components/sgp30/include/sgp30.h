#ifndef __SGP30_H__
#define __SGP30_H__

#include "app_i2c.h"
#include "esp_err.h"


typedef struct {
	uint8_t   scl_gpio_pin;
	uint8_t   sda_gpio_pin;
} sgp30_config_args_t;

typedef struct {
	char             *name ;
	uint8_t           address;
	app_i2c_handle_t *i2c;
} sgp30_handle_t;

/**
 * @brief Creates an SGP30 handle with given configuration.
 * 
 * Configuration options include SCL/SDA GPIO pins and handle name.
 * 
 * Memory allocation! Handle should be deleted after use. See sgp30_delete().
 * 
 * @param[in]  name  string with SGP30 name identification.
 * @param[in]  args  object with configuration parameters for SGO30 handle.
 * @param[out] sgp30 handle generated with memory allocation.
 * 
 * @return ESP_OK on success, the produced error otherwise.
 */
esp_err_t sgp30_create(
		char *name                 ,
		sgp30_config_args_t *args  ,
		sgp30_handle_t      *sgp30 );

/**
 * @brief Deletes a SGP30 handle.
 * 
 * @param[in]  sgp30  handle to be deleted.
 * 
 * @return ESP_OK on success, the produced error otherwise.
 */
esp_err_t sgp30_delete(
		sgp30_handle_t *sgp30 );


/**
 * @brief Sends a command 'iaq_init', which initializes the air quality
 *        measurement.
 * 
 *        After sending this command, measurement commands 'measure_iaq' must
 *        be issued in regular intervals of 1 second, to ensure proper
 *        operation of the SGP30 internal dynamic baseline compensation
 *        algorithm.
 * 
 *        For the first 15 seconds after the 'iaq_init' command has been sent,
 *        the sensor is in an initizalization phase during which measuring with
 *        'measure_iaq' returns fixed values of 400 ppm CO2eq and 0 ppb TVOC.
 * 
 *        Moreover, after the 'iaq_init' command, the IAQ baseline should be
 *        initialized using 'set_iaq_baseline' if the user has a valid baseline
 *        value available. Otherwise, the sensor will enter an 'early operation
 *        phase' in which a valid baseline value will be retrievable after 12
 *        hours of measuring operation.
 * 
 *        Whether an initial baseline value was set or a new one was retrieved
 *        after an 'early operation phase', the baseline value must be
 *        retrieved with 'get_iaq_baseline' every hour of operation and saved
 *        persisntently for future sensor initializations.
 * 
 * @param[in]  sgp30  handle for the SGP30 sensor.
 * 
 * @return ESP_OK on success, the produced error otherwise.
 */
esp_err_t sgp30_iaq_init(
		sgp30_handle_t *sgp30 );

/**
 * @brief Sends a command 'measure_iaq' and reads the measuremnet values
 *        sent by the SGP30 (CO2eq and TVOC).
 * 
 *        An 'iaq_init' command should be issued prior to requesting any
 *        measuring; and measurements should be issued in regular intervals
 *        of 1 second to ensure proper operation of the SGP30 internal dynamic
 *        baselina compensation algorithm.
 * 
 * @param[in]   sgp30   handle for the SGP30 sensor.
 * @param[out]  tvoc    measured TVOC value in ppb.
 * @param[out]  co2_eq  measured CO2eq value in ppm.
 * 
 * @return ESP_OK on success, the produced error otherwise.
 */
esp_err_t sgp30_measure_iaq_and_read(
		sgp30_handle_t *sgp30         ,
		uint16_t       *tvoc          ,
		uint16_t       *co2_eq        );

/**
 * @brief Sends a command 'get_iaq_baseline' and reads the IAQ baseline value
 *        sent by the SGP30.
 * 
 *        During functioning of the sensor, this command should be issued every
 *        hour to retrieve a valid baseline value, which should be stored for
 *        future sensor initializations.
 * 
 *        This command should not be used if no valid baseline value has been
 *        restored before, either by setting a baseline value after
 *        initialization or by waiting for baseline restoration after an 'early
 *        operation phase'.
 * 
 * @param[in]   sgp30     handle for the SGP30 sensor.
 * @param[out]  baseline  retreived value for IAQ basline.
 * 
 * @return ESP_OK on success, the produced error otherwise.
 */
esp_err_t sgp30_get_iaq_baseline_and_read(
		sgp30_handle_t *sgp30         ,
		uint32_t       *baseline      );

/**
 * @brief Sends a command 'set_iaq_baseline' which sets the IAQ baseline value
 *        to be used by the sensor for measurements calculations.
 * 
 *        This command should be used after an 'iaq_init' command to set a
 *        valid IAQ baseline value (less than 1 week old).
 * 
 * @param[in]  sgp30     handle for the SGP30 sensor.
 * @param[in]  baseline  baseline value to be set in the SGP30 sensor.
 * 
 * @return ESP_OK on success, the produced error otherwise.
 */
esp_err_t sgp30_set_iaq_baseline(
		sgp30_handle_t *sgp30         ,
		uint32_t        baseline      );

/**
 * @brief Sends a command 'set_absolute_humidity' which sets an absolute
 *        humidity value to be used for compensation in the measurements
 *        calculations.
 * 
 *        After sending a new humidity value, this value will be used by the
 *        compensation algorithm until a new humidity value is set again.
 * 
 *        Restarting the sensor or sending a value of 0 disables the humidity
 *        compensation until a new value is set again.
 * 
 * @param[in]  sgp30     handle for the SGP30 sensor.
 * @param[in]  humidity  absolute humidity value to be set in the SGP30 sensor.
 * 
 * @return ESP_OK on success, the produced error otherwise.
 */
esp_err_t sgp30_set_absolute_humidity(
		sgp30_handle_t *sgp30         ,
		uint16_t        humidity      );

/**
 * @brief Sends a 'measure_test' command, which runs an on-chip test used for
 *        testing the proper functionality of the sensor.
 * 
 *        This command should not be used after an 'iaq_init' command has been
 *        issued. Besides, after the test, an 'iaq_init' command is needed to
 *        resume IAQ operations.
 * 
 *        In case of a successful test, the sensor returns the fixed data
 *        pattern 0xD400.
 * 
 * @param[in]  sgp30  handle for the SGP30 sensor.
 * 
 * @return ESP_OK on success, the produced error otherwise.
 */
esp_err_t sgp30_measure_test(
		sgp30_handle_t *sgp30 );

/**
 * @brief Sends a 'get_feature_set' command, which retrieves the product type
 *        (SGP30) and version number.
 * 
 * @param[in]   sgp30    handle for the SGP30 sensor.
 * @param[out]  type     value determining the device type.
 * @param[out]  version  number of the device version.
 * 
 * @return ESP_OK on success, the produced error otherwise.
 */
esp_err_t sgp30_get_feature_set_and_read(
		sgp30_handle_t *sgp30         ,
		uint8_t        *type          ,
		uint8_t        *version       );

/**
 * @brief Sends a 'measure_raw' command and reads the measured raw values used
 *        in IAQ calculations (H2 and ethanol concentrations).
 * 
 * @param[in]   sgp30    handle for the SGP30 sensor.
 * @param[out]  h2       measured raw value for H2 concentration.
 * @param[out]  ethanol  measured raw value for ethanol concentration.
 * 
 * @return ESP_OK on success, the produced error otherwise.
 */
esp_err_t sgp30_measure_raw_and_read(
		sgp30_handle_t *sgp30         ,
		uint16_t      *h2            ,
		uint16_t      *ethanol       );

/**
 * @brief Sends a 'get_tvoc_inceptive_baseline' command and reads the retrieved
 *        TVOC baseline value.
 * 
 *        This baseline value offers a calibrated starting reference to the
 *        dynamic baseline compensation algorithm for first start-up period of
 *        the SGP30 sensor.
 * 
 * @param[in]   sgp30     handle for the SGP30 sensor.
 * @param[out]  baseline  value for the TVOC baseline.
 * 
 * @return ESP_OK on sucess, the produced error otherwise.
 */
esp_err_t sgp30_get_tvoc_inceptive_baseline_and_read(
		sgp30_handle_t *sgp30         ,
		uint16_t      *baseline      );

/**
 * @brief Sends a 'set_tvoc_baseline' command, which sets a value for the TVOC
 *        baseline.
 * 
 *        This command should only be used when activating the inceptive
 *        baseline, which should be used only for the very first start-up of
 *        the SGP30 sensor.
 * 
 * @param[in]  sgp30     handle for the SGP30 sensor.
 * @param[in]  baseline  TVOC baseline value to be set in the SGP30 sensor.
 * 
 * @return ESP_OK on success, the produced error otherwise.
 */
esp_err_t sgp30_set_tvoc_baseline(
		sgp30_handle_t *sgp30         ,
		uint16_t        baseline      );

#endif