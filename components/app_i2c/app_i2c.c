#include "app_i2c.h"

#include "string.h"

#include "esp_log.h"
static const char *TAG = "APP_I2C";

#include "defines_log.h"
#define LOG_LOCAL_LEVEL APP_I2C_LOG_LEVEL


/* I2C Handle methods */

#define I2C_NAME_SIZE 128

esp_err_t app_i2c_create(
		char                  *name ,
		app_i2c_config_args_t *args ,
		app_i2c_handle_t      *i2c  )
{
	ESP_LOGD(TAG, "Creating I2C handle.");

	size_t len = strnlen(name, I2C_NAME_SIZE);
	if (len == I2C_NAME_SIZE)
	{
		ESP_LOGE(TAG,
			"I2C handle name string length must be under %d characters.",
			I2C_NAME_SIZE
		);

		return ESP_ERR_INVALID_ARG;
	}

	ESP_LOGV(TAG, "Loading I2C handle name \"%.*s\".", len, name);
	i2c->name = malloc(sizeof(char) * I2C_NAME_SIZE);
	strcpy(i2c->name, name);

	ESP_LOGV(TAG, "Loading I2C configuration parameters.");
	i2c->args = malloc(sizeof(app_i2c_config_args_t));
	i2c->args->scl           = args->scl;
	i2c->args->sda           = args->sda;
	i2c->args->scl_period_ms = args->scl_period_ms;
	i2c->args->op_delay_ms   = args->op_delay_ms;

	return ESP_OK;
}

esp_err_t app_i2c_delete(
		app_i2c_handle_t *i2c )
{
	ESP_LOGD(TAG,
		"Destroying I2C handle \"%.*s\".",
		I2C_NAME_SIZE, i2c->name
	);

	free(i2c->name);
	free(i2c->args);

	return ESP_OK;
}

esp_err_t app_i2c_init(
		app_i2c_handle_t *i2c )
{
	ESP_LOGD(TAG,
		"Initializing I2C handle \"%.*s\" communication pins (SDA: %d, SCL: %d).",
		I2C_NAME_SIZE, i2c->name,
		i2c->args->scl,
		i2c->args->sda
	);

	app_i2c_ll_init_pins(i2c->args->sda, i2c->args->scl);
	return ESP_OK;
}

esp_err_t app_i2c_release(
		app_i2c_handle_t *i2c )
{
	ESP_LOGD(TAG,
		"Releasing I2C handle \"%.*s\" communication pins (SDA: %d, SCL: %d).",
		I2C_NAME_SIZE, i2c->name,
		i2c->args->scl,
		i2c->args->sda
	);

	app_i2c_ll_release_pins(i2c->args->scl, i2c->args->sda);
	return ESP_OK;
}





/* I2C basic logic methods */

static esp_err_t app_i2c_wait_while_clock_stretching(
		uint8_t  scl       ,
		uint32_t period_ms )
{
	ESP_LOGD(TAG, "Detecting if SCL high.");

	esp_err_t ret;
	
	// 150 ms timeout
	uint32_t timeout_cycles = 150 / period_ms;

	uint8_t level;

	while (--timeout_cycles)
	{
		ESP_LOGV(TAG, "Reading SCL level. Cycles left: %d.", timeout_cycles);
		if ( (ret = app_i2c_ll_SCL_read(scl, &level)) == ESP_OK && level)
		{
			ESP_LOGD(TAG, "SCL high detected waiting for clock.");
			return ESP_OK;
		}
		else if (ret == ESP_ERR_INVALID_ARG)
		{
			ESP_LOGE(TAG, "Error reading SCL while waiting for clock.");
			return ret;
		}

		ESP_LOGV(TAG,
			"Sleeping for SCL clock period (%d ms).",
			period_ms
		);
		app_i2c_ll_sleep(period_ms);
	}

	ESP_LOGE(TAG, "Timeout while trying to detect SCL high waiting for clock.");
	return ESP_FAIL;
}

