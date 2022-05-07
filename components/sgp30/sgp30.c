#include "sgp30.h"

#include "esp_log.h"
static const char *TAG = "SGP30";

#include "string.h"

// ** SGP30 HANDLE LOGIC ** //S
#define SGP30_NAME_SIZE          128
#define SGP30_I2C_ADDRESS        0x58
#define SGP30_I2C_NAME           "sgp30_i2c"
#define SGP30_I2C_SCL_PERIOD_MS  2
#define SGP30_I2C_OP_DELAY_MS    (SGP30_I2C_SCL_PERIOD_MS / 2)

esp_err_t sgp30_create(
		char *name                 ,
		sgp30_config_args_t *args  ,
		sgp30_handle_t      *sgp30 )
{
	ESP_LOGD(TAG, "Creating SGP30 handle.");

	size_t len = strnlen(name, SGP30_NAME_SIZE);
	if (len == SGP30_NAME_SIZE)
	{
		ESP_LOGE(TAG,
			"SGP20 handle name string length must be under %d characters.",
			SGP30_NAME_SIZE
		);

		return ESP_ERR_INVALID_ARG;
	}

	ESP_LOGV(TAG, "Loading SGP30 handle name \"%.*s\".", len, name);
	sgp30->name = malloc(sizeof(char) * SGP30_NAME_SIZE);
	strcpy(sgp30->name, name);

	sgp30->address = SGP30_I2C_ADDRESS;

	ESP_LOGV(TAG, "Loading SGP30 I2C configuration.");
	app_i2c_config_args_t i2c_args = {
		.scl = args->scl_gpio_pin                 ,
		.sda = args->sda_gpio_pin                 ,
		.scl_period_ms = SGP30_I2C_SCL_PERIOD_MS  ,
		.op_delay_ms   = SGP30_I2C_OP_DELAY_MS    };

	esp_err_t ret;
	app_i2c_handle_t *i2c = malloc(sizeof(app_i2c_handle_t));
	
	ret = app_i2c_create(
			SGP30_I2C_NAME ,
			&i2c_args      ,
			i2c            );
	if (ret != ESP_OK)
		return ret;

	sgp30->i2c = i2c;

	return ESP_OK;
}

esp_err_t sgp30_delete(
		sgp30_handle_t *sgp30 )
{
	ESP_LOGD(TAG,
		"Destroying SGP30 handle \"%.*s\".",
		SGP30_NAME_SIZE, sgp30->name
	);

	free(sgp30->name);
	
	esp_err_t ret;
	ret = app_i2c_delete(sgp30->i2c);
	if (ret != ESP_OK)
		return ret;

	return ESP_OK;
}

// *** *** //





// ** SGP30 I2C BASIC METHODS ** //

#define SGP30_I2C_CRC8_POLY  0x31 // x^8 + x^5 + x^4 + 1
#define SGP30_I2C_CRC8_INIT  0xFF
#define SGP30_I2C_CRC8_XOR   0x00

static uint8_t crc8_checksum_calculate(
		uint8_t  *bytes ,
		uint16_t  count )
{
	ESP_LOGD(TAG, "Calculating CRC8 checksum for %d bytes.", count);

	uint8_t crc8 = SGP30_I2C_CRC8_INIT;
	uint16_t i, j;
	for (i = 0; i < count; ++i)
	{
		crc8 ^= bytes[i];
		for (j = 0; j < 8; ++j)
		{
			if (crc8 & 0x80)
				crc8 = (crc8 << 1) ^ SGP30_I2C_CRC8_POLY;
			else
				crc8 = (crc8 << 1);
		}
		crc8 ^= SGP30_I2C_CRC8_XOR;
	}

	return crc8;
}

static void sgp30_sleep_ms(
		uint16_t ms )
{
	app_i2c_ll_sleep(ms);
}

static esp_err_t sgp30_i2c_checksum_check(
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

static esp_err_t sgp30_i2c_send_command(
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

static esp_err_t sgp30_i2c_send_command_with_data(
	app_i2c_handle_t *i2c      ,
	uint8_t           address  ,
	uint16_t          command  ,
	uint16_t         *data     ,
	uint16_t          num_data )
{
	ESP_LOGD(TAG, "Sending command with args to device.");

	uint16_t count = 2 + num_data * 3; // addr + data + crc
	uint8_t  buf[count];
	uint16_t i = 0;

	buf[i++] = (uint8_t) (command >> 8 );
	buf[i++] = (uint8_t) (command & 0x00FF);

	uint16_t j;
	for (j = 0; j < num_data; ++j)
	{
		uint8_t *buf_ptr = &buf[i];
		buf[i++] = (uint8_t) (data[j] >> 8);
		buf[i++] = (uint8_t) (data[j] & 0x00FF);
		buf[i++] = crc8_checksum_calculate(buf_ptr, 2);
	}

	esp_err_t ret;
	ret = app_i2c_write(i2c, address, buf, count);
	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG,
			"Error while sending command 0x%X with args to device address 0x%X.",
			command,
			address
		);
		return ret;
	}

	return ESP_OK;
}

