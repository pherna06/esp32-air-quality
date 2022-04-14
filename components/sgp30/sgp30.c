#include "sgp30.h"

#include "sgp30_i2c.h"

static const char *TAG= "SGP30";
#include "esp_log.h"

#define SGP30_PRODUCT_TYPE 0
static const uint8_t SGP30_I2C_ADDRESS = 0x58;

/* command and constants for reading the serial ID */
#define SGP30_CMD_GET_SERIAL_ID              0x3682
#define SGP30_CMD_GET_SERIAL_ID_DURATION_US  500
#define SGP30_CMD_GET_SERIAL_ID_WORDS        3

/* command and constants for reading the featureset version */
#define SGP30_CMD_GET_FEATURESET              0x202f
#define SGP30_CMD_GET_FEATURESET_DURATION_MS  10
#define SGP30_CMD_GET_FEATURESET_WORDS        1

/* command and constants for on-chip self-test */
#define SGP30_CMD_MEASURE_TEST              0x2032
#define SGP30_CMD_MEASURE_TEST_DURATION_MS  220
#define SGP30_CMD_MEASURE_TEST_WORDS        1
#define SGP30_CMD_MEASURE_TEST_OK           0xd400

/* command and constants for IAQ init */
#define SGP30_CMD_IAQ_INIT              0x2003
#define SGP30_CMD_IAQ_INIT_DURATION_MS  10

/* command and constants for IAQ measure */
#define SGP30_CMD_IAQ_MEASURE              0x2008
#define SGP30_CMD_IAQ_MEASURE_DURATION_MS  12
#define SGP30_CMD_IAQ_MEASURE_WORDS        2

/* command and constants for getting IAQ baseline */
#define SGP30_CMD_GET_IAQ_BASELINE              0x2015
#define SGP30_CMD_GET_IAQ_BASELINE_DURATION_MS  10
#define SGP30_CMD_GET_IAQ_BASELINE_WORDS        2

/* command and constants for setting IAQ baseline */
#define SGP30_CMD_SET_IAQ_BASELINE              0x201e
#define SGP30_CMD_SET_IAQ_BASELINE_DURATION_MS  10

/* command and constants for raw measure */
#define SGP30_CMD_RAW_MEASURE              0x2050
#define SGP30_CMD_RAW_MEASURE_DURATION_MS  25
#define SGP30_CMD_RAW_MEASURE_WORDS        2

/* command and constants for setting absolute humidity */
#define SGP30_CMD_SET_ABSOLUTE_HUMIDITY              0x2061
#define SGP30_CMD_SET_ABSOLUTE_HUMIDITY_DURATION_MS  10

/* command and constants for getting TVOC inceptive baseline */
#define SGP30_CMD_GET_TVOC_INCEPTIVE_BASELINE              0x20b3
#define SGP30_CMD_GET_TVOC_INCEPTIVE_BASELINE_DURATION_MS  10
#define SGP30_CMD_GET_TVOC_INCEPTIVE_BASELINE_WORDS        1

/* command and constants for setting TVOC baseline */
#define SGP30_CMD_SET_TVOC_BASELINE              0x2077
#define SGP30_CMD_SET_TVOC_BASELINE_DURATION_MS  10

static esp_err_t sgp30_get_feature_set_version(
		sgp30_handle_t *sgp30               ,
		uint16_t       *feature_set_version ,
		uint8_t        *product_type        )
{
	esp_err_t ret;
	uint16_t words[SGP30_CMD_GET_FEATURESET_WORDS];


	ret = sgp30_i2c_delayed_read_cmd(
		&(sgp30->i2c)                        ,
		SGP30_I2C_ADDRESS                    ,
		SGP30_CMD_GET_FEATURESET             ,
		SGP30_CMD_GET_FEATURESET_DURATION_MS ,
		words                                ,
		SGP30_CMD_GET_FEATURESET_WORDS       );

	if (ret != ESP_OK)
		return ret;

	*feature_set_version = words[0] & 0x00FF;
	*product_type = (uint8_t) ((words[0] & 0xF000) >> 12);

	return ESP_OK;
}

