#include "app_i2c.h"

#include "string.h"

static const char *TAG = "APP_I2C";
#include "esp_log.h"

static esp_err_t app_i2c_wait_while_clock_stretching(
		uint8_t  scl       ,
		uint32_t period_ms )
{
	ESP_LOGD(TAG, "Trying to detect SCL high while clock stretching.");

	esp_err_t ret;
	
	// 150 ms timeout
	uint32_t timeout_cycles = 150 / period_ms;

	uint8_t level;

	while (--timeout_cycles)
	{
		ESP_LOGV(TAG, "Reading SCL level. Cycles left: %d.", timeout_cycles);
		if ( (ret = app_i2c_ll_SCL_read(scl, &level)) == ESP_OK && level)
		{
			ESP_LOGD(TAG, "SCL pin HIGH level detected after clk stretching.");
			return ESP_OK;
		}
		else if (ret == ESP_ERR_INVALID_ARG)
		{
			ESP_LOGE(TAG, "Error reading SCL while clock stretching.");
			return ret;
		}

		ESP_LOGV(TAG,
			"Sleeping for SCL clock period (%d ms).",
			period_ms
		);
		app_i2c_ll_sleep(period_ms);
	}

	ESP_LOGE(TAG, "Timeout while trying to detect SCL in clk stretching.");
	return ESP_FAIL;
}


static esp_err_t app_i2c_write_byte(
		app_i2c_handle_t *i2c  ,
		uint8_t           data )
{
	ESP_LOGD(TAG, "Writing byte into SGP30. Data: %d.", data);

	esp_err_t ret;

	int8_t  i;
	uint8_t level;

	for (i = 7; i >= 0; i--)
	{
		ESP_ERROR_CHECK( app_i2c_ll_SCL_out(i2c->scl) );
		if ( (data >> i) & 0x01 )
			ESP_ERROR_CHECK( app_i2c_ll_SDA_in(i2c->sda) );
		else
			ESP_ERROR_CHECK( app_i2c_ll_SDA_out(i2c->sda) );
		app_i2c_ll_sleep(i2c->op_delay_ms);

		ESP_ERROR_CHECK( app_i2c_ll_SCL_in(i2c->scl) );
		app_i2c_ll_sleep(i2c->op_delay_ms);

		ret = app_i2c_wait_while_clock_stretching(i2c->scl, i2c->scl_period_ms);
		if (ret != ESP_OK)
			goto write_byte_error;
	}

	ESP_ERROR_CHECK( app_i2c_ll_SCL_out(i2c->scl) );
	ESP_ERROR_CHECK( app_i2c_ll_SDA_in(i2c->sda) );
	app_i2c_ll_sleep(i2c->op_delay_ms);

	ESP_ERROR_CHECK( app_i2c_ll_SCL_in(i2c->scl) );
	ret = app_i2c_wait_while_clock_stretching(i2c->scl, i2c->scl_period_ms);
	if (ret != ESP_OK)
		goto write_byte_error;

	app_i2c_ll_SDA_read(i2c->sda, &level);
	ESP_ERROR_CHECK( app_i2c_ll_SCL_out(i2c->scl) );

	ret = ESP_FAIL;
	if (level != 0)
		goto write_byte_error;

	return ESP_OK;

write_byte_error:
	ESP_LOGE(TAG, "Error while writing byte in SGP30.");
	return ret;
}

static esp_err_t app_i2c_read_byte(
		app_i2c_handle_t *i2c ,
		uint8_t  ack          ,
		uint8_t *data         )
{
	ESP_LOGD(TAG, "Reading byte from SGP30. Data: %d.", *data);

	esp_err_t ret;

	uint8_t level;
	int8_t  i;

	*data = 0;

	ESP_ERROR_CHECK( app_i2c_ll_SDA_in(i2c->sda) );
	for (i = 7; i >= 0; i--)
	{
		app_i2c_ll_sleep(i2c->op_delay_ms);
		
		ESP_ERROR_CHECK( app_i2c_ll_SCL_in(i2c->scl) );
		ret = app_i2c_wait_while_clock_stretching(i2c->scl, i2c->scl_period_ms);
		if (ret != ESP_OK)
			goto read_byte_error;

		ESP_ERROR_CHECK( app_i2c_ll_SDA_read(i2c->sda, &level) );
		*data |= level << i;
		ESP_ERROR_CHECK( app_i2c_ll_SCL_out(i2c->scl) );
	}

	if (ack)
		ESP_ERROR_CHECK( app_i2c_ll_SDA_out(i2c->sda) );
	else
		ESP_ERROR_CHECK( app_i2c_ll_SDA_in(i2c->sda) );
	app_i2c_ll_sleep(i2c->op_delay_ms);
	
	ESP_ERROR_CHECK( app_i2c_ll_SCL_in(i2c->scl) );
	app_i2c_ll_sleep(i2c->op_delay_ms);

	ret = app_i2c_wait_while_clock_stretching(i2c->scl, i2c->scl_period_ms);
	if (ret != ESP_OK)
		goto read_byte_error;

	ESP_ERROR_CHECK( app_i2c_ll_SCL_out(i2c->scl) );
	ESP_ERROR_CHECK( app_i2c_ll_SDA_in(i2c->sda) );

	return ESP_OK;

read_byte_error:
	ESP_LOGE(TAG, "Error while reading byte from SGP30.");
	return ret;
}