static esp_err_t app_i2c_start(
		app_i2c_handle_t *i2c )
{
	ESP_LOGD(TAG,
		"Sending START condition from I2C handle \"%.*s\".",
		I2C_NAME_SIZE, i2c->name
	);

	esp_err_t ret;

	// Set SCL loose.
	ret = app_i2c_ll_SCL_in(i2c->args->scl);
	if (ret != ESP_OK)
		goto app_i2c_start_error;

	// Loop check for available (high) SCL.
	ret = app_i2c_wait_while_clock_stretching(i2c->args->scl, i2c->args->scl_period_ms);
	if (ret != ESP_OK)
		goto app_i2c_start_error;

	// Set SDA low.
	ret = app_i2c_ll_SDA_out(i2c->args->sda);
	if (ret != ESP_OK)
		goto app_i2c_start_error;
	app_i2c_ll_sleep(i2c->args->op_delay_ms);

	// Set SCL low.
	ret = app_i2c_ll_SCL_out(i2c->args->scl);
	if (ret != ESP_OK)
		goto app_i2c_start_error;
	app_i2c_ll_sleep(i2c->args->op_delay_ms);

	return ESP_OK;

app_i2c_start_error:
	ESP_LOGE(TAG,
		"Error sending START condition from I2C handle \"%.*s\".",
		I2C_NAME_SIZE, i2c->name
	);
	return ret;
}

static esp_err_t app_i2c_stop(
		app_i2c_handle_t *i2c )
{
	ESP_LOGD(TAG,
		"Sending STOP condition from I2C handle \"%.*s\".",
		I2C_NAME_SIZE, i2c->name
	);

	esp_err_t ret;

	// Set SDA low.
	ret = app_i2c_ll_SDA_out(i2c->args->sda);
	if (ret != ESP_OK)
		goto app_i2c_stop_error;
	app_i2c_ll_sleep(i2c->args->op_delay_ms);

	// Set SCL loose (high)
	ret = app_i2c_ll_SCL_in(i2c->args->scl);
	if (ret != ESP_OK)
		goto app_i2c_stop_error;
	app_i2c_ll_sleep(i2c->args->op_delay_ms);

	// Set SDA high.
	ret = app_i2c_ll_SDA_in(i2c->args->sda);
	if (ret != ESP_OK)
		goto app_i2c_stop_error;
	app_i2c_ll_sleep(i2c->args->op_delay_ms);

	return ESP_OK;

app_i2c_stop_error:
	ESP_LOGE(TAG,
		"Error sending STOP condition from I2C handle \"%.*s\".",
		I2C_NAME_SIZE, i2c->name
	);
	return ret;
}

