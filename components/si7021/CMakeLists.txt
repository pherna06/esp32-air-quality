idf_component_register(
    SRCS si7021.c
    INCLUDE_DIRS "include"
    REQUIRES i2cdev
	PRIV_REQUIRES log esp_idf_lib_helpers
)