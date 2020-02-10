// Maximum number of connected sensors for a module.
#define MAX_SENSORS 10

// Max number of chars in a sensor module reply.
#define MAX_SENSOR_REPLY_LENGTH 32

// Address range for sensor modules.
#define TOP_ADDRESS 127

// I2C addresses of sensor modules, please add new sensor types here appropriately.
#define SENSOR_TEMP_HUM 1
#define SENSOR_PM25 2
#define SENSOR_RAIN 3
#define SENSOR_CO2_NO2 4
#define SENSOR_GPS 5
#define SENSOR_LIGHT_UV 6