
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
#include "shell.h"
#include "shell-commands.h"
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

#define SEND_INTERVAL		  10

#if !NETSTACK_CONF_WITH_IPV6 || !UIP_CONF_ROUTER || !UIP_IPV6_MULTICAST || !UIP_CONF_IPV6_RPL
#error "This example can not work with the current contiki configuration"
#error "Check the values of: NETSTACK_CONF_WITH_IPV6, UIP_CONF_ROUTER, UIP_CONF_IPV6_RPL"
#endif

#if CYCLE_DURATION < SLEEP_FREQUENCY
#error "CYCLE_DURATION lower than SLEEP_FREQUENCY"
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


#define MAX_PAYLOAD_LEN 44
#define MCAST_SINK_UDP_PORT 3001 /* Host byte order */
static struct uip_udp_conn * mcast_conn;
static char buf[MAX_PAYLOAD_LEN];


static uint64_t _CYCLE_DURATION = CYCLE_DURATION;
static uint32_t _SLEEP_FREQUENCY = SLEEP_FREQUENCY;
static uint64_t _MCST_TIME_SLOT = MCST_TIME_SLOT;
static uint64_t _NETWORKING_TIME_SLOT = NETWORKING_TIME_SLOT;

static uint64_t cycle_duration = CYCLE_DURATION;
static uint64_t time_to_send = CYCLE_DURATION;
static uint32_t sleep_frequency = SLEEP_FREQUENCY;
static uint64_t starting_cycle_date = CURRENT_DATE;
static uint8_t newcycle = 0;
static uint64_t current_date = CURRENT_DATE;

static uint64_t tsch_time = 0;
static uint64_t deltaT = 0;
static uint64_t elapsed_time = 0;

static uint64_t old_tsch_time = 0;

/*---------------------------------------------------------------------------*/
static void
multicast_send(void)
{
  tsch_time = tsch_get_network_uptime_ticks();
  deltaT = tsch_time - old_tsch_time ;
  old_tsch_time = tsch_time;

  cycle_duration = _CYCLE_DURATION;
  
  current_date += (deltaT/CLOCK_SECOND);

  if (newcycle){
    starting_cycle_date = current_date;
    newcycle = 0;
  }
  
  elapsed_time = current_date - starting_cycle_date;
  time_to_send = _CYCLE_DURATION - (elapsed_time % _CYCLE_DURATION);
  sleep_frequency = (time_to_send * _SLEEP_FREQUENCY) / _CYCLE_DURATION;


  //if(sleep_frequency > 0) {

  printf("Send at %llu, CD %llu, ttsend : %llu sleepF :%lu, mcst : %llu, net %llu\n",
	 current_date,
	 cycle_duration,
	 time_to_send,
	 sleep_frequency,
	 _MCST_TIME_SLOT,
	 _NETWORKING_TIME_SLOT);

    memset(buf, 0, MAX_PAYLOAD_LEN);
    memcpy(buf, &current_date, sizeof(uint64_t));
    memcpy(&buf[sizeof(uint64_t)], &time_to_send, sizeof(uint64_t));
    memcpy(&buf[2 * sizeof(uint64_t)], &cycle_duration, sizeof(uint64_t));
    memcpy(&buf[3 * sizeof(uint64_t)], &sleep_frequency, sizeof(uint32_t));
    memcpy(&buf[3 * sizeof(uint64_t) + sizeof(uint32_t)], &_MCST_TIME_SLOT, sizeof(uint64_t));
    memcpy(&buf[4 * sizeof(uint64_t) + sizeof(uint32_t)], &_NETWORKING_TIME_SLOT, sizeof(uint64_t));
    uip_udp_packet_send(mcast_conn, buf, (5 * sizeof(uint64_t)) + sizeof(uint32_t));
    //}
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
  printf("%llu ", tsch_get_network_uptime_ticks() / CLOCK_SECOND);
  LOG_INFO_6ADDR(sender_addr);
  for(int i = 0; i < datalen;i++)
    printf(" %d", data[i]);
  printf("\n");

  // when receiving, one can low the multicast sending frequency
  
  char ack = 'K';
  simple_udp_sendto(&server_conn, &ack,1, sender_addr);
}
/*---------------------------------------------------------------------------*/
static void
receive_data(){
  simple_udp_register(&server_conn, UDP_SERVER_PORT, NULL,
                      UDP_CLIENT_PORT, udp_rx_callback);
}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(cmd_sensing(struct pt *pt, shell_output_func output, char *args))
{
  
  char *next_args;
  uint64_t cydu;
  uint32_t sfq;
  
  PT_BEGIN(pt);

  SHELL_ARGS_INIT(args, next_args);

  /* Get argument (remote IPv6) */
  SHELL_ARGS_NEXT(args, next_args);
  if(args == NULL) {
    SHELL_OUTPUT(output, "cycle  ?\n");
    PT_EXIT(pt);
  }
  cydu = atoll(args);
  SHELL_ARGS_NEXT(args, next_args);
  if(args == NULL) {
    SHELL_OUTPUT(output, "cycle ok but sleep  ?\n");
    PT_EXIT(pt);
  }
  sfq = atol(args);
    
  if(sfq > cydu) {
    SHELL_OUTPUT(output, "cycle lower than sleep %s\n", args);
    PT_EXIT(pt);
  }

  _CYCLE_DURATION = cydu;
  _SLEEP_FREQUENCY = sfq;
  newcycle = 1;
  SHELL_OUTPUT(output,"OK\n");

  PT_END(pt);
}/*---------------------------------------------------------------------------*/
static
PT_THREAD(cmd_networking(struct pt *pt, shell_output_func output, char *args))
{
  
  char *next_args;
  uint64_t mcst;
  uint64_t net;
  
  PT_BEGIN(pt);

  SHELL_ARGS_INIT(args, next_args);

  /* Get argument (remote IPv6) */
  SHELL_ARGS_NEXT(args, next_args);
  if(args == NULL) {
    SHELL_OUTPUT(output, "Multicast time slot  ?\n");
    PT_EXIT(pt);
  }
  mcst = atoll(args);
  SHELL_ARGS_NEXT(args, next_args);
  if(args == NULL) {
    SHELL_OUTPUT(output, "Networking time slot ?\n");
    PT_EXIT(pt);
  }
  net = atol(args);
    
  if(mcst < 10) {
    SHELL_OUTPUT(output, "Multicast time slot too low (min 10) %s\n", args);
    PT_EXIT(pt);
  }
  if(net < 10) {
    SHELL_OUTPUT(output, "Networking time slot too low (min 10) %s\n", args);
    PT_EXIT(pt);
  }

  _MCST_TIME_SLOT = mcst;
  _NETWORKING_TIME_SLOT = net;
  SHELL_OUTPUT(output,"OK\n");

  PT_END(pt);
}
/*---------------------------------------------------------------------------*/
  