static esp_err_t sgp30_i2c_read(
	app_i2c_handle_t *i2c     ,
	uint8_t           address ,
	uint16_t         *data    ,
	uint16_t          num_data)
{
	ESP_LOGD(TAG, "Reading from device.");

	uint16_t count = num_data * 3;
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
		uint8_t *data_buf = buf + 3*j;
		ret = sgp30_i2c_checksum_check(data_buf, 2, data_buf[2]);
		if (ret != ESP_OK)
		{
			ESP_LOGE(TAG, "Error in checksum of read data.");
			return ret;
		}

		data[j] = ( (uint16_t) data_buf[0] << 8) | ( (uint16_t) data_buf[1] );
	}

	return ESP_OK;
}

// *** *** //





// ** SGP30 COMMAND METHODS ** //

#define SGP30_I2C_CMD_IAQ_INIT                     0x2003
#define SGP30_I2C_CMD_MEASURE_IAQ                  0x2008
#define SGP30_I2C_CMD_GET_IAQ_BASELINE             0x2015
#define SGP30_I2C_CMD_SET_IAQ_BASELINE             0x201E
#define SGP30_I2C_CMD_SET_ABSOLUTE_HUMIDITY        0x2061
#define SGP30_I2C_CMD_MEASURE_TEST                 0x2032
#define SGP30_I2C_CMD_GET_FEATURE_SET              0x202F
#define SGP30_I2C_CMD_MEASURE_RAW                  0x2050
#define SGP30_I2C_CMD_GET_TVOC_INCEPTIVE_BASELINE  0x20B3
#define SGP30_I2C_CMD_SET_TVOC_BASELINE            0x2077

#define SGP30_I2C_WAIT_MS_IAQ_INIT                     10
#define SGP30_I2C_WAIT_MS_MEASURE_IAQ                  24
#define SGP30_I2C_WAIT_MS_GET_IAQ_BASELINE             10
#define SGP30_I2C_WAIT_MS_SET_IAQ_BASELINE             10
#define SGP30_I2C_WAIT_MS_SET_ABSOLUTE_HUMIDITY        10
#define SGP30_I2C_WAIT_MS_MEASURE_TEST                 220
#define SGP30_I2C_WAIT_MS_GET_FEATURE_SET              10
#define SGP30_I2C_WAIT_MS_MEASURE_RAW                  25
#define SGP30_I2C_WAIT_MS_GET_TVOC_INCEPTIVE_BASELINE  10
#define SGP30_I2C_WAIT_MS_SET_TVOC_BASELINE            10

#define SGP30_I2C_RETURN_MEASURE_TEST 0xD400

esp_err_t sgp30_iaq_init(
		sgp30_handle_t *sgp30 )
{
	esp_err_t ret;
	ret = sgp30_i2c_send_command(
		sgp30->i2c                     ,
		sgp30->address                 ,
		SGP30_I2C_CMD_IAQ_INIT );

	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG, "Error with command 'iaq_init'.");
		return ret;
	}

	sgp30_sleep_ms(SGP30_I2C_WAIT_MS_IAQ_INIT);

	return ESP_OK;
}

static esp_err_t sgp30_measure_iaq(
		sgp30_handle_t *sgp30 )
{
	esp_err_t ret;
	ret = sgp30_i2c_send_command(
		sgp30->i2c                ,
		sgp30->address            ,
		SGP30_I2C_CMD_MEASURE_IAQ );

	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG, "Error with command 'measure_iaq'.");
		return ret;
	}

	return ESP_OK;
}

static esp_err_t sgp30_read_measure_iaq(
	sgp30_handle_t *sgp30         ,
	uint16_t       *tvoc          ,
	uint16_t       *co2_eq        )
{
	esp_err_t ret;
	uint16_t data[2];

	ret = sgp30_i2c_read(
		sgp30->i2c     ,
		sgp30->address ,
		data           ,
		2              );
	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG, "Error reading after command 'measure_iaq'.");
		return ret;
	}

	*tvoc   = data[0];
	*co2_eq = data[1];

	return ESP_OK;
}

