
#include "contiki.h"
#include "net/routing/routing.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include "net/mac/tsch/tsch.h"
#include "lib/random.h"
#include "sys/node-id.h"

#include "sys/log.h"


#include "cpu.h"
#include "sys/process.h"
#include "dev/leds.h"
#include "dev/sys-ctrl.h"
#include "lib/list.h"
//#include "power-mgmt.h"
#include "rtcc.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "lt-data.h"

#define LOG_MODULE "LT-SINK"
#define LOG_LEVEL LOG_LEVEL_INFO

#define UDP_CLIENT_PORT	8765
#define UDP_SERVER_PORT	5678

#define SEND_INTERVAL		  (5 * CLOCK_SECOND)


/*---------------------------------------------------------------------------*/
//static struct etimer et;
/*---------------------------------------------------------------------------*/
/* RE-Mote revision B, low-power PIC version */
#define PM_EXPECTED_VERSION               0x20
/*---------------------------------------------------------------------------*/
#define TEST_LEDS_FAIL                    leds_off(LEDS_ALL); \
                                          leds_on(LEDS_RED);  \
                                          PROCESS_EXIT();
/*---------------------------------------------------------------------------*/
#define TEST_ALARM_SECOND                 15
/*---------------------------------------------------------------------------*/
//static uint8_t rtc_buffer[sizeof(simple_td_map)];
//static simple_td_map *simple_td = (simple_td_map *)rtc_buffer;
//static uint64_t date_seconds = DATE_SECONDS;
/*---------------------------------------------------------------------------*/
static struct simple_udp_connection client_conn, server_conn;

PROCESS(node_process, "RPL Node");
AUTOSTART_PROCESSES(&node_process);
/*---------------------------------------------------------------------------*/
static void
udp_rx_callback(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{
  
  LOG_INFO("Received packet of %d bytes\n",datalen);
  int index = 0;
  for(int i = 0; i < datalen;i++)
    printf("%d ", data[i]);
  printf("\n");

  if(datalen >= sizeof(ltdata_t)) {
    char d_type;
    uint64_t time = 0;
    d_type = data[0];
    data++;
    index++;
    memcpy(&time, data, sizeof(uint64_t));
    data += sizeof(uint64_t);
    index +=  sizeof(uint64_t);
    uint32_t offset = 0;
    memcpy(&offset, data, sizeof(uint32_t));
    data +=  sizeof(uint32_t);
    index +=  sizeof(uint32_t);
    LOG_INFO("Received from ");
    LOG_INFO_6ADDR(sender_addr);
    LOG_INFO("\n");
    LOG_INFO("Packet type: %c , time: %llu, offset: %lu\n", d_type, time, offset);

#if defined(UIP_STATS) || defined(RPL_STATS)
      ltstatsdata_t ltstatsdata;
#endif

    switch(d_type){
    case 'S':
#if defined(UIP_STATS) || defined(RPL_STATS)
      //ltstatsdata_t ltstatsdata;
      memcpy(&ltstatsdata, data, sizeof(ltstatsdata_t));
#ifdef UIP_STATS
      LOG_INFO("IP out: %d, IP in: %d, IP fwd: %d, IP drop: %d\n",
	       ltstatsdata.uip_stats[SEND_IP],ltstatsdata.uip_stats[RCV_IP],
	       ltstatsdata.uip_stats[FWD_IP],ltstatsdata.uip_stats[DROP_IP]);
#endif
#ifdef RPL_STATS
      LOG_INFO("DIO out: %d, DIO in: %d, DAO out: %d, DAO in: %d, DIS out: %d, \
DIS in: %d, ACKDAO out: %d, ACKDAO in: %d\n",
	       ltstatsdata.rpl_stats[SEND_DIO],  ltstatsdata.rpl_stats[RCV_DIO], 
	       ltstatsdata.rpl_stats[SEND_DAO],  ltstatsdata.rpl_stats[RCV_DAO],
	       ltstatsdata.rpl_stats[SEND_DIS],  ltstatsdata.rpl_stats[RCV_DIS], 
	       ltstatsdata.rpl_stats[SEND_ACKDAO],  ltstatsdata.rpl_stats[RCV_ACKDAO]);
#endif
      data += sizeof(ltstatsdata_t);
      index += sizeof(ltstatsdata_t);
#endif
      break;
    case 'D':
      while(index < datalen){
	uint16_t num_rec = 0;
	memcpy(&num_rec, data, sizeof(uint16_t));
	data += sizeof(uint16_t);
	index += sizeof(uint16_t);
	ltdatarcv_t ltdata;
	memcpy(&ltdata, data, sizeof(ltdatarcv_t));
	data += sizeof(ltdatarcv_t);
	index += sizeof(ltdatarcv_t);
	
	LOG_INFO("%lu.%lu V\n", ltdata.voltage / 100, ltdata.voltage % 100);
	if(ltdata.sensors[SENSOR_TYPE] == BMP180)
	  LOG_INFO("Tmp: %d, P: %d\n",
		   ltdata.sensors[DATA_1],
		   ltdata.sensors[DATA_2]);
	if(ltdata.sensors[SENSOR_TYPE] == SHT25)
	    LOG_INFO("Tmp: %d.%d, H: %d.%d\n",
		     ltdata.sensors[DATA_1] / 100, ltdata.sensors[DATA_1] % 100,
		     ltdata.sensors[DATA_2] / 100, ltdata.sensors[DATA_2] % 100);
	if(ltdata.sensors[SENSOR_TYPE] == DHT22)
	  LOG_INFO("Tmp: %d.%d, H: %d.%d\n",
		   ltdata.sensors[DATA_1] / 10, ltdata.sensors[DATA_1] % 10, 
		   ltdata.sensors[DATA_2] / 10, ltdata.sensors[DATA_2] % 10);
      }
      break;
    }
  }
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(node_process, ev, data)
{
  static struct etimer periodic_timer;
  

  PROCESS_BEGIN();
  
  
    NETSTACK_ROUTING.root_start();

  /* Initialize UDP connections */
  simple_udp_register(&server_conn, UDP_SERVER_PORT, NULL,
                      UDP_CLIENT_PORT, udp_rx_callback);
  simple_udp_register(&client_conn, UDP_CLIENT_PORT, NULL,
                      UDP_SERVER_PORT, NULL);


  
  NETSTACK_MAC.on();

  etimer_set(&periodic_timer, random_rand() % SEND_INTERVAL);

  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