static esp_err_t sgp30_check_featureset(
		sgp30_handle_t *sgp30     ,
		uint16_t        needed_fs )
{
	ESP_LOGD(TAG, "Checking SGP30 features.");

	esp_err_t ret;

	uint16_t fs_version;
	uint8_t  product_type;

	ESP_LOGV(TAG, "Retrieving feature set version.");
	ret = sgp30_get_feature_set_version(
		sgp30         ,
		&fs_version   ,
		&product_type );
	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG, "Error getting set version while checking features.");
		return ret;
	}

	ESP_LOGV(TAG, "Checking product type match.");
	if (product_type != SGP30_PRODUCT_TYPE)
	{
		ESP_LOGE(TAG, "Product type error while checking features.");
		return ESP_ERR_INVALID_ARG;
	}

	ESP_LOGV(TAG, "Checking retrieved version nice agains provided.");
	if (fs_version < needed_fs)
	{
		ESP_LOGE(TAG,
			"Unsupported feature set: %d (min: %d) while checking features.",
			fs_version, needed_fs
		);
		return ESP_ERR_INVALID_ARG;
	}

	return ESP_OK;
}

esp_err_t sgp30_measure_test(
		sgp30_handle_t *sgp30       ,
		uint16_t       *test_result )
{
	ESP_LOGD(TAG, "TODO");

	esp_err_t ret;

	uint16_t measure_test_word_buf[SGP30_CMD_MEASURE_TEST_WORDS];

	*test_result = 0;

	ret = sgp30_i2c_delayed_read_cmd(
		&(sgp30->i2c)                              ,
		SGP30_I2C_ADDRESS                          ,
		SGP30_CMD_MEASURE_TEST                     ,
		SGP30_CMD_MEASURE_TEST_DURATION_MS         ,
		measure_test_word_buf                      ,
		SGP30_I2C_NUM_WORDS(measure_test_word_buf) );
	if (ret != ESP_OK)
		goto measure_test_error;

	*test_result = *measure_test_word_buf;
	if (*test_result == SGP30_CMD_MEASURE_TEST_OK)
		return ESP_OK;

measure_test_error:
	ESP_LOGE(TAG, "Measure test failed.");
	return ret;
}

esp_err_t sgp30_measure_iaq(
		sgp30_handle_t *sgp30 )
{
	ESP_LOGD(TAG, "Signaling SGP30 to start iaq measure.");

	return sgp30_i2c_write_cmd(
		&(sgp30->i2c)         ,
		SGP30_I2C_ADDRESS     ,
		SGP30_CMD_IAQ_MEASURE );
}

esp_err_t sgp30_read_iaq(
		sgp30_handle_t *sgp30      ,
		uint16_t       *tvoc_ppb   ,
		uint16_t       *co2_eq_ppm )
{
	ESP_LOGD(TAG, "Reading SGP30 iaq measured data.");

	esp_err_t ret; 

	uint16_t words[SGP30_CMD_IAQ_MEASURE_WORDS];

	ret = sgp30_i2c_read_words(
		&(sgp30->i2c)               ,
		SGP30_I2C_ADDRESS           ,
		words                       ,
		SGP30_CMD_IAQ_MEASURE_WORDS );
	
	*tvoc_ppb   = words[1];
	*co2_eq_ppm = words[0];

	if (ret != ESP_OK)
		ESP_LOGE(TAG, "Error reading words from iaq measured data.");

	return ret;
}

esp_err_t sgp30_measure_iaq_blocking_read(
		sgp30_handle_t *sgp30      ,
		uint16_t       *tvoc_ppb   ,
		uint16_t       *co2_eq_ppm )
{
	ESP_LOGD(TAG, "Measuring and reading SGP30 iaq with blocking.");

	esp_err_t ret;

	ESP_LOGV(TAG, "Signaling SGP30 measure iaq.");
	ret = sgp30_measure_iaq(sgp30);
	if (ret != ESP_OK)
		return ret;

	sgp30_i2c_sleep(SGP30_CMD_IAQ_MEASURE_DURATION_MS);

	ESP_LOGV(TAG, "Reading SGP30 iaq after measure time.");
	return sgp30_read_iaq(sgp30, tvoc_ppb, co2_eq_ppm);
}

esp_err_t sgp30_measure_tvoc(
		sgp30_handle_t *sgp30 )
{
	ESP_LOGD(TAG, "Signaling SGP30 to start tvoc measure.");
	return sgp30_measure_iaq(sgp30);
}