static esp_err_t app_i2c_start(
		app_i2c_handle_t *i2c )
{
	ESP_LOGD(TAG, "Starting I2C communication with SGP30");

	esp_err_t ret;

	ESP_ERROR_CHECK( app_i2c_ll_SCL_in(i2c->scl) );
	ret = app_i2c_wait_while_clock_stretching(i2c->scl, i2c->scl_period_ms);
	if (ret != ESP_OK)
		goto start_error;

	ESP_ERROR_CHECK( app_i2c_ll_SDA_out(i2c->sda) );
	app_i2c_ll_sleep(i2c->op_delay_ms);

	ESP_ERROR_CHECK( app_i2c_ll_SCL_out(i2c->scl) );
	app_i2c_ll_sleep(i2c->op_delay_ms);

	return ESP_OK;

start_error:
	ESP_LOGE(TAG, "Error while starting SGP30 I2C communication.");
	return ret;
}

static esp_err_t app_i2c_stop(
		app_i2c_handle_t *i2c )
{
	ESP_LOGD(TAG, "Stopping I2C communication with SGP30");

	ESP_ERROR_CHECK( app_i2c_ll_SDA_out(i2c->sda) );
	app_i2c_ll_sleep(i2c->op_delay_ms);
	
	ESP_ERROR_CHECK( app_i2c_ll_SCL_in(i2c->scl) );
	app_i2c_ll_sleep(i2c->op_delay_ms);
	
	ESP_ERROR_CHECK( app_i2c_ll_SDA_in(i2c->sda) );
	app_i2c_ll_sleep(i2c->op_delay_ms);

	return ESP_OK;
}

//////////////////////////////////////////////////////////////////






esp_err_t app_i2c_create(
		const char            *slave ,
		app_i2c_config_args_t *args  ,
		app_i2c_handle_t      *i2c   )
{
	size_t len = strlen(slave) + 1;
	if (len > 128)
		return ESP_ERR_INVALID_ARG;

	strcpy(i2c->slave, slave);

	i2c->scl           = args->scl;
	i2c->sda           = args->sda;
	i2c->scl_period_ms = args->scl_period_ms;
	i2c->op_delay_ms   = args->op_delay_ms;

	return ESP_OK;
}

esp_err_t app_i2c_delete(
		app_i2c_handle_t *i2c )
{
	return ESP_OK;
}

esp_err_t app_i2c_init(
		app_i2c_handle_t *i2c )
{
	ESP_LOGD(TAG, "Initializing I2C communication pins.");

	app_i2c_ll_init_pins(i2c->sda, i2c->scl);
	ESP_ERROR_CHECK( app_i2c_ll_SCL_in(i2c->scl) );
	ESP_ERROR_CHECK( app_i2c_ll_SDA_in(i2c->sda) );

	return ESP_OK;
}

esp_err_t app_i2c_release(
		app_i2c_handle_t *i2c )
{
	ESP_LOGD(TAG, "Releasing I2C communication pins.");

	ESP_ERROR_CHECK( app_i2c_ll_SCL_in(i2c->scl) );
	ESP_ERROR_CHECK( app_i2c_ll_SDA_in(i2c->sda) );
	app_i2c_ll_release_pins(i2c->scl, i2c->sda);

	return ESP_OK;
}

esp_err_t app_i2c_write(
		app_i2c_handle_t *i2c     ,
		uint8_t           address ,
		uint8_t const    *data    ,
		uint16_t          count   )
{
	ESP_LOGD(TAG, "Writing a string of %d bytes into SGP30.", count);

	esp_err_t ret;
	
	uint16_t i;

	ESP_LOGV(TAG, "Starting I2C communication.");
	ret = app_i2c_start(i2c);
	if (ret != ESP_OK)
		goto write_error;
	
	ESP_LOGV(TAG, "Writing op write address into SGP30.");
	ret = app_i2c_write_byte(i2c, address << 1);
	if (ret != ESP_OK)
	{
		app_i2c_stop(i2c);
		goto write_error;
	}

	ESP_LOGV(TAG, "Writing string of bytes into SGP30.");
	for (i = 0; i < count; ++i)
	{
		ESP_LOGV(TAG, "Writing byte no. %d / %d.", i, count);
		ret = app_i2c_write_byte(i2c, data[i]);
		if (ret != ESP_OK)
		{
			app_i2c_stop(i2c);
			goto write_error;
		}
	}

	ESP_LOGV(TAG, "Stopping I2C communication.");
	app_i2c_stop(i2c);

	return ESP_OK;

write_error:
	ESP_LOGE(TAG, "Error while writing data into SGP30.");
	return ret;
}

esp_err_t app_i2c_read(
		app_i2c_handle_t *i2c     ,
		uint8_t           address ,
		uint8_t          *data    ,
		uint16_t          count   )
{
	ESP_LOGD(TAG, "Reading a string of %d bytes from SGP30.", count);

	esp_err_t ret;
	
	uint16_t i;
	uint8_t  send_ack;

	ESP_LOGV(TAG, "Starting I2C communication.");
	ret = app_i2c_start(i2c);
	if (ret != ESP_OK)
		goto read_error;
	
	ESP_LOGV(TAG, "Writing op read address into SGP30.");
	ret = app_i2c_write_byte(i2c, (address << 1) | 1);
	if (ret != ESP_OK)
	{
		app_i2c_stop(i2c);
		goto read_error;
	}

	ESP_LOGV(TAG, "Writing string of bytes into SGP30.");
	for (i = 0; i < count; ++i)
	{
		ESP_LOGV(TAG, "Reading byte no. %d / %d.", i, count);

		send_ack = i < (count - 1); // last byte must be NACK'ed
		ret = app_i2c_read_byte(i2c, send_ack, data + i);
		if (ret != ESP_OK)
		{
			app_i2c_stop(i2c);
			goto read_error;
		}
	}

	ESP_LOGV(TAG, "Stopping I2C communication.");
	app_i2c_stop(i2c);

	return ESP_OK;

read_error:
	ESP_LOGE(TAG, "Error while reading data from iSGP30.");
	return ret;
}