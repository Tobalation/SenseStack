// Maximum number of connected sensors for a module.
#define MAX_SENSORS 10

// Max number of chars in a sensor module reply.
#define MAX_SENSOR_REPLY_LENGTH 32

// Address range for sensor modules.
#define TOP_ADDRESS 127

// I2C addresses of sensor modules, please add new sensor types here appropriately.
#define SENSOR_TEMP_HUM 11
#define SENSOR_PM25 12
#define SENSOR_RAIN 13
#define SENSOR_CO2_NO2 14
#define SENSOR_GPS 15
#define SENSOR_LIGHT_UV 16

// Data type macros
#define CH_TYPE_INT 'i'
#define CH_TYPE_FLOAT 'f'
#define CH_TYPE_BOOL 'b'
#define CH_TYPE_CUSTOM 'c'
#define CH_TERMINATOR 't'
