#include "si7021.h"

#include "esp_log.h"
static const char *TAG = "Si7021";

#include "string.h"

// ** SI7021 HANDLE LOGIC ** //

#define SI7021_NAME_SIZE          128
#define SI7021_I2C_ADDRESS        0x40
#define SI7021_I2C_NAME           "si7021_i2c"
#define SI7021_I2C_SCL_PERIOD_MS  2
#define SI7021_I2C_OP_DELAY_MS    (SI7021_I2C_SCL_PERIOD_MS / 2)

esp_err_t si7021_create(
		char *name                   ,
		si7021_config_args_t *args   ,
		si7021_handle_t      *si7021 )
{
	ESP_LOGD(TAG, "Creating Si7021 handle.");

	size_t len = strnlen(name, SI7021_NAME_SIZE);
	if (len == SI7021_NAME_SIZE)
	{
		ESP_LOGE(TAG,
			"Si7021 handle name string length must be under %d characters.",
			SI7021_NAME_SIZE
		);

		return ESP_ERR_INVALID_ARG;
	}

	ESP_LOGV(TAG, "Loading Si7021 handle name \"%.*s\".", len, name);
	si7021->name = malloc(sizeof(char) * SI7021_NAME_SIZE);
	strcpy(si7021->name, name);

	si7021->address = SI7021_I2C_ADDRESS;

	ESP_LOGV(TAG, "Loading Si7021 I2C configuration.");
	app_i2c_config_args_t i2c_args = {
		.scl = args->scl_gpio_pin                 ,
		.sda = args->sda_gpio_pin                 ,
		.scl_period_ms = SI7021_I2C_SCL_PERIOD_MS ,
		.op_delay_ms   = SI7021_I2C_OP_DELAY_MS   };

	esp_err_t ret;
	app_i2c_handle_t *i2c = malloc(sizeof(app_i2c_handle_t));
	
	ret = app_i2c_create(
			SI7021_I2C_NAME ,
			&i2c_args      ,
			i2c            );
	if (ret != ESP_OK)
		return ret;

	si7021->i2c = i2c;

	return ESP_OK;
}

esp_err_t si7021_delete(
		si7021_handle_t *si7021 )
{
	ESP_LOGD(TAG,
		"Destroying Si7021 handle \"%.*s\".",
		SI7021_NAME_SIZE, si7021->name
	);

	free(si7021->name);
	
	esp_err_t ret;
	ret = app_i2c_delete(si7021->i2c);
	if (ret != ESP_OK)
		return ret;

	return ESP_OK;
}

// *** *** //





// ** SI7021 I2C BASIC METHODS ** //

#define SI7021_I2C_CRC8_POLY  0x31 // x^8 + x^5 + x^4 + 1
#define SI7021_I2C_CRC8_INIT  0x00
#define SI7021_I2C_CRC8_XOR   0x00

static uint8_t crc8_checksum_calculate(
		uint8_t  *bytes ,
		uint16_t  count )
{
	ESP_LOGD(TAG, "Calculating CRC8 checksum for %d bytes.", count);

	uint8_t crc8 = SI7021_I2C_CRC8_INIT;
	uint16_t i, j;
	for (i = 0; i < count; ++i)
	{
		crc8 ^= bytes[i];
		for (j = 0; j < 8; ++j)
		{
			if (crc8 & 0x80)
				crc8 = (crc8 << 1) ^ SI7021_I2C_CRC8_POLY;
			else
				crc8 = (crc8 << 1);
		}
		crc8 ^= SI7021_I2C_CRC8_XOR;
	}

	return crc8;
}

static void si7021_sleep_ms(
		uint16_t ms )
{
	app_i2c_ll_sleep(ms);
}

static esp_err_t si7021_i2c_checksum_check(
		uint8_t  *bytes    ,
		uint16_t  count    ,
		uint8_t   checksum )
{
	ESP_LOGD(TAG, "Checking checksum correctness.");

	uint8_t crc8 = crc8_checksum_calculate(bytes, count);
	if (crc8 != checksum)
	{
		ESP_LOGE(TAG, "Data checksum does not match with correspondant CRC8.");
		return ESP_FAIL;
	}

	return ESP_OK;
}

