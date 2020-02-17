//TODO: Update protocol to support multiple data transmissions from a single module (length>32 transmissions).

// Maximum number of connected sensors for a module.
#define MAX_SENSORS 10

// Max number of chars in a sensor module reply.
#define MAX_SENSOR_REPLY_LENGTH 32

// JSON max reply length
#define MAX_JSON_REPLY 256

// Address range for sensor modules.
#define TOP_ADDRESS 127

// I2C addresses of sensor modules, please add new sensor types here appropriately.
#define SENSOR_TEMP_HUM 11
#define SENSOR_PM25 12
#define SENSOR_RAIN 13
#define SENSOR_CO2_NO2 14
#define SENSOR_GPS 15
#define SENSOR_LIGHT_UV 16
#define SENSOR_CO 17

// Data type macros
#define CH_DATA_NAME '$'
#define CH_TERMINATOR '^'
#define CH_DATA_BEGIN '#'