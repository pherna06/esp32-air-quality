#ifndef __SI7021_H__
#define __SI7021_H__

#include "app_i2c.h"
#include "esp_err.h"


// ** SI7021 HANDLE LOGIC ** //

typedef struct {
	uint8_t   scl_gpio_pin;
	uint8_t   sda_gpio_pin;
} si7021_config_args_t;

typedef struct {
	char             *name ;
	uint8_t           address;
	app_i2c_handle_t *i2c;
} si7021_handle_t;

/**
 * @brief Creates an Si7021 handle with given configuration.
 * 
 * Configuration options include SCL/SDA GPIO pins and handle name.
 * 
 * Memory allocation! Handle should be deleted after use. See si7021_delete().
 * 
 * @param[in]  name   string with Si7021 name identification.
 * @param[in]  args   object with configuration parameters for Si7021 handle.
 * @param[out] si7021  handle generated with memory allocation.
 * 
 * @return ESP_OK on success, the produced error otherwise.
 */
esp_err_t si7021_create(
		char *name                   ,
		si7021_config_args_t *args   ,
		si7021_handle_t      *si7021 );

/**
 * @brief Deletes a Si7021 handle.
 * 
 * @param[in]  si7021  handle to be deleted.
 * 
 * @return ESP_OK on success, the produced error otherwise.
 */
esp_err_t si7021_delete(
		si7021_handle_t *si7021 );





// ** SI7021 COMMAND METHODS ** //

/**
 * @brief Sends a command 'reset', which restores the Si7021 sensor to a its
 *        default state.
 * 
 *        The only changes are made over the sensor user and heater registers.
 *        USER_REGISTER:   0b00111010 (0x3A)
 *        HEATER_REGISTER: 0b00000000 (0x00)
 * 
 * @param[in]  si7021  handle for the Si7021 sensor.
 * 
 * @return ESP_OK on success, the produced error otherwise.
 */
esp_err_t si7021_reset(
		si7021_handle_t *si7021 );

/**
 * @brief Sends a command 'measure_rh' and reads the measurement value
 *        given for relative humidity (not converted to %RH).
 * 
 *        A temperature measurement is also taken to apply compensation to the
 *        calculation of the humidity value. This value can be retrieved
 *        without issuing another measurement using 'si7021_measure_temperature
 *        from_previous_rh_and_read()'.
 * 
 * @param[in]   si7021   handle for the Si7021 sensor.
 * @param[out]  rh       measured value for relative humidity.
 * 
 * @return ESP_OK on success, the produced error otherwise.
 */
esp_err_t si7021_measure_rh_and_read(
		si7021_handle_t *si7021 ,
		uint16_t        *rh     );

/**
 * @brief Sends a command 'measure_temperature' and reads the measurement value
 *        given for temperature (not converted a specific temperature unit).
 * 
 * @param[in]   si7021       handle for the Si7021 sensor.
 * @param[out]  temperature  measured value for temperature.
 * 
 * @return ESP_OK on success, the produced error otherwise.
 */
esp_err_t si7021_measure_temperature_and_read(
		si7021_handle_t *si7021      ,
		uint16_t        *temperature );

/**
 * @brief Sends a command 'measure_temperature_from_previous_rh' and reads the
 *        temperature value generated for the calculation of relative humidity
 *        after a 'measure_rh' command.
 * 
 *        Using this method instead of 'si7021_measyre_temperature_and_read()'
 *        prevents the sensor from doing another measurement to retrieve a
 *        temperature value.
 * 
 * @param[in]   si7021       handle for the Si7021 sensor.
 * @param[out]  temperature  measured value for temperature.
 * 
 * @return ESP_OK on success, the produced error otherwise.
 */
esp_err_t si7021_measure_temperature_from_previous_rh_and_read(
		si7021_handle_t *si7021      ,
		uint16_t        *temperature );

/**
 * @brief Sends a command 'set_user_register' which stores a given 8bit value
 *        into the Si7021 user register.
 * 
 *        The register is of the form ABXX XCXD, where X bits are reserved and
 *        should not be modified. To achive this, method
 *        'si7021_get_user_register_and_read()' should be issued first to
 *        retrieve the value of the reserved registers.
 * 
 *        AD (Measurement resolution): modifies the precision in the humidity
 *        and temperature measurements.
 *        00 > 12 - 14 (default on reset)
 *        01 >  8 - 12
 *        10 > 10 - 13
 *        11 > 11 - 11
 * 
 *        B (Vdd Status): whether the Vdd input is in low state.
 *        0 > V_OK (default on reset)
 *        1 > V_LOW
 * 
 *        C (On-Chip Heater): whether the on-chip heater is enabled or not.
 *        0 > Disabled (default on reset)
 *        1 > Enabled
 * 
 * @param[in]  si7021    handle for the Si7021 sensor.
 * @param[in]  user_reg  new value for the Si7021 user register.
 * 
 * @return ESP_OK on success, the produced error otherwise.
 */
esp_err_t si7021_set_user_register(
		si7021_handle_t *si7021   ,
		uint8_t          user_reg );

