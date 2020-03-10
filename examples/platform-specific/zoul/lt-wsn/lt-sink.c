
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

#include <lt-data.h>

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
  // uint64_t local_time_clock_ticks = tsch_get_network_uptime_ticks();
  ltdata_t ltdata;

  if(datalen >= sizeof(ltdata)) {
    memcpy(&ltdata, data, sizeof(ltdata));

    LOG_INFO("Received from ");
    LOG_INFO_6ADDR(sender_addr);
    LOG_INFO_("created at %lu : %u, + %d : %u, %u, %u V\n",
              (unsigned long)ltdata.time,ltdata.d1,
	      ltdata.offset,
              ltdata.d2, ltdata.d3,ltdata.d4);
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