static esp_err_t app_i2c_write_byte(
		app_i2c_handle_t *i2c  ,
		uint8_t           data )
{
	ESP_LOGD(TAG,
		"Writing byte \"%d\" with I2C handle \"%.*s\".",
		data,
		I2C_NAME_SIZE, i2c->name
	);

	esp_err_t ret;

	int8_t  i;
	uint8_t level;

	// Write byte //
	for (i = 7; i >= 0; i--) // 8 bits (MSB first)
	{
		// Set SCL low
		ret = app_i2c_ll_SCL_out(i2c->args->scl);
		if (ret != ESP_OK)
			goto app_i2c_write_byte_error;

		// Set SDA to corresponding data bit 0|1
		if ( (data >> i) & 0x01 )
			ret = app_i2c_ll_SDA_in(i2c->args->sda); // SDA high
		else
			ret = app_i2c_ll_SDA_out(i2c->args->sda); // SDA low
		if (ret != ESP_OK)
			goto app_i2c_write_byte_error;
		app_i2c_ll_sleep(i2c->args->op_delay_ms); // wait half period

		// Set SCL loose
		ret = app_i2c_ll_SCL_in(i2c->args->scl);
		app_i2c_ll_sleep(i2c->args->op_delay_ms); // wait half period

		// Wait for SCL high
		ret = app_i2c_wait_while_clock_stretching(i2c->args->scl, i2c->args->scl_period_ms);
		if (ret != ESP_OK)
			goto app_i2c_write_byte_error;
	}

	// Receive ACK //
	// Set SCL low
	ret = app_i2c_ll_SCL_out(i2c->args->scl);
	if (ret != ESP_OK)
		goto app_i2c_write_byte_error;

	// Set SDA loose
	ret = app_i2c_ll_SDA_in(i2c->args->sda);
	if (ret != ESP_OK)
		goto app_i2c_write_byte_error;
	app_i2c_ll_sleep(i2c->args->op_delay_ms); // wait half period

	// Set SCL loose
	ret = app_i2c_ll_SCL_in(i2c->args->scl);
	if (ret != ESP_OK)
		goto app_i2c_write_byte_error;

	// Wait for SCL high
	ret = app_i2c_wait_while_clock_stretching(i2c->args->scl, i2c->args->scl_period_ms);
	if (ret != ESP_OK)
		goto app_i2c_write_byte_error;

	// Read ACK/NACK bit
	app_i2c_ll_SDA_read(i2c->args->sda, &level);

	// Set SCL low
	ret = app_i2c_ll_SCL_out(i2c->args->scl);

	// Assert ACK
	if (level != 0)
	{
		ESP_LOGE(TAG, "NACK received after I2C write byte.");
		return ESP_FAIL;
	}

	return ESP_OK;

app_i2c_write_byte_error:
	ESP_LOGE(TAG,
		"Error writing byte with I2C handle \"%.*s\".",
		I2C_NAME_SIZE, i2c->name
	);
	return ret;
}

static esp_err_t app_i2c_read_byte(
		app_i2c_handle_t *i2c ,
		uint8_t  ack          ,
		uint8_t *data         )
{
	ESP_LOGD(TAG,
		"Reading byte with I2C handle \"%.*s\".",
		I2C_NAME_SIZE, i2c->name
	);

	esp_err_t ret;

	uint8_t level;
	int8_t  i;

	*data = 0x00;

	// Set SDA loose
	ret = app_i2c_ll_SDA_in(i2c->args->sda);
	if (ret != ESP_OK)
		goto app_i2c_read_byte_error;

	// Read byte //
	for (i = 7; i >= 0; i--)
	{
		app_i2c_ll_sleep(i2c->args->op_delay_ms);
		
		// Set SCL loose
		ret = app_i2c_ll_SCL_in(i2c->args->scl);
		if (ret != ESP_OK)
			goto app_i2c_read_byte_error;

		// Wait for SCL high
		ret = app_i2c_wait_while_clock_stretching(i2c->args->scl, i2c->args->scl_period_ms);
		if (ret != ESP_OK)
			goto app_i2c_read_byte_error;

		// Read SDA bit
		app_i2c_ll_SDA_read(i2c->args->sda, &level);
		*data |= level << i;

		// Set SCL low
		ret = app_i2c_ll_SCL_out(i2c->args->scl);
		if (ret != ESP_OK)
			goto app_i2c_read_byte_error;
	}

	// Send ACK/NACK //
	if (ack)
		ret = app_i2c_ll_SDA_out(i2c->args->sda); // ACK (low)
	else
		ret = app_i2c_ll_SDA_in(i2c->args->sda); // NACK (high)
	if (ret != ESP_OK)
		goto app_i2c_read_byte_error;
	app_i2c_ll_sleep(i2c->args->op_delay_ms);
	
	// Set SCL loose
	ret = app_i2c_ll_SCL_in(i2c->args->scl);
	if (ret != ESP_OK)
		goto app_i2c_read_byte_error;
	app_i2c_ll_sleep(i2c->args->op_delay_ms);

	// Wait for SCL high.
	ret = app_i2c_wait_while_clock_stretching(i2c->args->scl, i2c->args->scl_period_ms);
	if (ret != ESP_OK)
		goto app_i2c_read_byte_error;

	// Set SCL low.
	ret = app_i2c_ll_SCL_out(i2c->args->scl);
	if (ret != ESP_OK)
		goto app_i2c_read_byte_error;
	
	// Set SDA loose.
	ret = app_i2c_ll_SDA_in(i2c->args->sda);
	if (ret != ESP_OK)
		goto app_i2c_read_byte_error;

	return ESP_OK;

app_i2c_read_byte_error:
	ESP_LOGE(TAG,
		"Error reading byte with I2C handle \"%.*s\".",
		I2C_NAME_SIZE, i2c->name
	);
	return ret;
}