esp_err_t sgp30_measure_co2_eq(
		sgp30_handle_t *sgp30 )
{
	ESP_LOGD(TAG, "Signaling SGP30 to start co2_eq measure.");
	return sgp30_measure_iaq(sgp30);
}

esp_err_t sgp30_read_tvoc(
		sgp30_handle_t *sgp30    ,
		uint16_t       *tvoc_ppb )
{
	ESP_LOGD(TAG, "Reading SGP30 tvoc measured data.");
	uint16_t co2_eq_ppm;
	return sgp30_read_iaq(sgp30, tvoc_ppb, &co2_eq_ppm);
}

esp_err_t sgp30_read_co2_eq(
		sgp30_handle_t *sgp30      ,
		uint16_t       *co2_eq_ppm )
{
	ESP_LOGD(TAG, "Reading SGP30 co2_eq measured data.");
	uint16_t tvoc_ppb;
	return sgp30_read_iaq(sgp30, &tvoc_ppb, co2_eq_ppm);
}

esp_err_t sgp30_measure_tvoc_blocking_read(
		sgp30_handle_t *sgp30    ,
		uint16_t       *tvoc_ppb )
{
	ESP_LOGD(TAG, "Measuring and reading SGP30 tvoc with blocking.");
	uint16_t co2_eq_ppm;
	return sgp30_measure_iaq_blocking_read(sgp30, tvoc_ppb, &co2_eq_ppm);
}

esp_err_t sgp30_measure_co2_eq_blocking_read(
		sgp30_handle_t *sgp30      ,
		uint16_t       *co2_eq_ppm )
{
	ESP_LOGD(TAG, "Measuring and reading SGP30 co2_eq with blocking.");
	uint16_t tvoc_ppb;
	return sgp30_measure_iaq_blocking_read(sgp30, &tvoc_ppb, co2_eq_ppm);
}

esp_err_t sgp30_measure_raw(
		sgp30_handle_t *sgp30 )
{
	ESP_LOGD(TAG, "Signaling SGP30 to start raw measure.");
	return sgp30_i2c_write_cmd(
		&(sgp30->i2c)         ,
		SGP30_I2C_ADDRESS     ,
		SGP30_CMD_RAW_MEASURE );
}

esp_err_t sgp30_read_raw(
		sgp30_handle_t *sgp30              ,
		uint16_t       *ethanol_raw_signal ,
		uint16_t       *h2_raw_signal      )
{
	ESP_LOGD(TAG, "Reading SGP30 raw measured data.");

	esp_err_t ret;

	uint16_t words[SGP30_CMD_RAW_MEASURE_WORDS];

	ret = sgp30_i2c_read_words(
		&(sgp30->i2c)               ,
		SGP30_I2C_ADDRESS           ,
		words                       ,
		SGP30_CMD_RAW_MEASURE_WORDS );

	*ethanol_raw_signal = words[1];
	*h2_raw_signal      = words[0];

	if (ret != ESP_OK)
		ESP_LOGE(TAG, "Error reading words from raw measured data.");

	return ret;
}

esp_err_t sgp30_measure_raw_blocking_read(
		sgp30_handle_t *sgp30              ,
		uint16_t       *ethanol_raw_signal ,
		uint16_t       *h2_raw_signal      )
{
	ESP_LOGD(TAG, "Measuring and reading SGP30 raw with blocking.");

	esp_err_t ret;

	ESP_LOGV(TAG, "Signaling SGP30 measure raw.");
	ret = sgp30_measure_raw(sgp30);
	if (ret != ESP_OK)
		return ret;

	sgp30_i2c_sleep(SGP30_CMD_RAW_MEASURE_DURATION_MS);

	ESP_LOGV(TAG, "Reading SGP30 raw after measure time.");
	return sgp30_read_raw(sgp30, ethanol_raw_signal, h2_raw_signal);
}

