
#include "contiki.h"
#include "net/routing/routing.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include "net/mac/tsch/tsch.h"
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

#define SEND_INTERVAL		  2

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
static struct simple_udp_connection server_conn;//, rdv_conn;

PROCESS(node_process, "RPL Node");
AUTOSTART_PROCESSES(&node_process);
/*---------------------------------------------------------------------------*/


#define MAX_PAYLOAD_LEN 44
#define MCAST_SINK_UDP_PORT 3001 /* Host byte order */

static uint64_t _CYCLE_DURATION = CYCLE_DURATION;
static uint32_t _SLEEP_FREQUENCY = SLEEP_FREQUENCY;
static uint64_t _RDV_TIME_SLOT = DEFAULT_RDV_TIME_SLOT;
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
rdv_update(void)
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
}
/*---------------------------------------------------------------------------*/
static void
udp_rx_callback(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen) {
  if (data[0] == 'R'){
    uint8_t rdv_data[MAX_PAYLOAD_LEN];
    
    memset(rdv_data, 0, MAX_PAYLOAD_LEN);
    memcpy(rdv_data, &current_date, sizeof(uint64_t));
    memcpy(&rdv_data[sizeof(uint64_t)], &time_to_send, sizeof(uint64_t));
    memcpy(&rdv_data[2 * sizeof(uint64_t)], &cycle_duration, sizeof(uint64_t));
    memcpy(&rdv_data[3 * sizeof(uint64_t)], &sleep_frequency, sizeof(uint32_t));
    memcpy(&rdv_data[3 * sizeof(uint64_t) + sizeof(uint32_t)], &_RDV_TIME_SLOT, sizeof(uint64_t));
    memcpy(&rdv_data[4 * sizeof(uint64_t) + sizeof(uint32_t)], &_NETWORKING_TIME_SLOT, sizeof(uint64_t));
    memcpy(&rdv_data[5 * sizeof(uint64_t) + sizeof(uint32_t)], &data[1], sizeof(uint64_t));
    
    printf("%llu : %llu  %llu %llu %lu %llu %llu\n",
	   tsch_get_network_uptime_ticks() / CLOCK_SECOND,
	   current_date,
	   cycle_duration,
	   time_to_send,
	   sleep_frequency,
	   _RDV_TIME_SLOT,
	   _NETWORKING_TIME_SLOT);
    simple_udp_sendto(&server_conn, rdv_data, (6 * sizeof(uint64_t)) + sizeof(uint32_t), sender_addr);
  }
  else{
    printf("%llu : ", tsch_get_network_uptime_ticks() / CLOCK_SECOND);
    LOG_INFO_6ADDR(sender_addr);
    for(int i = 0; i < datalen;i++)
      printf(" %d", data[i]);
    printf("\n");
    char ack = 'K';
    simple_udp_sendto(&server_conn, &ack,1, sender_addr);
  }
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
}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(cmd_networking(struct pt *pt, shell_output_func output, char *args))
{ 
  char *next_args;
  uint64_t rdv;
  uint64_t net;
  
  PT_BEGIN(pt);

  SHELL_ARGS_INIT(args, next_args);

  /* Get argument (remote IPv6) */
  SHELL_ARGS_NEXT(args, next_args);
  if(args == NULL) {
    SHELL_OUTPUT(output, "Rdv time slot  ?\n");
    PT_EXIT(pt);
  }
  rdv = atoll(args);
  SHELL_ARGS_NEXT(args, next_args);
  if(args == NULL) {
    SHELL_OUTPUT(output, "Networking time slot ?\n");
    PT_EXIT(pt);
  }
  net = atoll(args);
    
  if(rdv < 10) {
    SHELL_OUTPUT(output, "Multicast time slot too low (min 10) %s\n", args);
    PT_EXIT(pt);
  }
  if(net < 10) {
    SHELL_OUTPUT(output, "Networking time slot too low (min 10) %s\n", args);
    PT_EXIT(pt);
  }

  _RDV_TIME_SLOT = rdv;
  _NETWORKING_TIME_SLOT = net;
  SHELL_OUTPUT(output,"OK\n");

  PT_END(pt);
}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(cmd_date(struct pt *pt, shell_output_func output, char *args))
{
  char *next_args;
  uint64_t date;
  
  PT_BEGIN(pt);

  SHELL_ARGS_INIT(args, next_args);

  /* Get argument (date in sec) */
  SHELL_ARGS_NEXT(args, next_args);
  if(args == NULL) {
    SHELL_OUTPUT(output, "date in seconds  ?\n");
    PT_EXIT(pt);
  }
  date = atoll(args);
  current_date = date;
  SHELL_OUTPUT(output,"OK\n");

  PT_END(pt);
}
/*---------------------------------------------------------------------------*/
  
const struct shell_command_t sensing_shell_commands[] =
  {{"sensing", cmd_sensing, "'> sensing ' : modify cycle and sensing frequency"},
   {"networking", cmd_networking, "'> networking ' : modify multicast and transmitting times slots"},
   {"date", cmd_date, "'> date ' : modify current date"}};
static struct shell_command_set_t sensing_shell_command_set = {
  .next = NULL,
  .commands = sensing_shell_commands,
};

static uint8_t tsch_cnx = 0;
static struct etimer rdv_timer,join_tsch_timer;
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(node_process, ev, data)
{

  PROCESS_BEGIN();
  
    shell_command_set_register(&sensing_shell_command_set);
    
  
    NETSTACK_MAC.on();
    NETSTACK_ROUTING.root_start();
    
    etimer_set(&join_tsch_timer, CLOCK_SECOND);
    while(!tsch_cnx){
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&join_tsch_timer));
      if (data == &join_tsch_timer){      
	tsch_time = tsch_get_network_uptime_ticks();
	LOG_INFO("tsch time %llu\n", tsch_time);
	if (tsch_time != -1)
	  tsch_cnx = 1;
	else
	  etimer_set(&join_tsch_timer, CLOCK_SECOND);
      }
    }

    simple_udp_register(&server_conn, UDP_SERVER_PORT, NULL,
                      UDP_CLIENT_PORT, udp_rx_callback);
    
    etimer_set(&rdv_timer, SEND_INTERVAL * CLOCK_SECOND);
    while(1) {
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&rdv_timer));
      rdv_update();
      etimer_restart(&rdv_timer);
    }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