/* I2C read/write methods */

esp_err_t app_i2c_write(
		app_i2c_handle_t *i2c     ,
		uint8_t           address ,
		uint8_t const    *data    ,
		uint16_t          count   )
{
	ESP_LOGD(TAG,
		"Writing %d bytes with I2C handle \"%.*s\" to device address \"%d\".",
		count,
		I2C_NAME_SIZE, i2c->name,
		address
	);

	esp_err_t ret;
	
	uint16_t i;

	ESP_LOGV(TAG, "Sending START condition.");
	ret = app_i2c_start(i2c);
	if (ret != ESP_OK)
		goto app_i2c_write_error;

	ESP_LOGV(TAG, "Writing device address.");
	ret = app_i2c_write_byte(i2c, address << 1); // read byte 0
	if (ret != ESP_OK)
		goto app_i2c_write_error;

	ESP_LOGV(TAG, "Writing string of bytes into device.");
	for (i = 0; i < count; ++i)
	{
		ESP_LOGV(TAG, "Writing byte no. %d / %d.", i, count);
		ret = app_i2c_write_byte(i2c, data[i]);
		if (ret != ESP_OK)
			goto app_i2c_write_error;
	}

	ESP_LOGV(TAG, "Sending STOP condition.");
	ret = app_i2c_stop(i2c);
	if (ret != ESP_OK)
		goto app_i2c_write_error;

	return ESP_OK;

app_i2c_write_error:
	ESP_LOGE(TAG,
		"Error writing with I2C handle \"%.*s\". Sending STOP condition.",
		I2C_NAME_SIZE, i2c->name
	);
	app_i2c_stop(i2c);
	return ret;
}

esp_err_t app_i2c_read(
		app_i2c_handle_t *i2c     ,
		uint8_t           address ,
		uint8_t          *data    ,
		uint16_t          count   )
{
	ESP_LOGD(TAG,
		"Reading %d bytes with I2C handle \"%.*s\" from device address \"%d\".",
		count,
		I2C_NAME_SIZE, i2c->name,
		address
	);

	esp_err_t ret;

	ESP_LOGV(TAG, "Sending START condition.");
	ret = app_i2c_start(i2c);
	if (ret != ESP_OK)
		goto app_i2c_read_error;
	
	ESP_LOGV(TAG, "Writing address device.");
	ret = app_i2c_write_byte(i2c, (address << 1) | 1);
	if (ret != ESP_OK)
		goto app_i2c_read_error;

	uint16_t i;
	uint8_t  send_ack;

	ESP_LOGV(TAG, "Writing string of bytes into SGP30.");
	for (i = 0; i < count; ++i)
	{
		ESP_LOGV(TAG, "Reading byte no. %d / %d.", i, count);

		send_ack = i < (count - 1); // last byte must be NACK'ed
		ret = app_i2c_read_byte(i2c, send_ack, data + i);
		if (ret != ESP_OK)
			goto app_i2c_read_error;
	}

	ESP_LOGV(TAG, "Sending STOP condition.");
	ret = app_i2c_stop(i2c);
	if (ret != ESP_OK)
		goto app_i2c_read_error;

	return ESP_OK;

app_i2c_read_error:
	ESP_LOGE(TAG,
		"Error reading with I2C handle \"%.*s\". Sending STOP condition.",
		I2C_NAME_SIZE, i2c->name
	);
	ret = app_i2c_stop(i2c);
	return ret;
}