#include "app_i2c.h"

#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h" // delay

// LOG
#include "esp_log.h"
static const char *TAG = "APP_I2C_LOW_LEVEL";

#include "defines_log.h"
#define LOG_LOCAL_LEVEL APP_I2C_LL_LOG_LEVEL

esp_err_t app_i2c_ll_init_pins(
		uint8_t scl ,
		uint8_t sda )
{
	ESP_LOGD(TAG,
		"Initializing SCL (GPIO %d) and SDA (GPIO %d) pins.",
		scl, sda
	);

	// Always return ESP_OK.
	ESP_LOGV(TAG, "Resetting SCL (GPIO %d) pin", scl);
	gpio_reset_pin( (gpio_num_t) scl );

	ESP_LOGV(TAG, "Resetting SDA (GPIO %d) pin", sda);
	gpio_reset_pin( (gpio_num_t) sda );

	return ESP_OK;
}

esp_err_t app_i2c_ll_release_pins(
		uint8_t scl ,
		uint8_t sda )
{
	ESP_LOGD(TAG,
		"Releasing SCL (GPIO %d) and SDA (GPIO %d) pins.",
		scl, sda
	);

	// Always return ESP_OK.
	ESP_LOGV(TAG, "Resetting SCL (GPIO %d) pin", scl);
	gpio_reset_pin( (gpio_num_t) scl );

	ESP_LOGV(TAG, "Resetting SDA (GPIO %d) pin", sda);
	gpio_reset_pin( (gpio_num_t) sda );

	return ESP_OK;
}

esp_err_t app_i2c_ll_SDA_in(
		uint8_t sda )
{
	ESP_LOGD(TAG,
		"Setting SDA (GPIO %d) pin as input with internal pull-up.",
		sda
	);

	esp_err_t ret;

	// Set as input
	ESP_LOGV(TAG, "Setting SDA (GPIO %d) pin as input.", sda);
	ret = gpio_set_direction( (gpio_num_t) sda, GPIO_MODE_INPUT);
	if (ret == ESP_ERR_INVALID_ARG)
	{
		ESP_LOGE(
			TAG,
			"INVALID_ARG error while setting SDA (GPIO %d) as input.",
			sda );
		return ret;
	}

	// Enable internal pull-up
	ESP_LOGV(TAG, "Activating SDA (GPIO %d) pin internal pull-up.", sda);
	ret = gpio_set_pull_mode( (gpio_num_t) sda, GPIO_PULLUP_ONLY);
	if (ret == ESP_ERR_INVALID_ARG)
	{
		ESP_LOGE(
			TAG,
			"INVALID_ARG error while setting SDA (GPIO %d) pull-up resistor.",
			sda );
		return ret;
	}

	return ret;
}

esp_err_t app_i2c_ll_SDA_out(
		uint8_t sda )
{
	ESP_LOGD(TAG,
		"Setting SDA (GPIO %d) pin as output with LOW level.",
		sda
	);

	esp_err_t ret;

	// Disable internal pull-up
	ESP_LOGV(TAG, "Disabling SDA (GPIO %d) pin internal pull-up.", sda);
	ret = gpio_set_pull_mode( (gpio_num_t) sda, GPIO_FLOATING);
	if (ret == ESP_ERR_INVALID_ARG)
	{
		ESP_LOGE(
			TAG,
			"INVALID_ARG error while setting SDA (GPIO %d) as floating.",
			sda );
		return ret;
	}

	// As output
	ESP_LOGV(TAG, "Setting SDA (GPIO %d) pin as input.", sda);
	gpio_set_direction( (gpio_num_t) sda, GPIO_MODE_OUTPUT);
	if (ret == ESP_ERR_INVALID_ARG)
	{
		ESP_LOGE(
			TAG,
			"INVALID_ARG error while setting SDA (GPIO %d) as output.",
			sda );
		return ret;
	}

	// Set to LOW
	ret = gpio_set_level( (gpio_num_t) sda, 0);
	if (ret == ESP_ERR_INVALID_ARG)
	{
		ESP_LOGE(
			TAG,
			"INVALID_ARG error while setting SDA (GPIO %d) output to LOW.",
			sda );
		return ret;
	}

	return ret;
}

esp_err_t app_i2c_ll_SDA_read(
		uint8_t  sda   ,
		uint8_t *level )
{
	ESP_LOGD(TAG,
		"Reading bit from SDA (GPIO %d) pin.",
		sda
	);
	
	// Always return 0 or 1.
	*level = gpio_get_level( (gpio_num_t) sda);

	return ESP_OK;
}

esp_err_t app_i2c_ll_SCL_in(
		uint8_t scl )
{
	ESP_LOGD(TAG,
		"Setting SCL (GPIO %d) pin as input with internal pull-up.",
		scl
	);
	
	esp_err_t ret;

	// As input
	ret = gpio_set_direction( (gpio_num_t) scl, GPIO_MODE_INPUT);
	if (ret == ESP_ERR_INVALID_ARG)
	{
		ESP_LOGE(
			TAG,
			"INVALID_ARG error while setting SCL (GPIO %d) as input.",
			scl );
		return ret;
	}

	// Enable internal pull-up
	ret = gpio_set_pull_mode( (gpio_num_t) scl, GPIO_PULLUP_ONLY);
	if (ret == ESP_ERR_INVALID_ARG)
	{
		ESP_LOGE(
			TAG,
			"INVALID_ARG error while setting SCL (GPIO %d) pull-up resistor.",
			scl );
		return ret;
	}
	
	return ret;
}

esp_err_t app_i2c_ll_SCL_out(
		uint8_t scl )
{
	ESP_LOGD(TAG,
		"Setting SCL (GPIO %d) pin as output with LOW level.",
		scl
	);

	esp_err_t ret;

	// Disable internal pull-up
	ret = gpio_set_pull_mode( (gpio_num_t) scl, GPIO_FLOATING);
	if (ret == ESP_ERR_INVALID_ARG)
	{
		ESP_LOGE(
			TAG,
			"INVALID_ARG error while setting SCL (GPIO %d) as floating.",
			scl );
		return ret;
	}

	// As output
	gpio_set_direction( (gpio_num_t) scl, GPIO_MODE_OUTPUT);
	if (ret == ESP_ERR_INVALID_ARG)
	{
		ESP_LOGE(
			TAG,
			"INVALID_ARG error while setting SCL (GPIO %d) as output.",
			scl );
		return ret;
	}

	// Set to LOW
	ret = gpio_set_level( (gpio_num_t) scl, 0);
	if (ret == ESP_ERR_INVALID_ARG)
	{
		ESP_LOGE(
			TAG,
			"INVALID_ARG error while setting SCL (GPIO %d) output to LOW.",
			scl );
		return ret;
	}

	return ret;
}

esp_err_t app_i2c_ll_SCL_read(
		uint8_t  scl   ,
		uint8_t *level )
{
	ESP_LOGD(TAG,
		"Reading bit from SCL (GPIO %d) pin.",
		scl
	);

	// Always return 0 or 1.
	*level = gpio_get_level( (gpio_num_t) scl);

	return ESP_OK;
}

void app_i2c_ll_sleep(
		uint32_t ms )
{
	ESP_LOGD(TAG,
		"I2C logic sleep for %d ms.",
		ms
	);

	vTaskDelay( ms / portTICK_PERIOD_MS );
}
