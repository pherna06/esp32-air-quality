#include "sgp30_i2c.h"

static const char *TAG = "SGP30_I2C";
#include "esp_log.h"

static uint16_t bytes_to_uint16_t(
		const uint8_t *bytes )
{
	ESP_LOGD(TAG, "Transforming 2 bytes to uint16_t.");

	return ((uint16_t) bytes[0] << 8) |
	       ((uint16_t) bytes[1])      ;
}

static uint32_t bytes_to_uint32_t(
		const uint8_t *bytes )
{
	ESP_LOGD(TAG, "Transforming 4 bytes to uint16_t.");

	return ((uint32_t) bytes[0] << 24) |
	       ((uint32_t) bytes[1] << 16) |
	       ((uint32_t) bytes[2] << 8)  |
	       ((uint32_t) bytes[3])       ;
}

static uint8_t generate_crc(
		const uint8_t *data  ,
		uint16_t       count )
{
	ESP_LOGD(TAG, "Generating CRC for data array of bytes.");

	uint16_t current_byte;
	uint8_t  crc = SGP30_CRC8_INIT;
	uint8_t  crc_bit;

	/* calculates 8-Bit checksum with given polynomial */
	for (current_byte = 0; current_byte < count; ++current_byte)
	{
		crc ^= (data[current_byte]);
		for (crc_bit = 8; crc_bit > 0; --crc_bit)
		{
			if (crc & 0x80)
				crc = (crc << 1) ^ SGP30_CRC8_POLYNOMIAL;
			else
				crc = (crc << 1);
		}
	}

	return crc;
}

static esp_err_t check_crc(
		const uint8_t *data     ,
		uint16_t       count    ,
		uint8_t        checksum )
{
	ESP_LOGD(TAG, "Checking checksum against data CRC.");

	if (generate_crc(data, count) != checksum)
	{
		ESP_LOGE(TAG, "Data checksum does not match with generated CRC.");
		return ESP_FAIL;
	}
		
	return ESP_OK;
}

static uint16_t fill_cmd_send_buf(
		uint8_t        *buf      ,
		uint16_t        cmd      ,
		const uint16_t *args     ,
		uint8_t         num_args )
{
	ESP_LOGD(TAG, "Generating buffer with command and checksumed args.");

	uint8_t crc;
	uint8_t i;
	uint16_t idx = 0;

	buf[idx++] = (uint8_t)((cmd & 0xFF00) >> 8);
	buf[idx++] = (uint8_t)((cmd & 0x00FF) >> 0);

	for (i = 0; i < num_args; ++i)
	{
		buf[idx++] = (uint8_t)((args[i] & 0xFF00) >> 8);
		buf[idx++] = (uint8_t)((args[i] & 0x00FF) >> 0);

		crc = generate_crc(
			(uint8_t *)(&buf[idx - 2]) ,
			SGP30_I2C_WORD_SIZE        );
		buf[idx++] = crc;
	}

	return idx;
}

esp_err_t sgp30_i2c_read_words_as_bytes(
		app_i2c_handle_t *i2c        ,
		uint8_t           address    ,
		uint8_t          *data       ,
		uint16_t          num_words  )
{
	ESP_LOGD(TAG,
		"Reading %d words (2 bytes) from SGP30 with I2C into byte array.",
		num_words
	);

	esp_err_t ret;

	uint16_t i, j;
	uint16_t size = num_words * (SGP30_I2C_WORD_SIZE + SGP30_CRC8_LEN);
	uint16_t word_buf[SGP30_I2C_MAX_BUFFER_WORDS];
	uint8_t* const buf8 = (uint8_t *)word_buf;

	ESP_LOGV(TAG, "Reading words from I2C comm.");
	ret = app_i2c_read(i2c, address, buf8, size);
	if (ret != ESP_OK)
		goto read_words_as_bytes_error;

	ESP_LOGV(TAG, "Checking checksum for each word.");
	for (i = 0, j = 0; i < size; i += SGP30_I2C_WORD_SIZE + SGP30_CRC8_LEN)
	{
		ret = check_crc(
			&buf8[i]                      ,
			SGP30_I2C_WORD_SIZE           ,
			buf8[i + SGP30_I2C_WORD_SIZE] );
		if (ret != ESP_OK)
			goto read_words_as_bytes_error;

		data[j++] = buf8[i];
		data[j++] = buf8[i + 1];
	}

	return ESP_OK;

read_words_as_bytes_error:
	ESP_LOGE(TAG, "Error while reading words as bytes from SGP30.");
	return ret;
}

