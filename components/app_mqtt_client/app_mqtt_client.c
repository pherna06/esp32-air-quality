#include <stdio.h>

#include "esp_log.h"
#include "mqtt_client.h"
#include "defines.h"

static const char *TAG = "APP_MQTT_CLIENT";

#ifdef CONFIG_MQTT_BROKER_CERTIFICATE
extern const uint8_t app_mqtt_server_pem_start[] asm("_binary_app_mqtt_server_pem_start");
extern const uint8_t app_mqtt_server_pem_end[]   asm("_binary_app_mqtt_server_pem_end");
#endif

typedef struct
{
	esp_mqtt_client_handle_t client;
	// TODO elemento sincronización
} app_mqtt_client_handle_t;


static void app_mqtt_event_handler(
		void             *handler_args ,
		esp_event_base_t  base         ,
		int32_t           event_id     ,
		void             *event_data
)
{
	ESP_LOGD(TAG,
		"Event dispatched from event loop base=%s, event_id=%d",
		base, event_id
	);

	esp_mqtt_event_handle_t  event  = event_data;
	esp_mqtt_client_handle_t client = event->client;

	switch ((esp_mqtt_event_id_t)event_id)
	{
		case MQTT_EVENT_CONNECTED:
			ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED. msg_id=%d", event->msg_id);
			break;
		case MQTT_EVENT_DISCONNECTED:
			ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED. msg_id=%d", event->msg_id);
			break;
		case MQTT_EVENT_SUBSCRIBED:
			ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
			break;
		case MQTT_EVENT_UNSUBSCRIBED:
			ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
			break;
		case MQTT_EVENT_PUBLISHED:
			ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
			break;
		case MQTT_EVENT_DATA:
			ESP_LOGI(TAG, "MQTT_EVENT_DATA");
			printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
			printf("DATA=%.*s\r\n", event->data_len, event->data);
			break;
		case MQTT_EVENT_ERROR:
			ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
			switch (event->error_handle->error_type)
			{
				case MQTT_ERROR_TYPE_TCP_TRANSPORT:
					ESP_LOGI(TAG,
						"Last error code reported from esp-tls: 0x%x",
						event->error_handle->esp_tls_last_esp_err
					);
					ESP_LOGI(TAG,
						"Last tls stack error number: 0x%x",
						event->error_handle->esp_tls_stack_err
					);
					ESP_LOGI(TAG,
						"Last captured errno : %d (%s)",
						event->error_handle->esp_transport_sock_errno,
						strerror(event->error_handle->esp_transport_sock_errno)
					);
					break;
				case MQTT_ERROR_TYPE_CONNECTION_REFUSED:
					ESP_LOGI(TAG,
						"Connection refused error: 0x%x",
						event->error_handle->connect_return_code
					);
					break;
				default:
					ESP_LOGW(TAG,
						"Unknown error type: 0x%x",
						event->error_handle->error_type
					);
					break;
			}
		default:
			ESP_LOGI(TAG, "Other event id:%d", event->event_id);
			break;
	}
}

static int app_mqtt_subscribe(
		app_mqtt_client_handle_t  *app_client ,
		char                      *topic      ,
		int                        qos        
)
{
	int result = esp_mqtt_client_subscribe(
		app_client->client,
		topic,
		qos
	);

	return result;
}

static int app_mqtt_unsubscribe(
		app_mqtt_client_handle_t  *app_client ,
		char                      *topic      
)
{
	int result = esp_mqtt_client_unsubscribe(
		app_client->client,
		topic
	);

	return result;
}

static int app_mqtt_publish(
		app_mqtt_client_handle_t  *app_client ,
		char                      *topic      ,
		char                      *data       ,
		int                        data_len   ,
		int                        qos        ,
		int                        retain     
)
{
	int result = esp_mqtt_client_publish( // blocking
		app_client->client,
		topic,
		data, data_len,
		qos, retain
	);

	return result;
}

static int app_mqtt_enqueue(
		app_mqtt_client_handle_t  *app_client ,
		char                      *topic      ,
		char                      *data       ,
		int                        data_len   ,
		int                        qos        ,
		int                        retain     ,
		bool                       store      
)
{
	int result = esp_mqtt_client_enqueue( // non-blocking
		app_client->client,
		topic,
		data, data_len,
		qos, retain,
		store // true to enqueue qos=0
	);

	return result;
}

static app_mqtt_client_handle_t *app_mqtt_ssl_client_start()
{
	// TODO
	// Elemento de sincronización en app_mqtt_client_handle_t

	esp_mqtt_client_config_t mqtt_cfg = {
		.uri = MQTT_BROKER_URI,
		.cert_pem = (const char *)app_mqtt_server_pem_start,
		.cert_len = app_mqtt_server_pem_end - app_mqtt_server_pem_start,
	};
	esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
	esp_mqtt_client_register_event(
		client,
		ESP_EVENT_ANY_ID,
		app_mqtt_event_handler,
		NULL
	);

	app_mqtt_client_handle_t *app_mqtt_client;
	app_mqtt_client = malloc(sizeof(app_mqtt_client_handle_t));
	app_mqtt_client->client = client;

	esp_mqtt_client_start(client);

	return app_mqtt_client;
}

static void app_mqtt_client_task(void *args)
{
	
	
	while(1)
	{

	}
}