static esp_err_t si7021_i2c_send_command(
		app_i2c_handle_t *i2c     ,
		uint8_t           address ,
		uint8_t           command )
{
	ESP_LOGD(TAG, "Sending command to device.");

	esp_err_t ret;
	ret = app_i2c_write(i2c, address, &command, 1);
	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG,
			"Error while sending command %X to device address %X.",
			command,
			address
		);
		return ret;
	}

	return ESP_OK;
}

static esp_err_t si7021_i2c_send_command_long(
		app_i2c_handle_t *i2c     ,
		uint8_t           address ,
		uint16_t          command )
{
	ESP_LOGD(TAG, "Sending command to device.");

	uint16_t count = 2;
	uint8_t  buf[count];

	buf[0] = (uint8_t) (command >> 8 );
	buf[1] = (uint8_t) (command & 0x00FF);

	esp_err_t ret;
	ret = app_i2c_write(i2c, address, buf, count);
	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG,
			"Error while sending command %X to device address %X.",
			command,
			address
		);
		return ret;
	}

	return ESP_OK;
}

static esp_err_t si7021_i2c_send_command_with_data(
		app_i2c_handle_t *i2c           ,
		uint8_t           address       ,
		uint8_t           command       ,
		uint8_t          *data          ,
		uint8_t           num_data      ,
		uint8_t           checksum_flag )
{
	ESP_LOGD(TAG, "Sending command with args to device.");

	uint16_t count = 1 + num_data;
	if (checksum_flag)
		count += num_data;

	uint8_t  buf[count];
	uint16_t i = 0;

	buf[i++] = command;

	uint16_t j;
	for (j = 0; j < num_data; ++j)
	{
		buf[i++] = data[j];
		if (checksum_flag)
			buf[i++] = crc8_checksum_calculate(data + j, 1);
	}

	esp_err_t ret;
	ret = app_i2c_write(i2c, address, buf, count);
	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG,
			"Error while sending command with args %X to device address %X.",
			command,
			address
		);
		return ret;
	}

	return ESP_OK;
}

static esp_err_t si7021_i2c_read(
		app_i2c_handle_t *i2c           ,
		uint8_t           address       ,
		uint8_t          *data          ,
		uint8_t           num_data      ,
		uint8_t           checksum_flag )
{
	ESP_LOGD(TAG, "Reading from device.");

	uint16_t count = num_data;
	if (checksum_flag)
		count += num_data;
	uint8_t buf[count];

	esp_err_t ret;
	ret = app_i2c_read(i2c, address, buf, count);
	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG,
			"Error while reading from device address %X.",
			address
		);
		return ret;
	}

	uint16_t j;
	for (j = 0; j < num_data; ++j)
	{
		if (checksum_flag)
		{
			ret = si7021_i2c_checksum_check(&buf[j], 1, buf[j + 1]);
			if (ret != ESP_OK)
			{
				ESP_LOGE(TAG, "Error in checksum of read data.");
				return ret;
			}
		}

		data[j] = buf[j];
	}

	return ESP_OK;
}

static esp_err_t si7021_i2c_read_long(
		app_i2c_handle_t *i2c           ,
		uint8_t           address       ,
		uint16_t         *data          ,
		uint16_t          num_data      ,
		uint8_t           checksum_flag )
{
	ESP_LOGD(TAG, "Reading from device.");

	uint16_t count = num_data * 2;
	if (checksum_flag)
		count += num_data;
	uint8_t buf[count];

	esp_err_t ret;
	ret = app_i2c_read(i2c, address, buf, count);
	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG,
			"Error while reading from device address %X.",
			address
		);
		return ret;
	}

	uint16_t j;
	for (j = 0; j < num_data; ++j)
	{
		if (checksum_flag)
		{
			uint8_t *data_buf = buf + 3*j;
			ret = si7021_i2c_checksum_check(data_buf, 2, data_buf[2]);
			if (ret != ESP_OK)
			{
				ESP_LOGE(TAG, "Error in checksum of read data.");
				return ret;
			}
		}

		data[j] = ( (uint16_t) buf[j] << 8) | ( (uint16_t) buf[j + 1] );
	}

	return ESP_OK;
}