esp_err_t sgp30_i2c_read_words(
		app_i2c_handle_t *i2c        ,
		uint8_t           address    ,
		uint16_t         *data_words ,
		uint16_t          num_words  )
{
	ESP_LOGD(TAG,
		"Reading %d words (2 bytes) from SGP30 with I2C.",
		num_words
	);

	esp_err_t ret;

	uint8_t i;

	ret = sgp30_i2c_read_words_as_bytes(
		i2c                   ,
		address               ,
		(uint8_t *)data_words ,
		num_words             );
	if (ret != ESP_OK)
		goto read_words_error;

	for (i = 0; i < num_words; ++i)
		data_words[i] = bytes_to_uint16_t( (uint8_t *)&data_words[i] );

	return ESP_OK;

read_words_error:
	ESP_LOGE(TAG, "Error while reading words from SGP30.");
	return ret;
}

esp_err_t sgp30_i2c_write_cmd(
		app_i2c_handle_t *i2c     ,
		uint8_t           address ,
		uint16_t          command )
{
	ESP_LOGD(TAG, "Writing command into SGP30.");

	uint8_t buf[SGP30_I2C_COMMAND_SIZE];

	fill_cmd_send_buf(
		buf     ,
		command ,
		NULL    ,
		0       );

	return app_i2c_write(
		i2c                    ,
		address                ,
		buf                    ,
		SGP30_I2C_COMMAND_SIZE );
}

esp_err_t sgp30_i2c_write_cmd_with_args(
		app_i2c_handle_t *i2c        ,
		uint8_t           address    ,
		uint16_t          command    ,
		const uint16_t   *data_words ,
		uint16_t          num_words  )
{
	ESP_LOGD(TAG, "Writing command with args into SGP30.");

	uint8_t buf[SGP30_I2C_MAX_BUFFER_WORDS];
	uint16_t buf_size;

	buf_size = fill_cmd_send_buf(
		buf        ,
		command    ,
		data_words ,
		num_words  );

	return app_i2c_write(
		i2c      ,
		address  ,
		buf      ,
		buf_size );
}

esp_err_t sgp30_i2c_delayed_read_cmd(
		app_i2c_handle_t *i2c        ,
		uint8_t           address    ,
		uint16_t          cmd        ,
		uint32_t          delay_ms   ,
		uint16_t         *data_words ,
		uint16_t          num_words  )
{
	ESP_LOGD(TAG, "Writing command and delayed reading response from SGP30.");

	int16_t ret;
	uint8_t buf[SGP30_I2C_COMMAND_SIZE];

	fill_cmd_send_buf(
		buf  ,
		cmd  ,
		NULL ,
		0    );

	ret = app_i2c_write(
		i2c                    ,
		address                ,
		buf                    ,
		SGP30_I2C_COMMAND_SIZE );
	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG, "Error while sending command to SGP30.");
		return ret;
	}

	if (delay_ms)
		app_i2c_ll_sleep(delay_ms);

	ret =  sgp30_i2c_read_words(
		i2c        ,
		address    ,
		data_words ,
		num_words  );
	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG, "Error while reading command response from SGP30.");
	}

	return ret;
}

esp_err_t sgp30_i2c_read_cmd(
		app_i2c_handle_t *i2c        ,
		uint8_t           address    ,
		uint16_t          cmd        ,
		uint16_t         *data_words ,
		uint16_t          num_words  )
{
	ESP_LOGD(TAG, "Writing command and reading response from SGP30.");

	return sgp30_i2c_delayed_read_cmd(
		i2c        ,
		address    ,
		cmd        ,
		0          ,
		data_words ,
		num_words  );
}

void sgp30_i2c_sleep(
		uint32_t ms )
{
	app_i2c_ll_sleep(ms);
}