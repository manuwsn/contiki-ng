#ifndef LT_DATA_H_
#define LT_DATA_H_

#include "routing/rpl-lite/rpl.h"
#include "ipv6/uip.h"

typedef struct  {
  uint32_t voltage;    // node voltage (1/100 V)
#ifdef SENSOR_BMP180
  uint16_t sensors[3];
#endif
#ifdef SENSOR_SHT25
  uint16_t sensors[3];
#endif
#ifdef SENSOR_DHT22
  uint16_t sensors[3];
#endif
#ifdef SENSOR_LDR
  uint16_t sensors[2];
#endif
#ifdef SENSOR_DLS
  uint16_t sensors[2];
#endif
  
} ltdata_t;

typedef struct {
  uint32_t voltage;
  uint16_t sensors[3];
} ltdatarcv_t;


typedef struct {
#ifdef RPL_STATS
  uint16_t rpl_stats[8];
#endif
#ifdef UIP_STATS
  uint16_t uip_stats[4];
#endif
} ltstatsdata_t;

#include "lt-data-zoul.h"

#ifdef RPL_STATS
enum {
  SEND_DIO,
  RCV_DIO,
  SEND_DAO,
  RCV_DAO,
  SEND_DIS,
  RCV_DIS,
  SEND_ACKDAO,
  RCV_ACKDAO,
};
#endif

#ifdef UIP_STATS
enum {
  SEND_IP,
  RCV_IP,
  FWD_IP,
  DROP_IP,
};
#endif

void ltdata_read_stats_data(ltstatsdata_t *msgl);
void ltdata_read_sensor_data(ltdata_t *msgl);
void ltdata_update_stats_data(ltstatsdata_t *ltsd,ltstatsdata_t *oldltsd);

#endif /* LT_DATA_H_ */
