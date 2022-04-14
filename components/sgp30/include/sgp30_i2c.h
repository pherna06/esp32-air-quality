#ifndef __SGP30_I2C_H__
#define __SGP30_I2C_H__

#include "esp_err.h"
#include "app_i2c.h"

#define SGP30_CRC8_INIT       0xFF
#define SGP30_CRC8_POLYNOMIAL 0x31
#define SGP30_CRC8_LEN        1

#define SGP30_I2C_COMMAND_SIZE     2
#define SGP30_I2C_WORD_SIZE        2
#define SGP30_I2C_MAX_BUFFER_WORDS 32
#define SGP30_I2C_NUM_WORDS(x) (sizeof(x) / SGP30_I2C_WORD_SIZE)

#define SGP30_I2C_SCL_CLOCK_PERIOD_MS 2

/**
 * @brief Read data words as a byte stream from SGP30 address.
 * 
 * @param i2c[in]       SGP30 slave handle with configuration.
 * @param address[in]   to read from SGP30 I2C comm.
 * @param data[out]     allocated buffer to store the read bytes.
 * @param num_words[in] number of data words to read from the SGP30.
 * @return ESP_OK on success, an error otherwise.
 */
esp_err_t sgp30_i2c_read_words_as_bytes(
		app_i2c_handle_t *i2c        ,
		uint8_t           address    ,
		uint8_t          *data       ,
		uint16_t          num_words  );

/**
 * @brief Read data words from SGP30 address.
 * 
 * @param i2c[in]         SGP30 slave handle with configuration.
 * @param address[in]     to read from SGP30 I2C channel.
 * @param data_words[out] allocated buffer to store the read words.
 * @param num_words[in]   number of data words to read from the SGP30.
 * @return ESP_OK on success, an error otherwise.
 */
esp_err_t sgp30_i2c_read_words(
		app_i2c_handle_t *i2c        ,
		uint8_t           address    ,
		uint16_t         *data_words ,
		uint16_t          num_words  );

/**
 * @brief Write a command to the SGP30 address.
 * 
 * @param i2c[in]     SGP30 slave handle with configuration.
 * @param address[in] to write into SGP30 I2C channel.
 * @param command[in] SGP30 command code.
 * @return ESP_OK on success, an error otherwise.
 */
esp_err_t sgp30_i2c_write_cmd(
		app_i2c_handle_t *i2c     ,
		uint8_t           address ,
		uint16_t          command );

/**
 * @brief Write a command with arguments to the SGP30 address.
 * 
 * @param i2c[in]        SGP30 slave handle with configuration.
 * @param address[in]    to write into SGP30 I2C channel.
 * @param command[in]    SGP30 command code.
 * @param data_words[in] allocated buffer with words to send.
 * @param num_words[in]  number of data words to send.
 * @return ESP_OK on success, an error otherwise.
 */
esp_err_t sgp30_i2c_write_cmd_with_args(
		app_i2c_handle_t *i2c        ,
		uint8_t           address    ,
		uint16_t          command    ,
		const uint16_t   *data_words ,
		uint16_t          num_words  );

/**
 * @brief Send a command, wait for the SGP30 to process it and read data back.
 * 
 * @param i2c[in]         SGP30 slave handle with configuration.
 * @param address[in]     for SGP30 I2C channel.
 * @param command[in]     SGP30 command code.
 * @param delay_ms[in]    time in ms to delay sending the read request.
 * @param data_words[out] allocated buffer to store the read data.
 * @param num_words[in]   number of data words to read.
 * @return ESP_OK on success, an error otherwise.
 */
esp_err_t sgp30_i2c_delayed_read_cmd(
		app_i2c_handle_t *i2c        ,
		uint8_t           address    ,
		uint16_t          cmd        ,
		uint32_t          delay_ms   ,
		uint16_t         *data_words ,
		uint16_t          num_words  );

/**
 * @brief Send a command and read data back.
 * 
 * @param i2c[in]         SGP30 slave handle with configuration.
 * @param address[in]     for SGP30 I2C channel.
 * @param command[in]     SGP30 command code.
 * @param data_words[out] allocated buffer to store the read data.
 * @param num_words[in]   number of data words to read.
 * @return ESP_OK on success, an error otherwise.
 */
esp_err_t sgp30_i2c_read_cmd(
		app_i2c_handle_t *i2c        ,
		uint8_t           address    ,
		uint16_t          cmd        ,
		uint16_t         *data_words ,
		uint16_t          num_words  );

void sgp30_i2c_sleep(
		uint32_t ms );

#endif