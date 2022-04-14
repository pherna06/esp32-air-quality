#include "defines_debug.h"

#ifdef DEBUG_CONFIG
#define MQTT_BROKER_URI "matt://127.0.0.1:8384"
#define CONFIG_MQTT_BROKER_CERTIFICATE 1
#else
#define MQTT_BROKER_URI CONFIG_MQTT_BROKER_URI
#endif

#ifdef CONFIG_MQTT_BROKER_CERTIFICATE
extern const uint8_t app_mqtt_server_pem_start[]
	asm("_binary_app_mqtt_server_pem_start");
extern const uint8_t app_mqtt_server_pem_end[]
	asm("_binary_app_mqtt_server_pem_end");
#endif

#ifdef DEBUG_CONFIG
#define SGP30_GPIO_SCL  18
#define SGP30_GPIO_SDA  19
#else
#define SGP30_GPIO_SCL  CONFIG_SGP30_GPIO_SCL
#define SGP30_GPIO_SDA  CONFIG_SGP30_GPIO_SDA
#endif

#ifdef DEBUG_CONFIG
#define SI7021_GPIO_SCL  16
#define SI7021_GPIO_SDA  17
#else
#define SI7021_GPIO_SCL  CONFIG_SI7021_GPIO_SCL
#define SI7021_GPIO_SDA  CONFIG_SI7021_GPIO_SDA
#endif