// *** *** //





// ** SI7021 COMMAND METHODS ** //

#define SI7021_I2C_CMD_RESET                              0xFE
#define SI7021_I2C_CMD_MEASURE_RH                         0xE5
#define SI7021_I2C_CMD_MEASURE_TEMPERATURE                0xE3
#define SI7021_I2C_CMD_READ_TEMPERATURE_FROM_PREVIOUS_RH  0xE0
#define SI7021_I2C_CMD_SET_USER_REGISTER                  0xE6
#define SI7021_I2C_CMD_GET_USER_REGISTER                  0xE7
#define SI7021_I2C_CMD_SET_HEATER_REGISTER                0x51
#define SI7021_I2C_CMD_GET_HEATER_REGISTER                0x11
#define SI7021_I2C_CMD_GET_ID_FST_ACCESS                  0xFA0F
#define SI7021_I2C_CMD_GET_ID_SND_ACCESS                  0xFCC9
#define SI7021_I2C_CMD_GET_FIRMWARE_REVISION              0x84B8

// unused commands (clock stretching instead of NACK if measurement not ready)
// #define SI7021_I2C_CMD_MEASURE_TEMPERATURE_HOLD 0xF3
// #define SI7021_I2C_CMD_MEASURE_RH_HOLD 0xF5

#define SI7021_I2C_WAIT_MS_RESET                  30 // 15 * 2
#define SI7021_I2C_WAIT_MS_MEASURE_RH             24 // 12 * 2
#define SI7021_I2C_WAIT_MS_MEASURE_TEMPERATURE    22 // 10.8 * 2
#define SI7021_I2C_WAIT_MS_SET_USER_REGISTER      10
#define SI7021_I2C_WAIT_MS_GET_USER_REGISTER      10
#define SI7021_I2C_WAIT_MS_SET_HEATER_REGISTER    10
#define SI7021_I2C_WAIT_MS_GET_HEATER_REGISTER    10
#define SI7021_I2C_WAIT_MS_GET_ID_FST_ACCESS      10
#define SI7021_I2C_WAIT_MS_GET_ID_SND_ACCESS      10
#define SI7021_I2C_WAIT_MS_GET_FIRMWARE_REVISION  10

esp_err_t si7021_reset(
		si7021_handle_t *si7021 )
{
	esp_err_t ret;
	ret = si7021_i2c_send_command(
		si7021->i2c          ,
		si7021->address      ,
		SI7021_I2C_CMD_RESET );

	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG, "Error with command 'reset'.");
		return ret;
	}

	si7021_sleep_ms(SI7021_I2C_WAIT_MS_RESET);

	return ESP_OK;
}

static esp_err_t si7021_measure_rh(
		si7021_handle_t *si7021 )
{
	esp_err_t ret;
	ret = si7021_i2c_send_command(
		si7021->i2c               ,
		si7021->address           ,
		SI7021_I2C_CMD_MEASURE_RH );

	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG, "Error with command 'measure_rh'.");
		return ret;
	}

	return ESP_OK;
}

static esp_err_t si7021_read_measure_rh(
		si7021_handle_t *si7021 ,
		uint16_t        *rh     )
{
	esp_err_t ret;

	ret = si7021_i2c_read_long(
		si7021->i2c     ,
		si7021->address ,
		rh              ,
		1               ,
		1               );
	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG, "Error reading after command 'measure_iaq'.");
		return ret;
	}

	return ESP_OK;
}

