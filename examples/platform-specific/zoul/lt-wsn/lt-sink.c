
#include "contiki.h"
#include "net/routing/routing.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include "net/mac/tsch/tsch.h"
#include "net/ipv6/multicast/uip-mcast6.h"
#include "net/ipv6/uip-udp-packet.h"
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
#include "lib/memb.h"


#include "lt-data.h"

#define LOG_MODULE "LT-SINK"
#define LOG_LEVEL LOG_LEVEL_INFO

#define UDP_CLIENT_PORT	8765
#define UDP_SERVER_PORT	5678

#define SEND_INTERVAL		  (5 * CLOCK_SECOND)

#if !NETSTACK_CONF_WITH_IPV6 || !UIP_CONF_ROUTER || !UIP_IPV6_MULTICAST || !UIP_CONF_IPV6_RPL
#error "This example can not work with the current contiki configuration"
#error "Check the values of: NETSTACK_CONF_WITH_IPV6, UIP_CONF_ROUTER, UIP_CONF_IPV6_RPL"
#endif

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
static struct simple_udp_connection server_conn;

PROCESS(node_process, "RPL Node");
AUTOSTART_PROCESSES(&node_process);
/*---------------------------------------------------------------------------*/

#define MAX_PAYLOAD_LEN 12
#define MCAST_SINK_UDP_PORT 3001 /* Host byte order */
static struct uip_udp_conn * mcast_conn;
static char buf[MAX_PAYLOAD_LEN];

static uint64_t time_to_sleep;
static uint32_t sleep_frequency = SLEEP_FREQUENCY;

static void
multicast_send(void)
{
  memset(buf, 0, MAX_PAYLOAD_LEN);
  memcpy(buf, &time_to_sleep, sizeof(uint64_t));
  memcpy(&buf[sizeof(uint64_t)], &sleep_frequency, sizeof(uint32_t));
  uip_udp_packet_send(mcast_conn, buf, sizeof(uint64_t) + sizeof(uint32_t));
}
/*---------------------------------------------------------------------------*/
static void
prepare_mcast(void)
{
  uip_ipaddr_t ipaddr;

#if UIP_MCAST6_CONF_ENGINE == UIP_MCAST6_ENGINE_MPL
/*
 * MPL defines a well-known MPL domain, MPL_ALL_FORWARDERS, which
 *  MPL nodes are automatically members of. Send to that domain.
 */
  uip_ip6addr(&ipaddr, 0xFF03,0,0,0,0,0,0,0xFC);
#else
  /*
   * IPHC will use stateless multicast compression for this destination
   * (M=1, DAC=0), with 32 inline bits (1E 89 AB CD)
   */
  uip_ip6addr(&ipaddr, 0xFF1E,0,0,0,0,0,0x89,0xABCD);
#endif
  mcast_conn = udp_new(&ipaddr, UIP_HTONS(MCAST_SINK_UDP_PORT), NULL);
}

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
  //LOG_INFO("Received packet of %d bytes\n",datalen);
  LOG_INFO_6ADDR(sender_addr);
  for(int i = 0; i < datalen;i++)
    printf(" %d", data[i]);
  printf("\n");

}
/*---------------------------------------------------------------------------*/
static void
receive_data(){
  simple_udp_register(&server_conn, UDP_SERVER_PORT, NULL,
                      UDP_CLIENT_PORT, udp_rx_callback);
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(node_process, ev, data)
{
  static struct etimer send_periodic_timer;
  static struct etimer cycle_timer;
  static int boot = 1;
  

  PROCESS_BEGIN();
  
  
    NETSTACK_ROUTING.root_start();
    prepare_mcast();


  NETSTACK_MAC.on();
  
  etimer_set(&cycle_timer, CLOCK_SECOND);
  etimer_set(&send_periodic_timer, random_rand() % SEND_INTERVAL);

  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER);

    if (data == &cycle_timer){
      if (!boot)
	receive_data();
      else
	boot = 0;
      
      time_to_sleep = tsch_get_network_uptime_ticks() + (CYCLE_DURATION * CLOCK_SECOND);
      etimer_set(&cycle_timer, CYCLE_DURATION * CLOCK_SECOND);
      etimer_set(&send_periodic_timer, random_rand() % SEND_INTERVAL);
      
      
    }
    if (data == &send_periodic_timer){
	multicast_send();
	etimer_set(&send_periodic_timer, random_rand() % SEND_INTERVAL);
    }
  }


  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
