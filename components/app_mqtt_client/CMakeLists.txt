idf_component_register(SRCS "app_mqtt_client.c"
					   INCLUDE_DIRS "." "include"
					   PRIV_REQUIRES mqtt defines)