esp_err_t si7021_measure_rh_and_read(
		si7021_handle_t *si7021 ,
		uint16_t        *rh     )
{
	esp_err_t ret;
	ret = si7021_measure_rh(si7021);
	if (ret != ESP_OK)
		return ret;

	si7021_sleep_ms(SI7021_I2C_WAIT_MS_MEASURE_RH);

	ret = si7021_read_measure_rh(si7021, rh);
	if (ret != ESP_OK)
		return ret;

	return ESP_OK;
}

static esp_err_t si7021_measure_temperature(
		si7021_handle_t *si7021 )
{
	esp_err_t ret;
	ret = si7021_i2c_send_command(
		si7021->i2c                        ,
		si7021->address                    ,
		SI7021_I2C_CMD_MEASURE_TEMPERATURE );

	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG, "Error with command 'measure_temperature'.");
		return ret;
	}

	return ESP_OK;
}

static esp_err_t si7021_read_measure_temperature(
		si7021_handle_t *si7021      ,
		uint16_t        *temperature )
{
	esp_err_t ret;

	ret = si7021_i2c_read_long(
		si7021->i2c     ,
		si7021->address ,
		temperature     ,
		1               ,
		1               );
	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG, "Error reading after command 'measure_temperature'.");
		return ret;
	}

	return ESP_OK;
}

esp_err_t si7021_measure_temperature_and_read(
		si7021_handle_t *si7021      ,
		uint16_t        *temperature )
{
	esp_err_t ret;
	ret = si7021_measure_temperature(si7021);
	if (ret != ESP_OK)
		return ret;

	si7021_sleep_ms(SI7021_I2C_WAIT_MS_MEASURE_TEMPERATURE);

	ret = si7021_read_measure_temperature(si7021, temperature);
	if (ret != ESP_OK)
		return ret;

	return ESP_OK;
}

static esp_err_t si7021_measure_temperature_from_previous_rh(
		si7021_handle_t *si7021 )
{
	esp_err_t ret;
	ret = si7021_i2c_send_command(
		si7021->i2c                                      ,
		si7021->address                                  ,
		SI7021_I2C_CMD_READ_TEMPERATURE_FROM_PREVIOUS_RH );

	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG, "Error with command 'measure_temperature_from_previous_rh'.");
		return ret;
	}

	return ESP_OK;
}

static esp_err_t si7021_read_measure_temperature_from_previous_rh(
		si7021_handle_t *si7021      ,
		uint16_t        *temperature )
{
	esp_err_t ret;

	ret = si7021_i2c_read_long(
		si7021->i2c     ,
		si7021->address ,
		temperature     ,
		1               ,
		0               );
	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG, "Error reading after command 'measure_temperature_from_previous_rh'.");
		return ret;
	}

	return ESP_OK;
}

esp_err_t si7021_measure_temperature_from_previous_rh_and_read(
		si7021_handle_t *si7021      ,
		uint16_t        *temperature )
{
	esp_err_t ret;
	ret = si7021_measure_temperature_from_previous_rh(si7021);
	if (ret != ESP_OK)
		return ret;

	// no wait time

	ret = si7021_read_measure_temperature_from_previous_rh(si7021, temperature);
	if (ret != ESP_OK)
		return ret;

	return ESP_OK;
}

esp_err_t si7021_set_user_register(
		si7021_handle_t *si7021   ,
		uint8_t          user_reg )
{
	esp_err_t ret;

	ret = si7021_i2c_send_command_with_data(
		si7021->i2c                        ,
		si7021->address                    ,
		SI7021_I2C_CMD_SET_USER_REGISTER   ,
		&user_reg                          ,
		1                                  ,
		0                                  );
	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG, "Error with command 'set_user_register'.");
		return ret;
	}

	si7021_sleep_ms(SI7021_I2C_WAIT_MS_SET_USER_REGISTER);

	return ESP_OK;
}