/**
 * @brief Sends a command 'get_user_register' and reads the sent Si7021
 *        user register value.
 * 
 *        For more information on the register bits meaning, see method
 *        'si7021_set_user_register()'.
 * 
 * @param[in]   si7021    handle for the Si7021 sensor.
 * @param[out]  user_reg  read value from the Si7021 user register.
 * 
 * @return ESP_OK on success, the produced error otherwise.
 */
esp_err_t si7021_get_user_register_and_read(
		si7021_handle_t *si7021   ,
		uint8_t         *user_reg );

/**
 * @brief Sends a command 'set_heater_register' which stores a given 8bit value
 *        into the Si7021 heater register.
 * 
 *        The register is of the form XXXX ABCD, where X bits are reserved and
 *        should not be modified. To achive this, method
 *        'si7021_get_heater_register_and_read()' should be issued first to
 *        retrieve the value of the reserved registers.
 * 
 *        ABCD (Heater current): modifies the current of the heater, thus the
 *        heat it generates.
 *        0000 >  3.09 mA (default on reset)
 *        .... > linear growth
 *        1111 > 94.20 mA
 * 
 * @param[in]  si7021      handle for the Si7021 sensor.
 * @param[in]  heater_reg  new value for the Si7021 heater register.
 * 
 * @return ESP_OK on success, the produced error otherwise.
 */
esp_err_t si7021_set_heater_register(
		si7021_handle_t *si7021   ,
		uint8_t          heater_reg );

/**
 * @brief Sends a command 'get_heater_register' and reads the sent Si7021
 *        heaterregister value.
 * 
 *        For more information on the register bits meaning, see method
 *        'si7021_set_heater_register()'.
 * 
 * @param[in]   si7021    handle for the Si7021 sensor.
 * @param[out]  user_reg  read value from the Si7021 heater register.
 * 
 * @return ESP_OK on success, the produced error otherwise.
 */
esp_err_t si7021_get_heater_register_and_read(
		si7021_handle_t *si7021   ,
		uint8_t         *heater_reg );

/**
 * @brief Sends a command 'get_id_fst_access' and reads the serial bytes sent
 *        by the SI7021 sensor.
 * 
 * @param[in]   si7021  handle for the Si7021 sensor.
 * @param[out]  sna3    1st most significant serial byte
 * @param[out]  sna2    2nd most significant serial byte
 * @param[out]  sna1    3rd most significant serial byte
 * @param[out]  sna0    4th most significant serial byte
 * 
 * @return ESP_OK on success, the produced error otherwise.
 */
esp_err_t si7021_get_id_fst_access_and_read(
		si7021_handle_t *si7021 ,
		uint8_t         *sna3   ,
		uint8_t         *sna2   ,
		uint8_t         *sna1   ,
		uint8_t         *sna0   );

/**
 * @brief Sends a command 'get_id_snd_access' and reads the serial bytes sent
 *        by the SI7021 sensor.
 * 
 * @param[in]   si7021  handle for the Si7021 sensor.
 * @param[out]  snb3    5th most significant serial byte
 * @param[out]  snb2    6th most significant serial byte
 * @param[out]  snb1    7th most significant serial byte
 * @param[out]  snb0    8th most significant serial byte
 * 
 * @return ESP_OK on success, the produced error otherwise.
 */
esp_err_t si7021_get_id_snd_access_and_read(
		si7021_handle_t *si7021 ,
		uint8_t         *snb3   ,
		uint8_t         *snb2   ,
		uint8_t         *snb1   ,
		uint8_t         *snb0   );

/**
 * @brief Sends command 'get_firmware_revision' and reads the given Si7021
 *        firmware version.
 * 
 * @param[in]   si7021  handle for the Si7021 sensor.
 * @param[out]  fw_rev  firmware version read from the sensor.
 * 
 * @return ESP_OK on success, the produced error otherwise.
 */
esp_err_t si7021_get_firmware_revision_and_read(
		si7021_handle_t *si7021 ,
		uint8_t         *fw_rev );





// ** SI7021 ADVANCED METHODS ** //

/**
 * @brief Sends 'measure_rh' and 'measure_temperature_from_previous_rh'
 *        commands sequentially and calculates the percent relative humidity
 *        and the Celsius temperature from the read measurement values.
 * 
 *        The resulting values are in 'float' type.
 * 
 * @param[in]   si7021      handle for the Si7021 sensor.
 * @param[out]  rh_percent  %RH value.
 * @param[out]  celsius     ºC temperature.
 * 
 * @return ESP_OK on success, the produced error otherwise.
 */
esp_err_t si7021_measure_and_read_converted(
		si7021_handle_t *si7021     ,
		float           *rh_percent ,
		float           *celsius    );

/**
 * @brief Sends commands 'get_id_fst_access' and 'get_id_snd_access' to
 *        retrieve the Si7021 64bit serial number.
 * 
 * @param[in]   si7021  handle for the Si7021 sensor.
 * @param[out]  serial  serial number for the Si7021 sensor.
 * 
 * @return ESP_OK on success, the produced error otherwise.
 */
esp_err_t si7021_get_serial_number(
		si7021_handle_t *si7021 ,
		uint64_t        *serial );

// TODO


#endif