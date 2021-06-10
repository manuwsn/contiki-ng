#include "contiki.h"

#include "sys/energest.h"
#include "lt-data.h"

#include <string.h>
#include <stdio.h>

/*---------------------------------------------------------------------------*/
void
ltdata_read_sensor_data(ltdata_t *msg)
{
  /*
  static uint64_t last_cpu, last_lpm, last_dlpm, last_transmit, last_listen;
  uint32_t cpu, lpm, dlpm, transmit, listen;
 
  energest_flush();
  
  cpu = (uint32_t)(energest_type_time(ENERGEST_TYPE_CPU) - last_cpu);
  lpm = (uint32_t)(energest_type_time(ENERGEST_TYPE_LPM) - last_lpm);
  dlpm = (uint32_t)(energest_type_time(ENERGEST_TYPE_DEEP_LPM) - last_dlpm);
  transmit = (uint32_t)(energest_type_time(ENERGEST_TYPE_TRANSMIT) - last_transmit);
  listen = (uint32_t)(energest_type_time(ENERGEST_TYPE_LISTEN) - last_listen);

  msg->cpu = cpu;
  msg->lpm = lpm;
  msg->dlpm = dlpm;
  msg->transmit = transmit;
  msg->listen = listen;
  //memcpy(&msg->listen,&listen,4);
 
  last_cpu = energest_type_time(ENERGEST_TYPE_CPU);
  last_lpm = energest_type_time(ENERGEST_TYPE_LPM);
  last_dlpm = energest_type_time(ENERGEST_TYPE_DEEP_LPM);
  last_transmit = energest_type_time(ENERGEST_TYPE_TRANSMIT);
  last_listen = energest_type_time(ENERGEST_TYPE_LISTEN);
  */
  ltdata_arch_read_sensors(msg);
}
/*---------------------------------------------------------------------------*/
void ltdata_update_stats_data(ltstatsdata_t *ltstd,ltstatsdata_t *oldltsd){
#ifdef RPL_STATS
  ltstd->rpl_stats[SEND_DIO] += oldltsd->rpl_stats[SEND_DIO];
  ltstd->rpl_stats[RCV_DIO] += oldltsd->rpl_stats[RCV_DIO];
  ltstd->rpl_stats[SEND_DAO] += oldltsd->rpl_stats[SEND_DAO];
  ltstd->rpl_stats[RCV_DAO] += oldltsd->rpl_stats[RCV_DAO];
  ltstd->rpl_stats[SEND_DIS] += oldltsd->rpl_stats[SEND_DIS];
  ltstd->rpl_stats[RCV_DIS] += oldltsd->rpl_stats[RCV_DIS];
  ltstd->rpl_stats[SEND_ACKDAO] += oldltsd->rpl_stats[SEND_ACKDAO];
  ltstd->rpl_stats[RCV_ACKDAO] += oldltsd->rpl_stats[RCV_ACKDAO];
#endif
  
#ifdef UIP_STATS
  ltstd->uip_stats[SEND_IP] += oldltsd->uip_stats[SEND_IP];
  ltstd->uip_stats[RCV_IP] += oldltsd->uip_stats[RCV_IP];
  ltstd->uip_stats[FWD_IP] += oldltsd->uip_stats[FWD_IP];
  ltstd->uip_stats[DROP_IP] += oldltsd->uip_stats[DROP_IP];
#endif
}
/*---------------------------------------------------------------------------*/
void 
ltdata_read_stats_data(ltstatsdata_t *msg)
{
#ifdef RPL_STATS
  memcpy(&msg->rpl_stats[SEND_DIO], &rpl_stats.send_dios, sizeof(uint16_t));
  memcpy(&msg->rpl_stats[RCV_DIO], &rpl_stats.received_dios, sizeof(uint16_t));
  memcpy(&msg->rpl_stats[SEND_DAO], &rpl_stats.send_daos, sizeof(uint16_t));
  memcpy(&msg->rpl_stats[RCV_DAO], &rpl_stats.received_daos, sizeof(uint16_t));
  memcpy(&msg->rpl_stats[SEND_DIS], &rpl_stats.send_diss, sizeof(uint16_t));
  memcpy(&msg->rpl_stats[RCV_DIS], &rpl_stats.received_diss, sizeof(uint16_t));
  memcpy(&msg->rpl_stats[SEND_ACKDAO], &rpl_stats.send_ackdaos, sizeof(uint16_t));
  memcpy(&msg->rpl_stats[RCV_ACKDAO], &rpl_stats.received_ackdaos, sizeof(uint16_t));
#endif
  
#ifdef UIP_STATS
  memcpy(&msg->uip_stats[SEND_IP], &uip_stat.ip.sent, sizeof(uint16_t));
  memcpy(&msg->uip_stats[RCV_IP], &uip_stat.ip.recv, sizeof(uint16_t));
  memcpy(&msg->uip_stats[FWD_IP], &uip_stat.ip.forwarded, sizeof(uint16_t));
  memcpy(&msg->uip_stats[DROP_IP], &uip_stat.ip.drop, sizeof(uint16_t));
#endif
}
