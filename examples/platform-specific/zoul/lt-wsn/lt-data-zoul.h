#ifndef LT_DATA_ZOUL_H
#define LT_DATA_ZOUL_H

#include "lt-data.h"

#define BMP180 0
#define SHT25 1
#define DHT22 2
#define LDR 3
#define DLS 4

#ifdef SENSOR_BMP180
enum {
  SENSOR_TYPE,
  TEMPERATURE_SENSOR,
  PRESSION_SENSOR,
};
#elif SENSOR_SHT25
enum {
  SENSOR_TYPE,
  TEMPERATURE_SENSOR,
  HUMIDITY_SENSOR,
};
#elif SENSOR_DHT22
enum {
  SENSOR_TYPE,
  TEMPERATURE_SENSOR,
  HUMIDITY_SENSOR,
};
#elif SENSOR_LDR
enum {
  SENSOR_TYPE,
  LIGHT_SENSOR,
};
#elif SENSOR_DLS
enum {
  SENSOR_TYPE,
  LIGHT_SENSOR,
};
#else
enum{
  SENSOR_TYPE,
  DATA_1,
  DATA_2,
};
#endif



void ltdata_arch_read_sensors(ltdata_t *msg);

#endif /* COLLECT_VIEW_ZOUL_H */