esp_err_t sgp30_get_iaq_baseline(
		sgp30_handle_t *sgp30    ,
		uint32_t       *baseline )
{
	ESP_LOGD(TAG, "Retrieving SGP30 iaq baseline.");
	
	esp_err_t ret;

	uint16_t words[SGP30_CMD_GET_IAQ_BASELINE_WORDS];

	ESP_LOGV(TAG, "Signaling SGP30 to prepare baseline data.");
	ret = sgp30_i2c_write_cmd(
		&(sgp30->i2c)              ,
		SGP30_I2C_ADDRESS          ,
		SGP30_CMD_GET_IAQ_BASELINE );
	if (ret != ESP_OK)
		goto get_iaq_baseline_error;

	sgp30_i2c_sleep(SGP30_CMD_GET_IAQ_BASELINE_DURATION_MS);

	ESP_LOGV(TAG, "Reading SGP30 retrieved iaq baseline.");
	ret = sgp30_i2c_read_words(
		&(sgp30->i2c)                    ,
		SGP30_I2C_ADDRESS                ,
		words                            ,
		SGP30_CMD_GET_IAQ_BASELINE_WORDS );
	if (ret != ESP_OK)
		goto get_iaq_baseline_error;

	*baseline = ( (uint32_t) words[1] << 16 ) | ( (uint32_t) words[0] );

	if (*baseline)
		return ESP_OK;

	ret = ESP_FAIL;

get_iaq_baseline_error:
	ESP_LOGE(TAG, "Error trying to retrieve SGP30 iaq baseline.");
	return ret;
}

esp_err_t sgp30_set_iaq_baseline(
		sgp30_handle_t *sgp30    ,
		uint32_t        baseline )
{
	ESP_LOGD(TAG, "Setting SGP30 iaq baseline.");
	
	esp_err_t ret;

	if (!baseline)
	{
		ret = ESP_ERR_INVALID_ARG;
		goto set_iaq_baseline_error;
	}

	uint16_t words[2] = {
		(uint16_t) ((baseline & 0xffff0000) >> 16) ,
		(uint16_t) (baseline & 0x0000ffff)         };

	ret = sgp30_i2c_write_cmd_with_args(
		&(sgp30->i2c)              ,
		SGP30_I2C_ADDRESS          ,
		SGP30_CMD_SET_IAQ_BASELINE ,
		words                      ,
		SGP30_I2C_NUM_WORDS(words) );
	if (ret != ESP_OK)
		goto set_iaq_baseline_error;

	sgp30_i2c_sleep(SGP30_CMD_SET_IAQ_BASELINE_DURATION_MS);

	return ESP_OK;

set_iaq_baseline_error:
	ESP_LOGE(TAG, "Error trying to set SGP30 iaq baseline.");
	return ret;
}

esp_err_t sgp30_get_tvoc_inceptive_baseline(
		sgp30_handle_t *sgp30                   ,
		uint16_t       *tvoc_inceptive_baseline )
{
	ESP_LOGD(TAG, "Retrieving SGP30 tvoc inceptive baseline.");

	esp_err_t ret;

	ret = sgp30_check_featureset(sgp30, 0x21);
	if (ret != ESP_OK)
		goto get_tvoc_inceptive_baseline_error;

	ret = sgp30_i2c_write_cmd(
		&(sgp30->i2c)                         ,
		SGP30_I2C_ADDRESS                     ,
		SGP30_CMD_GET_TVOC_INCEPTIVE_BASELINE );
	if (ret != ESP_OK)
		goto get_tvoc_inceptive_baseline_error;

	sgp30_i2c_sleep(SGP30_CMD_GET_TVOC_INCEPTIVE_BASELINE_DURATION_MS);

	ret = sgp30_i2c_read_words(
		&(sgp30->i2c)                               ,
		SGP30_I2C_ADDRESS                           ,
		tvoc_inceptive_baseline                     ,
		SGP30_CMD_GET_TVOC_INCEPTIVE_BASELINE_WORDS );
	if (ret != ESP_OK)
		goto get_tvoc_inceptive_baseline_error;

	return ESP_OK;
get_tvoc_inceptive_baseline_error:
	ESP_LOGE(TAG, "Error trying to retrieve SGP30 tvoc inceptive baseline.");
	return ret;
}