static esp_err_t si7021_get_user_register(
		si7021_handle_t *si7021   )
{
	esp_err_t ret;
	ret = si7021_i2c_send_command(
		si7021->i2c                      ,
		si7021->address                  ,
		SI7021_I2C_CMD_GET_USER_REGISTER );

	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG, "Error with command 'get_user_register'.");
		return ret;
	}

	return ESP_OK;
}

static esp_err_t si7021_read_get_user_register(
		si7021_handle_t *si7021   ,
		uint8_t         *user_reg )
{
	esp_err_t ret;

	ret = si7021_i2c_read(
		si7021->i2c     ,
		si7021->address ,
		user_reg        ,
		1               ,
		0               );
	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG, "Error reading after command 'get_user_register'.");
		return ret;
	}

	return ESP_OK;
}

esp_err_t si7021_get_user_register_and_read(
		si7021_handle_t *si7021   ,
		uint8_t         *user_reg )
{
	esp_err_t ret;
	ret = si7021_get_user_register(si7021);
	if (ret != ESP_OK)
		return ret;

	si7021_sleep_ms(SI7021_I2C_WAIT_MS_GET_USER_REGISTER);

	ret = si7021_read_get_user_register(si7021, user_reg);
	if (ret != ESP_OK)
		return ret;

	return ESP_OK;
}



esp_err_t si7021_set_heater_register(
		si7021_handle_t *si7021     ,
		uint8_t          heater_reg )
{
	esp_err_t ret;

	ret = si7021_i2c_send_command_with_data(
		si7021->i2c                        ,
		si7021->address                    ,
		SI7021_I2C_CMD_SET_HEATER_REGISTER ,
		&heater_reg                        ,
		1                                  ,
		0                                  );
	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG, "Error with command 'set_heater_register'.");
		return ret;
	}

	si7021_sleep_ms(SI7021_I2C_WAIT_MS_SET_HEATER_REGISTER);

	return ESP_OK;
}

static esp_err_t si7021_get_heater_register(
		si7021_handle_t *si7021   )
{
	esp_err_t ret;
	ret = si7021_i2c_send_command(
		si7021->i2c                        ,
		si7021->address                    ,
		SI7021_I2C_CMD_GET_HEATER_REGISTER );

	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG, "Error with command 'get_heater_register'.");
		return ret;
	}

	return ESP_OK;
}

static esp_err_t si7021_read_get_heater_register(
		si7021_handle_t *si7021     ,
		uint8_t         *heater_reg )
{
	esp_err_t ret;

	ret = si7021_i2c_read(
		si7021->i2c     ,
		si7021->address ,
		heater_reg      ,
		1               ,
		0               );
	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG, "Error reading after command 'get_heater_register'.");
		return ret;
	}

	return ESP_OK;
}

esp_err_t si7021_get_heater_register_and_read(
		si7021_handle_t *si7021     ,
		uint8_t         *heater_reg )
{
	esp_err_t ret;
	ret = si7021_get_heater_register(si7021);
	if (ret != ESP_OK)
		return ret;

	si7021_sleep_ms(SI7021_I2C_WAIT_MS_GET_HEATER_REGISTER);

	ret = si7021_read_get_heater_register(si7021, heater_reg);
	if (ret != ESP_OK)
		return ret;

	return ESP_OK;
}

static esp_err_t si7021_get_id_fst_access(
		si7021_handle_t *si7021 )
{
	esp_err_t ret;
	ret = si7021_i2c_send_command_long(
		si7021->i2c                      ,
		si7021->address                  ,
		SI7021_I2C_CMD_GET_ID_FST_ACCESS );

	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG, "Error with command 'get_id_fst_access'.");
		return ret;
	}

	return ESP_OK;
}

static esp_err_t si7021_read_get_id_fst_access(
		si7021_handle_t *si7021 ,
		uint8_t         *sna3   ,
		uint8_t         *sna2   ,
		uint8_t         *sna1   ,
		uint8_t         *sna0   )
{
	esp_err_t ret;

	uint8_t buf[4];

	ret = si7021_i2c_read(
		si7021->i2c     ,
		si7021->address ,
		buf             ,
		4               ,
		1               );
	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG, "Error reading after command 'get_id_fst_access'.");
		return ret;
	}

	*sna3 = buf[0];
	*sna2 = buf[1];
	*sna1 = buf[2];
	*sna0 = buf[3];

	return ESP_OK;
}