esp_err_t sgp30_measure_iaq_and_read(
		sgp30_handle_t *sgp30         ,
		uint16_t       *tvoc          ,
		uint16_t       *co2_eq        )
{
	esp_err_t ret;
	ret = sgp30_measure_iaq(sgp30);
	if (ret != ESP_OK)
		return ret;

	sgp30_sleep_ms(SGP30_I2C_WAIT_MS_MEASURE_IAQ);

	ret = sgp30_read_measure_iaq(sgp30, tvoc, co2_eq);
	if (ret != ESP_OK)
		return ret;

	return ESP_OK;
}

static esp_err_t sgp30_get_iaq_baseline(
		sgp30_handle_t *sgp30 )
{
	esp_err_t ret;
	ret = sgp30_i2c_send_command(
		sgp30->i2c                     ,
		sgp30->address                 ,
		SGP30_I2C_CMD_GET_IAQ_BASELINE );

	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG, "Error with command 'get_iaq_baseline'.");
		return ret;
	}

	return ESP_OK;
}

static esp_err_t sgp30_read_get_iaq_baseline(
		sgp30_handle_t *sgp30         ,
		uint32_t       *baseline      )
{
	esp_err_t ret;

	uint16_t data[2];
	ret = sgp30_i2c_read(
		sgp30->i2c     ,
		sgp30->address ,
		data           ,
		2              );
	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG, "Error reading after command 'get_iaq_baseline'.");
		return ret;
	}

	*baseline = ( (uint32_t) data[0] << 16 ) | ( (uint32_t) data[1] );

	return ESP_OK;
}

esp_err_t sgp30_get_iaq_baseline_and_read(
		sgp30_handle_t *sgp30         ,
		uint32_t       *baseline      )
{
	esp_err_t ret;
	ret = sgp30_get_iaq_baseline(sgp30);
	if (ret != ESP_OK)
		return ret;

	sgp30_sleep_ms(SGP30_I2C_WAIT_MS_GET_IAQ_BASELINE);

	ret = sgp30_read_get_iaq_baseline(sgp30, baseline);
	if (ret != ESP_OK)
		return ret;

	return ESP_OK;
}

esp_err_t sgp30_set_iaq_baseline(
		sgp30_handle_t *sgp30         ,
		uint32_t        baseline      )
{
	esp_err_t ret;

	ret = sgp30_i2c_send_command_with_data(
		sgp30->i2c                     ,
		sgp30->address                 ,
		SGP30_I2C_CMD_SET_IAQ_BASELINE ,
		(uint16_t *)&baseline          ,
		2                              );
	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG, "Error with command 'set_iaq_baseline'.");
		return ret;
	}

	sgp30_sleep_ms(SGP30_I2C_WAIT_MS_SET_IAQ_BASELINE);

	return ESP_OK;
}

esp_err_t sgp30_set_absolute_humidity(
		sgp30_handle_t *sgp30         ,
		uint16_t        humidity      )
{
	esp_err_t ret;

	ret = sgp30_i2c_send_command_with_data(
		sgp30->i2c                          ,
		sgp30->address                      ,
		SGP30_I2C_CMD_SET_ABSOLUTE_HUMIDITY ,
		&humidity                           ,
		1                                   );
	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG, "Error with command 'set_absolute_humidity'.");
		return ret;
	}

	sgp30_sleep_ms(SGP30_I2C_WAIT_MS_SET_ABSOLUTE_HUMIDITY);

	return ESP_OK;
}

esp_err_t sgp30_measure_test(
		sgp30_handle_t *sgp30 )
{
	esp_err_t ret;

	ret = sgp30_i2c_send_command(
		sgp30->i2c                 ,
		sgp30->address             ,
		SGP30_I2C_CMD_MEASURE_TEST );
	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG, "Error with command 'measure_test'.");
		return ret;
	}

	sgp30_sleep_ms(SGP30_I2C_WAIT_MS_MEASURE_TEST);

	uint16_t data;
	ret = sgp30_i2c_read(
		sgp30->i2c     ,
		sgp30->address ,
		&data          ,
		1              );
	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG, "Error reading after command 'measure_test.");
		return ret;
	}

	if (data != SGP30_I2C_RETURN_MEASURE_TEST)
	{
		ESP_LOGE(TAG, "Not succesfull in performing command 'measure_test'.");
		return ESP_FAIL;
	}

	return ESP_OK;
}

static esp_err_t sgp30_get_feature_set(
		sgp30_handle_t *sgp30   )
{
	esp_err_t ret;

	ret = sgp30_i2c_send_command(
		sgp30->i2c                    ,
		sgp30->address                ,
		SGP30_I2C_CMD_GET_FEATURE_SET );
	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG, "Error with command 'get_feature_set'.");
		return ret;
	}

	return ESP_OK;
}