const struct shell_command_t sensing_shell_commands[] =
  {{"sensing", cmd_sensing, "'> sensing ' : modify cycle and sensing frequency"},
   {"networking", cmd_networking, "'> networking ' : modify multicast and transmitting times slots"}};
static struct shell_command_set_t sensing_shell_command_set = {
  .next = NULL,
  .commands = sensing_shell_commands,
};


static uint8_t tsch_cnx = 0;
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(node_process, ev, data)
{
  static struct etimer send_periodic_timer,join_tsch_timer, init_mcst;

  PROCESS_BEGIN();
  
    NETSTACK_MAC.on();
    NETSTACK_ROUTING.root_start();
    
    shell_command_set_register(&sensing_shell_command_set);
    
    prepare_mcast();
    receive_data();
  
    etimer_set(&join_tsch_timer, CLOCK_SECOND);
    
    while(!tsch_cnx){
    PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER);
      if (data == &join_tsch_timer){      
	tsch_time = tsch_get_network_uptime_ticks();
	LOG_INFO("tsch time %llu\n", tsch_time);
	if (tsch_time != -1)
	  tsch_cnx = 1;
	else
	  etimer_set(&join_tsch_timer, CLOCK_SECOND);
      }
    }
  
    etimer_set(&send_periodic_timer, random_rand() % (SEND_INTERVAL * CLOCK_SECOND));
    etimer_set(&init_mcst, CLOCK_SECOND * 30);

    while(1) {
      PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER);
      if (data == &send_periodic_timer){ 
	multicast_send();
	etimer_set(&send_periodic_timer, random_rand() % (SEND_INTERVAL * CLOCK_SECOND));
      }
      if (data == &init_mcst){
	//LOG_INFO("re init mcast\n");
	//UIP_MCAST6.init();
	etimer_set(&init_mcst, CLOCK_SECOND * 30);}
    }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