esp_err_t si7021_get_id_fst_access_and_read(
		si7021_handle_t *si7021 ,
		uint8_t         *sna3   ,
		uint8_t         *sna2   ,
		uint8_t         *sna1   ,
		uint8_t         *sna0   )
{
	esp_err_t ret;
	ret = si7021_get_id_fst_access(si7021);
	if (ret != ESP_OK)
		return ret;

	si7021_sleep_ms(SI7021_I2C_WAIT_MS_GET_ID_FST_ACCESS);

	ret = si7021_read_get_id_fst_access(si7021, sna3, sna2, sna1, sna0);
	if (ret != ESP_OK)
		return ret;

	return ESP_OK;
}

static esp_err_t si7021_get_id_snd_access(
		si7021_handle_t *si7021 )
{
	esp_err_t ret;
	ret = si7021_i2c_send_command_long(
		si7021->i2c                      ,
		si7021->address                  ,
		SI7021_I2C_CMD_GET_ID_SND_ACCESS );

	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG, "Error with command 'get_id_snd_access'.");
		return ret;
	}

	return ESP_OK;
}

static esp_err_t si7021_read_get_id_snd_access(
		si7021_handle_t *si7021 ,
		uint8_t         *snb3   ,
		uint8_t         *snb2   ,
		uint8_t         *snb1   ,
		uint8_t         *snb0   )
{
	esp_err_t ret;

	uint16_t buf[2];

	ret = si7021_i2c_read_long(
		si7021->i2c     ,
		si7021->address ,
		buf             ,
		2               ,
		1               );
	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG, "Error reading after command 'get_id_snd_access'.");
		return ret;
	}

	*snb3 = (uint8_t) ( (buf[0] & 0xFF00) >> 8 );
	*snb2 = (uint8_t) (buf[0] & 0x00FF);
	*snb1 = (uint8_t) ( (buf[1] & 0xFF00) >> 8 );
	*snb0 = (uint8_t) (buf[1] & 0x00FF);

	return ESP_OK;
}

esp_err_t si7021_get_id_snd_access_and_read(
		si7021_handle_t *si7021 ,
		uint8_t         *snb3   ,
		uint8_t         *snb2   ,
		uint8_t         *snb1   ,
		uint8_t         *snb0   )
{
	esp_err_t ret;
	ret = si7021_get_id_snd_access(si7021);
	if (ret != ESP_OK)
		return ret;

	si7021_sleep_ms(SI7021_I2C_WAIT_MS_GET_ID_SND_ACCESS);

	ret = si7021_read_get_id_snd_access(si7021, snb3, snb2, snb1, snb0);
	if (ret != ESP_OK)
		return ret;

	return ESP_OK;
}

static esp_err_t si7021_get_firmware_revision(
		si7021_handle_t *si7021 )
{
	esp_err_t ret;
	ret = si7021_i2c_send_command_long(
		si7021->i2c                          ,
		si7021->address                      ,
		SI7021_I2C_CMD_GET_FIRMWARE_REVISION );

	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG, "Error with command 'get_firmware_revision'.");
		return ret;
	}

	return ESP_OK;
}

static esp_err_t si7021_read_get_firmware_revision(
		si7021_handle_t *si7021 ,
		uint8_t         *fw_rev )
{
	esp_err_t ret;

	ret = si7021_i2c_read(
		si7021->i2c     ,
		si7021->address ,
		fw_rev          ,
		1               ,
		0               );
	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG, "Error reading after command 'get_firmware_revision'.");
		return ret;
	}

	return ESP_OK;
}

