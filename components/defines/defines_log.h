#include "defines_debug.h"

#ifdef DEBUG_CONFIG
#define APP_I2C_LL_LOG_LEVEL 3
#else
#define APP_I2C_LL_LOG_LEVEL CONFIG_APP_I2C_UTILS_LOG_LEVEL
#endif