static esp_err_t sgp30_read_get_feature_set(
		sgp30_handle_t *sgp30   ,
		uint8_t       *type    ,
		uint8_t       *version )
{
	esp_err_t ret;
	uint16_t data;

	ret = sgp30_i2c_read(
		sgp30->i2c     ,
		sgp30->address ,
		&data          ,
		1              );

	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG, "Error reading after command 'get_feature_set'.");
		return ret;
	}

	return ESP_OK;
}

esp_err_t sgp30_get_feature_set_and_read(
		sgp30_handle_t *sgp30         ,
		uint8_t        *type          ,
		uint8_t        *version       )
{
	esp_err_t ret;
	ret = sgp30_get_feature_set(sgp30);
	if (ret != ESP_OK)
		return ret;

	sgp30_sleep_ms(SGP30_I2C_WAIT_MS_GET_FEATURE_SET);

	ret = sgp30_read_get_feature_set(sgp30, type, version);
	if (ret != ESP_OK)
		return ret;

	return ESP_OK;
}

static esp_err_t sgp30_measure_raw(
		sgp30_handle_t *sgp30 )
{
	esp_err_t ret;

	ret = sgp30_i2c_send_command(
		sgp30->i2c                    ,
		sgp30->address                ,
		SGP30_I2C_CMD_MEASURE_RAW );
	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG, "Error with command 'measure_raw'.");
		return ret;
	}

	return ESP_OK;
}

static esp_err_t sgp30_read_measure_raw(
		sgp30_handle_t *sgp30    ,
		uint16_t       *h2       ,
		uint16_t       *ethanol  )
{
	esp_err_t ret;
	uint16_t data[2];

	ret = sgp30_i2c_read(
		sgp30->i2c     ,
		sgp30->address ,
		data           ,
		2              );

	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG, "Error reading after command 'get_feature_set'.");
		return ret;
	}

	*h2      = data[0];
	*ethanol = data[1];

	return ESP_OK;
}

esp_err_t sgp30_measure_raw_and_read(
		sgp30_handle_t *sgp30         ,
		uint16_t       *h2            ,
		uint16_t       *ethanol       )
{
	esp_err_t ret;
	ret = sgp30_measure_raw(sgp30);
	if (ret != ESP_OK)
		return ret;

	sgp30_sleep_ms(SGP30_I2C_WAIT_MS_MEASURE_RAW);

	ret = sgp30_read_measure_raw(sgp30, h2, ethanol);
	if (ret != ESP_OK)
		return ret;

	return ESP_OK;
}

static esp_err_t sgp30_get_tvoc_inceptive_baseline(
		sgp30_handle_t *sgp30 )
{
	esp_err_t ret;

	ret = sgp30_i2c_send_command(
		sgp30->i2c                                ,
		sgp30->address                            ,
		SGP30_I2C_CMD_GET_TVOC_INCEPTIVE_BASELINE );
	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG, "Error with command 'get_tvoc_inceptive_baseline'.");
		return ret;
	}

	return ESP_OK;
}

static esp_err_t sgp30_read_get_tvoc_inceptive_baseline(
		sgp30_handle_t *sgp30    ,
		uint16_t       *baseline )
{
	esp_err_t ret;

	ret = sgp30_i2c_read(
		sgp30->i2c     ,
		sgp30->address ,
		baseline       ,
		1              );

	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG, "Error reading after command 'get_tvoc_inceptive_baseline'.");
		return ret;
	}

	return ESP_OK;
}

esp_err_t sgp30_get_tvoc_inceptive_baseline_and_read(
		sgp30_handle_t *sgp30         ,
		uint16_t       *baseline      )
{
	esp_err_t ret;
	ret = sgp30_get_tvoc_inceptive_baseline(sgp30);
	if (ret != ESP_OK)
		return ret;

	sgp30_sleep_ms(SGP30_I2C_WAIT_MS_GET_TVOC_INCEPTIVE_BASELINE);

	ret = sgp30_read_get_tvoc_inceptive_baseline(sgp30, baseline);
	if (ret != ESP_OK)
		return ret;

	return ESP_OK;
}

esp_err_t sgp30_set_tvoc_baseline(
		sgp30_handle_t *sgp30         ,
		uint16_t        baseline      )
{
	esp_err_t ret;

	ret = sgp30_i2c_send_command_with_data(
		sgp30->i2c                      ,
		sgp30->address                  ,
		SGP30_I2C_CMD_SET_TVOC_BASELINE ,
		&baseline                       ,
		1                               );
	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG, "Error with command 'set_tvoc_baseline'.");
		return ret;
	}

	sgp30_sleep_ms(SGP30_I2C_WAIT_MS_SET_TVOC_BASELINE);

	return ESP_OK;
}