esp_err_t sgp30_set_tvoc_baseline(
		sgp30_handle_t *sgp30         ,
		uint16_t        tvoc_baseline )
{
	ESP_LOGD(TAG, "Setting SGP30 tvoc baseline.");

	esp_err_t ret;

	ret = sgp30_check_featureset(sgp30, 0x21);
	if (ret != ESP_OK)
		goto set_tvoc_baseline_error;

	if (!tvoc_baseline)
	{
		ret = ESP_FAIL;
		goto set_tvoc_baseline_error;
	}

	ret = sgp30_i2c_write_cmd_with_args(
		&(sgp30->i2c)                      ,
		SGP30_I2C_ADDRESS                  ,
		SGP30_CMD_SET_TVOC_BASELINE        ,
		&tvoc_baseline                     ,
		SGP30_I2C_NUM_WORDS(tvoc_baseline) );
	if (ret != ESP_OK)
		goto set_tvoc_baseline_error;

	sgp30_i2c_sleep(SGP30_CMD_SET_TVOC_BASELINE_DURATION_MS);

	return ESP_OK;

set_tvoc_baseline_error:
	ESP_LOGE(TAG, "Error trying to set SGP30 tvoc baseline.");
	return ret;
}

esp_err_t sgp30_set_absolute_humidity(
		sgp30_handle_t *sgp30             ,
		uint32_t        absolute_humidity )
{
	ESP_LOGD(TAG, "Setting SGP30 absolute humidity.");

	esp_err_t ret;

	uint16_t ah_scaled;

	if (absolute_humidity > 256000)
	{
		ret = ESP_ERR_INVALID_ARG;
		goto set_absolute_humidity_error;
	}

	ah_scaled = (uint16_t) ((absolute_humidity * 16777) >> 16);

	ret = sgp30_i2c_write_cmd_with_args(
		&(sgp30->i2c)                   ,
		SGP30_I2C_ADDRESS               ,
		SGP30_CMD_SET_ABSOLUTE_HUMIDITY ,
		&ah_scaled                      ,
		SGP30_I2C_NUM_WORDS(ah_scaled)  );
	if (ret != ESP_OK)
		goto set_absolute_humidity_error;

	sgp30_i2c_sleep(SGP30_CMD_SET_ABSOLUTE_HUMIDITY_DURATION_MS);

	return ESP_OK;

set_absolute_humidity_error:
	ESP_LOGE(TAG, "Error trying to set SGP30 absolute humidity.");
	return ret;
}

esp_err_t sgp30_iaq_init(
		sgp30_handle_t *sgp30 )
{
	ESP_LOGD(TAG, "Initializing SGP30.");

	esp_err_t ret;
	ret = sgp30_i2c_write_cmd(
		&(sgp30->i2c)      ,
		SGP30_I2C_ADDRESS  ,
		SGP30_CMD_IAQ_INIT );

	sgp30_i2c_sleep(SGP30_CMD_IAQ_INIT_DURATION_MS);
	
	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG, "Error trying to initialize SGP30.");
		return ret;
	}

	return ESP_OK;
}

esp_err_t sgp30_probe(
		sgp30_handle_t *sgp30 )
{
	ESP_LOGD(TAG, "Probing SGP30.");

	esp_err_t ret;

	ret = sgp30_check_featureset(sgp30, 0x21);

	if (ret != ESP_OK)
		return ret;

	return sgp30_iaq_init(sgp30);
}

uint8_t sgp30_get_configured_address()
{
	return SGP30_I2C_ADDRESS;
}


esp_err_t sgp30_create(
		const char     *name  ,
		uint8_t         scl   ,
		uint8_t         sda   ,
		sgp30_handle_t *sgp30 )
{
	esp_err_t ret;

	app_i2c_config_args_t config = {
		.scl           = scl                               ,
		.sda           = sda                               ,
		.scl_period_ms = SGP30_I2C_SCL_CLOCK_PERIOD_MS     ,
		.op_delay_ms   = SGP30_I2C_SCL_CLOCK_PERIOD_MS / 2 };

	ret = app_i2c_create(
		name             ,
		&config          ,
		&(sgp30->i2c)    );

	if (ret != ESP_OK)
		ESP_LOGE(TAG, "Error creating SGP30 handle.");

	return ret;
}


esp_err_t sgp30_delete(
		sgp30_handle_t *sgp30 )
{
	return app_i2c_delete(&(sgp30->i2c));
}