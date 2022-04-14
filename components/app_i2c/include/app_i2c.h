#ifndef __APP_I2C_UTILS_H__
#define __APP_I2C_UTILS_H__

#include "esp_err.h"

/// LOW LEVEL METHODS ///

/**
 * @brief Resets GPIO logic in corresponding SCL/SDA GPIO pins.
 * 
 * Used for SCL/SDA 'initialization'.
 * 
 * @param[in] scl GPIO number for SCL pin.
 * @param[in] sda GPIO number for SDA pin.
 * 
 * @return ESP_OK (always successful)
 */
esp_err_t app_i2c_ll_init_pins(
		uint8_t scl ,
		uint8_t sda );

/**
 * @brief Resets GPIO logic in corresponding SCL/SDA GPIO pins.
 * 
 * Used for SCL/SDA 'release'.
 * 
 * @param[in] scl GPIO number for SCL pin.
 * @param[in] sda GPIO number for SDA pin.
 * 
 * @return ESP_OK (always successful)
 */
esp_err_t app_i2c_ll_release_pins(
		uint8_t scl ,
		uint8_t sda );

/**
 * @brief Sets SDA pin as GPIO input with internal pull-up.
 * 
 * @param[in] sda GPIO number for SDA pin.
 * 
 * @return ESP_OK if successful.
 * @return ESP_ERR_INVALID_ARG if invalid GPIO number.
 */
esp_err_t app_i2c_ll_SDA_in(
		uint8_t sda );

/**
 * @brief Sets SDA pin as GPIO outputw with low level.
 * 
 * @param[in] sda GPIO number for SDA pin.
 * 
 * @return ESP_OK if successful.
 * @return ESP_ERR_INVALID_ARG if invalid GPIO number.
 */
esp_err_t app_i2c_ll_SDA_out(
		uint8_t sda );

/**
 * @brief Read the value of SDA pin.
 * 
 * SDA pin must be GPIO output for proper functionality.
 * 
 * @param[in]  sda   GPIO number for SDA pin.
 * @param[out] level 0 if pin is low. 1 otherwise.
 * 
 * @return ESP_OK if successful.
 * @return ESP_ERR_INVALID_ARG if invalid GPIO number.
 */
esp_err_t app_i2c_ll_SDA_read(
		uint8_t  sda   ,
		uint8_t *level );

/**
 * @brief Sets SCL pin as GPIO input with internal pull-up.
 * 
 * @param[in] scl GPIO number for SCL pin.
 * 
 * @return ESP_OK if successful.
 * @return ESP_ERR_INVALID_ARG if invalid GPIO number.
 */
esp_err_t app_i2c_ll_SCL_in(
		uint8_t scl );

/**
 * @brief Sets SCL pin as GPIO outputw with low level.
 * 
 * @param[in] scl GPIO number for SCL pin.
 * 
 * @return ESP_OK if successful.
 * @return ESP_ERR_INVALID_ARG if invalid GPIO number.
 */
esp_err_t app_i2c_ll_SCL_out(
		uint8_t scl );

/**
 * @brief Read the value of SCL pin.
 * 
 * SCL pin must be GPIO output for proper functionality.
 * 
 * @param[in]  scl   GPIO number for SCL pin.
 * @param[out] level 0 if pin is low. 1 otherwise.
 * 
 * @return ESP_OK if successful.
 * @return ESP_ERR_INVALID_ARG if invalid GPIO number.
 */
esp_err_t app_i2c_ll_SCL_read(
		uint8_t  scl   ,
		uint8_t *level );

/**
 * @brief Sleep for given time.
 * 
 * @param ms time to sleep in miliseconds.
 */
void app_i2c_ll_sleep(
		uint32_t ms );





/// HIGH LEVEL METHODS ///

typedef struct {
	uint8_t   scl           ;
	uint8_t   sda           ;
	uint32_t  scl_period_ms ;
	uint32_t  op_delay_ms   ; // half SCL period
} app_i2c_config_args_t;

typedef struct {
	char      slave[128]    ;
	uint8_t   scl           ;
	uint8_t   sda           ;
	uint32_t  scl_period_ms ;
	uint32_t  op_delay_ms   ; // half SCL period
} app_i2c_handle_t;

/**
 * @brief Creates an I2C handle with given slave configuration.
 * 
 * Configuration options for SCL/SDA GPIO pins, SCL clock period and low level
 * op delay (should be period / 2).
 * 
 * Memory allocation! Handle should be deleted after use. See app_i2c_delete().
 * 
 * @param[in]  slave string with slave name identification.
 * @param[in]  args  struct with configuration data.
 * @param[out] i2c   handle generated with memory allocation.
 * 
 * @return ESP_OK (always successful)
 */
esp_err_t app_i2c_create(
		const char            *slave ,
		app_i2c_config_args_t *args  ,
		app_i2c_handle_t      *i2c   );

/**
 * @brief Deletes an I2C slave handle.
 * 
 * @param[out] i2c   slave handle to be deleted
 * 
 * @return ESP_OK (always successful)
 */
esp_err_t app_i2c_delete(
		app_i2c_handle_t *i2c );

/**
 * @brief Initializes the needed resources for future I2C communication.
 * 
 * Aborts app if I2C low-level error.
 * 
 * @param[in] i2c slave handle to perform initialization.
 * 
 * @return ESP_OK (always successful if no abort)
 */
esp_err_t app_i2c_init(
		app_i2c_handle_t *i2c );

/**
 * @brief Releases the allocated resources used in I2C communication.
 * 
 * Aborts app if I2C low-level error.
 * 
 * @param[in] i2c slave handle to perform resource release.
 * 
 * @return ESP_OK (always successful if no abort).
 */
esp_err_t app_i2c_release(
		app_i2c_handle_t *i2c );

/**
 * @brief Executes one write transaction on the I2C bus, sending a given number
 *        of bytes.
 * 
 * Aborts app if I2C low-level error.
 * 
 * @param[in] i2c     slave handle for I2C operation.
 * @param[in] address 7-bit I2C address for slave.
 * @param[in] data    pointer to buffer containing the bytes to write.
 * @param[in] count   number of bytes to read from the buffer and send to slave.
 * 
 * @return ESP_OK on success.
 * @return Error otherwise.
 */
esp_err_t app_i2c_write(
		app_i2c_handle_t *i2c     ,
		uint8_t           address ,
		uint8_t const    *data    ,
		uint16_t          count   );

/**
 * @brief Executes one read transaction on the I2C bus, reading a number of
 *        bytes.
 * 
 * Aborts app if I2C low-level error.
 * 
 * @param[in] i2c     slave handle for I2C operation.
 * @param[in] address 7-bit I2C address for slave.
 * @param[in] data    pointer to buffer where read data will be stored.
 * @param[in] count   number of bytes to read from the slave into the buffer.
 * 
 * @return ESP_OK on success.
 * @return Error otherwise.
 */
esp_err_t app_i2c_read(
		app_i2c_handle_t *i2c     ,
		uint8_t           address ,
		uint8_t          *data    ,
		uint16_t          count   );

#endif