esp_err_t si7021_get_firmware_revision_and_read(
		si7021_handle_t *si7021 ,
		uint8_t         *fw_rev )
{
	esp_err_t ret;
	ret = si7021_get_firmware_revision(si7021);
	if (ret != ESP_OK)
		return ret;

	si7021_sleep_ms(SI7021_I2C_WAIT_MS_GET_FIRMWARE_REVISION);

	ret = si7021_read_get_firmware_revision(si7021, fw_rev);
	if (ret != ESP_OK)
		return ret;

	return ESP_OK;
}





// ** SI7021 ADVANCED METHODS ** //

esp_err_t si7021_measure_and_read_converted(
		si7021_handle_t *si7021     ,
		float           *rh_percent ,
		float           *celsius    )
{
	esp_err_t ret;

	uint16_t rh;
	uint16_t temperature;

	ret = si7021_measure_rh_and_read(si7021, &rh);
	if (ret != ESP_OK)
		return ret;

	*rh_percent = ( (125 * (float) rh) / 65536 ) - 6;

	ret = si7021_measure_temperature_from_previous_rh_and_read(si7021, &temperature);
	if (ret != ESP_OK)
		return ret;

	*celsius    = ( (175.72 * (float) temperature) / 65536 ) - 46.85;

	return ESP_OK;
}

esp_err_t si7021_get_serial_number(
		si7021_handle_t *si7021 ,
		uint64_t        *serial )
{
	esp_err_t ret;

	uint8_t buf[8];

	ret = si7021_get_id_fst_access_and_read(si7021, &buf[0], &buf[1], &buf[2], &buf[3]);
	if (ret != ESP_OK)
		return ret;

	ret = si7021_get_id_snd_access_and_read(si7021, &buf[4], &buf[5], &buf[6], &buf[7]);
	if (ret != ESP_OK)
		return ret;

	*serial = 0;

	uint8_t i;
	for (i = 0; i < 8; ++i)
		*serial |= ( (uint64_t) buf[i] ) << (8 * i);

	return ESP_OK;
}

esp_err_t si7021_heater_enable(
		si7021_handle_t *si7021 )
{
	esp_err_t ret;

	uint8_t user_reg;
	ret = si7021_get_user_register_and_read(si7021, &user_reg);
	if (ret != ESP_OK)
		return ret;

	user_reg |= 0x04; // 0000 0100

	ret = si7021_set_user_register(si7021, user_reg);
	if (ret != ESP_OK)
		return ret;

	return ESP_OK;
}

esp_err_t si7021_heater_disable(
		si7021_handle_t *si7021 )
{
	esp_err_t ret;

	uint8_t user_reg;
	ret = si7021_get_user_register_and_read(si7021, &user_reg);
	if (ret != ESP_OK)
		return ret;

	user_reg &= 0xFB; // 1111 1011

	ret = si7021_set_user_register(si7021, user_reg);
	if (ret != ESP_OK)
		return ret;

	return ESP_OK;
}

esp_err_t si7021_heater_set_current(
		si7021_handle_t *si7021 ,
		uint8_t          mA_val )
{
	esp_err_t ret;

	uint8_t heater_reg;
	ret = si7021_get_heater_register_and_read(si7021, &heater_reg);
	if (ret != ESP_OK)
		return ret;

	heater_reg &= 0xF0;
	heater_reg |= (mA_val & 0x0F);

	ret = si7021_set_heater_register(si7021, heater_reg);
	if (ret != ESP_OK)
		return ret;

	return ESP_OK;
}

esp_err_t si7021_set_measurement_precision(
		si7021_handle_t *si7021   ,
		uint8_t          prec_val )
{
	esp_err_t ret;

	uint8_t user_reg;
	ret = si7021_get_user_register_and_read(si7021, &user_reg);
	if (ret != ESP_OK)
		return ret;

	user_reg &= 0x7E; // 0111 1110
	user_reg |= ( (prec_val << 6) & 0x80 ) | (prec_val & 0x01);

	ret = si7021_set_user_register(si7021, user_reg);
	if (ret != ESP_OK)
		return ret;

	return ESP_OK;
}