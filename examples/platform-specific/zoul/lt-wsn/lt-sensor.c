
#include "contiki.h"
#include "dev/button-hal.h"
#include "net/routing/routing.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include "net/mac/tsch/tsch.h"
#include "net/ipv6/multicast/uip-mcast6.h"
#include "lib/random.h"
#include "storage/cfs/cfs.h"
#include "storage/cfs/cfs-coffee.h"
#include "sys/node-id.h"

#include "sys/log.h"


#include "cpu.h"
#include "sys/process.h"
#include "dev/leds.h"
#include "dev/sys-ctrl.h"
#include "lib/list.h"
#include "power-mgmt.h"
#include "rtcc.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <lt-data.h>
#include <rtcc-tools.h>

#define LOG_MODULE "LT-SENSOR"
#define LOG_LEVEL LOG_LEVEL_INFO

#define UDP_CLIENT_PORT	8765
#define UDP_SERVER_PORT	5678

#define SEND_INTERVAL		  (5 * CLOCK_SECOND)


#if !NETSTACK_CONF_WITH_IPV6 || !UIP_CONF_ROUTER || !UIP_IPV6_MULTICAST || !UIP_CONF_IPV6_RPL
#error "This example can not work with the current contiki configuration"
#error "Check the values of: NETSTACK_CONF_WITH_IPV6, UIP_CONF_ROUTER, UIP_CONF_IPV6_RPL"
#endif

/*---------------------------------------------------------------------------*/
static struct etimer shutdown_timer,
  init_led_timer, sending_led_timer;
/*---------------------------------------------------------------------------*/
/* RE-Mote revision B, low-power PIC version */
#define PM_EXPECTED_VERSION               0x20
/*---------------------------------------------------------------------------*/
#define TEST_LEDS_FAIL                    leds_off(LEDS_ALL); \
                                          leds_on(LEDS_RED);  \
                                          PROCESS_EXIT();
/*---------------------------------------------------------------------------*/
#define TEST_ALARM_SECOND                 3600

#define NETWORKING_DURATION (DEEP_MAX * JOIN_DURATION * CLOCK_SECOND)

#define SLEEP_DURATION (CYCLE_DURATION / SLEEP_FREQUENCY)

/*---------------------------------------------------------------------------*/
static uint8_t rtc_buffer[sizeof(simple_td_map)];
static simple_td_map *simple_td = (simple_td_map *)rtc_buffer;

static uint64_t network_uptime;
static uint32_t offset;

static uint8_t rendezvous;

static int tot_steps, cur_step, clear_to_send, on_network, resetted;

static char CFS_EOF = (char) -1;
/*---------------------------------------------------------------------------*/
static struct simple_udp_connection client_conn;

#define MCAST_SINK_UDP_PORT 3001 /* Host byte order */

static struct uip_udp_conn *sink_conn;

PROCESS(node_process, "RPL Node");
AUTOSTART_PROCESSES(&node_process);
/*---------------------------------------------------------------------------*/

static void
tcpip_handler(void)
{
  if(uip_newdata()) {
    uint64_t time_to_sleep;
    uint32_t frequency;

    memcpy(&time_to_sleep, (uint8_t*)uip_appdata, sizeof(uint64_t));
    memcpy(&frequency,  &((uint8_t*)uip_appdata)[sizeof(uint64_t)], sizeof(uint32_t));
    LOG_INFO("rdv\n");
    // TODO paramétrer l'heure de réveil
    rendezvous = 1;
  }
  return;
}

static void
time_param_cb(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{}

int first_step(){
  return cur_step == 1;
}
int last_step(){
  return cur_step == tot_steps;
}
void inc_step(){
  char conf[4];
  memset(conf,0,4);
  cur_step++;
  int fconf = cfs_open("root/run", CFS_WRITE);   
  cfs_seek(fconf, 4, CFS_SEEK_SET);
  memcpy(conf, &cur_step,4);
  cfs_write(fconf, conf, 4);
  cfs_write(fconf, &CFS_EOF, 1);
  cfs_close(fconf);
  LOG_INFO("inc step last\n");
}

void raz_step(){
  char conf[4];
  memset(conf,0,4);
  cur_step = 1;
  int fconf = cfs_open("root/run", CFS_WRITE);   
  cfs_seek(fconf, 4, CFS_SEEK_SET);
  memset(conf,0,4);
  memcpy(conf, &cur_step,4);
  cfs_write(fconf, conf, 4);
  cfs_write(fconf, &CFS_EOF, 1);
  cfs_close(fconf);
}
void init_data(uint64_t net_time, uint32_t ofst){
  int fdata = cfs_open("root/data", CFS_WRITE|CFS_APPEND);
  char char_time[8];
  char char_offset[4];
  memset(char_time, 0, 8);
  memset(char_offset, 0, 4);
  memcpy(char_time, &net_time,8);
  memcpy(char_offset, &ofst,4);
  cfs_write(fdata, &char_time, 8);
  cfs_write(fdata, &char_offset, 4);
  cfs_write(fdata, &CFS_EOF, 1);
  cfs_close(fdata);
}
void record_sensor_data(){  
  int fdata = cfs_open("root/data", CFS_WRITE |  CFS_APPEND);
  cfs_seek(fdata, -1, CFS_SEEK_END);
  ltdata_t ltdata;
  memset(&ltdata, 0, sizeof(ltdata_t));
  ltdata_read_sensor_data(&ltdata);
  cfs_write(fdata, &ltdata,sizeof(ltdata_t));
  cfs_write(fdata, &CFS_EOF, 1);
  cfs_close(fdata);
}
void get_stats_data(ltstatsdata_t * ltsd){
  int fstats = cfs_open("root/stats", CFS_READ);
  cfs_read(fstats, ltsd, sizeof(ltstatsdata_t));
  cfs_close(fstats);
}
void update_stats_data(){
  ltstatsdata_t ltsd, oldltsd;
  ltdata_read_stats_data(&ltsd);
  get_stats_data(&oldltsd);
  ltdata_update_stats_data(&ltsd,&oldltsd);
  cfs_remove("root/stats");
  int fstats = cfs_open("root/stats", CFS_WRITE |  CFS_APPEND);
  cfs_write(fstats, &ltsd, sizeof(ltstatsdata_t));
  cfs_write(fstats, &CFS_EOF, 1);
  cfs_close(fstats);
}
void record_stats_data(){  
  int fstats = cfs_open("root/stats", CFS_WRITE |  CFS_APPEND);
  cfs_seek(fstats, -1, CFS_SEEK_END);
  ltstatsdata_t ltstatsdata;
  memset(&ltstatsdata, 0, sizeof(ltstatsdata_t));
  ltdata_read_stats_data(&ltstatsdata);
  cfs_write(fstats, &ltstatsdata,sizeof(ltstatsdata_t));
  cfs_write(fstats, &CFS_EOF, 1);
  cfs_close(fstats);
} 
void shutdown_to_next_step(){
  uint64_t some_time = SLEEP_DURATION;
      
  rtcc_sec_to_date(simple_td, 0);
  rtcc_set_time_date(simple_td);
  
  rtcc_date_increment_seconds(simple_td, some_time);
  pm_set_timeout(0x00);
  rtcc_set_alarm_time_date(simple_td, RTCC_ALARM_ON, RTCC_REPEAT_DAY,
			   RTCC_TRIGGER_INT2);
  leds_off(LEDS_ALL);
  if(pm_shutdown_now(PM_HARD_SLEEP_CONFIG) == PM_SUCCESS) {
    printf("PM: good night for %llu\n",some_time );
  } else {
    printf("PM: error shutting down the system!\n");
  }
}
void shutdown_to_first_step(uint64_t nt){
  
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(node_process, ev, data)
{
  button_hal_button_t *btn;
  static uint8_t initialized, blue,red;
  static struct etimer periodic_timer;
  static struct etimer waiting_mcst;
  uip_ipaddr_t dest_ipaddr;
  static int fconf;
  //static struct cfs_dir dirp;
  //static struct cfs_dirent dirent;
  static int sleep_frequency = SLEEP_FREQUENCY;
  
  PROCESS_BEGIN();
  LOG_INFO("START\n");
  pm_enable();
  
  fconf = cfs_open("conf", CFS_READ);
  
  /** first boot after flashing or too much reboot **/
  
  if (fconf < 0) { 

    LOG_INFO("First boot\n");
    btn = button_hal_get_by_index(0);
    initialized = 0;
    blue = 0;
    while(!initialized) {
      PROCESS_YIELD();
      if(ev == button_hal_periodic_event) {
	btn = (button_hal_button_t *)data;
	if(btn->press_duration_seconds > 2) {
	  blue=1;
	  initialized = 1;
	  etimer_set(&init_led_timer, CLOCK_SECOND);
	  leds_on(LEDS_BLUE);
	  
	  fconf = cfs_open("conf", CFS_WRITE);
	  uint8_t buf[2];
	  buf[0] = 0;
	  buf[1] = 0;
	  memset(buf, 0, 2 * sizeof(uint8_t));
	  cfs_write(fconf, buf, 2 * sizeof(uint8_t));
	  cfs_write(fconf, &CFS_EOF, 1);
	  cfs_close(fconf);
	}
      }
    }
  }

  // conf file exist, test if it is
  // a reboot for waiting mcst
  
  fconf = cfs_open("conf", CFS_READ);
  uint8_t buf[2];
  cfs_read(fconf, buf, 2 * sizeof(uint8_t));
  cfs_close(fconf);
  if (buf[0] == 0){  // Always waiting for mcst
    if (buf[1] < MAX_ATTEMPT_MCST){  // try again
      buf[1]++;                      // increments tentative
      fconf = cfs_open("conf", CFS_WRITE);
      cfs_write(fconf, buf, 2 * sizeof(uint8_t));
      cfs_write(fconf, &CFS_EOF, 1);
      cfs_close(fconf);
    }
    else { // too much tries, full RAZ and shutdwown
      cfs_remove("conf");
      if(pm_shutdown_now(PM_HARD_SLEEP_CONFIG) == PM_SUCCESS) {
	printf("PM: good night for %d\n",RETRY_MCST_TIME );
      } else{
	TEST_LEDS_FAIL;}
    }
  }
   
  
 
    etimer_set(&waiting_mcst, WAIT_MCST_TIME * CLOCK_SECOND);
    
    sink_conn = udp_new(NULL, UIP_HTONS(0), NULL);
    udp_bind(sink_conn, UIP_HTONS(MCAST_SINK_UDP_PORT));
    
    NETSTACK_MAC.on();

    rendezvous = 0;
    LOG_INFO("Waiting multicast\n");
    while(!rendezvous) {
      PROCESS_WAIT_EVENT_UNTIL(ev == tcpip_event || ev == PROCESS_EVENT_TIMER);
      if (ev == tcpip_event)
	tcpip_handler();
      if (ev == PROCESS_EVENT_TIMER){
	if (data == &init_led_timer){
	  if (blue){
	    leds_off(LEDS_ALL);
	    blue=0;
	  }
	  else {
	    leds_on(LEDS_BLUE);
	    blue=1;
	  }
	  etimer_reset(&init_led_timer);
	}
	if (data == &waiting_mcst){
	  uint64_t retry = RETRY_MCST_TIME;
	  LOG_INFO("retry multicast later\n");

	  
	  rtcc_sec_to_date(simple_td, 0);
	  rtcc_set_time_date(simple_td);
	  rtcc_date_increment_seconds(simple_td, retry);
	  pm_set_timeout(0x00);
	  rtcc_set_alarm_time_date(simple_td, RTCC_ALARM_ON, RTCC_REPEAT_DAY,
				   RTCC_TRIGGER_INT2);
	  leds_off(LEDS_ALL);
	  if(pm_shutdown_now(PM_HARD_SLEEP_CONFIG) == PM_SUCCESS) {
	    printf("PM: good night for %d\n",RETRY_MCST_TIME );
	  } else{
	    LOG_INFO("mert..%d\n", pm_shutdown_now(PM_HARD_SLEEP_CONFIG));
	  TEST_LEDS_FAIL;}
	    
	}
      }
	  
    }

    /* enregistrement fichier de conf */
  
    
      /* Initialize UDP connections */
      /*
      simple_udp_register(&client_conn, UDP_CLIENT_PORT, NULL,
			  UDP_SERVER_PORT, NULL);
      */
  
    // save parameter in /root/run
    
    fconf = cfs_open("root/run", CFS_WRITE|CFS_APPEND);
    cfs_coffee_set_io_semantics(fconf, CFS_COFFEE_IO_ENSURE_READ_LENGTH);
    char sfrq[4];  
    memset(sfrq, 0, 4);
    memcpy(sfrq,&sleep_frequency,4);  // total step
    cfs_write(fconf, sfrq,4);
    memset(sfrq, 0, 4);
    int first = 1;
    memcpy(sfrq,&first,4);
    cfs_write(fconf, sfrq,4);
    cfs_write(fconf, &CFS_EOF, 1);
    cfs_close(fconf);

  
  /** Read the current and total steps **/
  
  char conf[4];
  memset(conf,0,4);
  
  fconf = cfs_open("root/run", CFS_READ);
  cfs_read(fconf, &conf, 4);
  tot_steps = 0;
  memcpy(&tot_steps, conf, 4);
  memset(conf,0,4);
  cfs_read(fconf,&conf, 4);
  cur_step = 0;
  memcpy(&cur_step, conf, 4);
  cfs_close(fconf);
  
  clear_to_send = 0;

  LOG_INFO("run conf : %d %d\n", tot_steps, cur_step);
  
  if (!first_step()){  // First step needs network access
    if(!last_step()) { // sensing but no transmit : sense and sleep
      record_sensor_data(); // just record sensor data
      inc_step();      // prepare next boot or step
      leds_on(LEDS_BLUE);
      struct etimer et;
      etimer_set(&et, CLOCK_SECOND);
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
      shutdown_to_next_step();
    }
    else {  // sensing and transmit
      clear_to_send = 1;
    }
  }
  etimer_set(&init_led_timer, CLOCK_SECOND);
  
  LOG_INFO("First or last step\n");
  
  
  /* Initialize UDP connections */
  simple_udp_register(&client_conn, UDP_CLIENT_PORT, NULL,
                      UDP_SERVER_PORT, &time_param_cb);
  NETSTACK_MAC.on();
  on_network = 0;
  resetted = 0;
  
  etimer_set(&periodic_timer, random_rand() % SEND_INTERVAL);
      
  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(PROCESS_EVENT_TIMER);
    
    if (data == &init_led_timer){
      if (blue){
	leds_off(LEDS_ALL);
	blue=0;
      }
      else {
	leds_on(LEDS_BLUE);
	blue=1;
      }
      etimer_reset(&init_led_timer);
    }
    if(data == &shutdown_timer){
      if(red)
	leds_off(LEDS_ALL);
      etimer_stop(&sending_led_timer);
      etimer_stop(&periodic_timer);
      etimer_stop(&shutdown_timer);
      if (resetted)
	cfs_remove("root/data");
      if(clear_to_send){
	rtcc_sec_to_date(simple_td, 0);
	rtcc_set_time_date(simple_td);
	
	rtcc_date_increment_seconds(simple_td,2);
	pm_set_timeout(0x00);
	rtcc_set_alarm_time_date(simple_td, RTCC_ALARM_ON, RTCC_REPEAT_DAY,
				 RTCC_TRIGGER_INT2);
      } else {
	if(!resetted){
	  rtcc_set_time_date(simple_td);
	  rtcc_date_increment_seconds(simple_td, offset);
	  pm_set_timeout(0x00);
	  rtcc_set_alarm_time_date(simple_td, RTCC_ALARM_ON, RTCC_REPEAT_DAY,
				   RTCC_TRIGGER_INT2);	  
	}
      }
      if(pm_shutdown_now(PM_HARD_SLEEP_CONFIG) == PM_SUCCESS) {
	printf("PM: good night for shutdown %lu\n", offset);
      } else {
	printf("PM: error shutting down the system!\n");
	TEST_LEDS_FAIL
      }
      etimer_set(&periodic_timer, CLOCK_SECOND * 2);
    }
    if(data == &sending_led_timer){
      if(red){
	leds_off(LEDS_ALL);
	red=0;
      }
      else {
	leds_on(LEDS_RED);
	red=1;
      }
      etimer_reset(&sending_led_timer);
    }
    if(data == &periodic_timer){
      if(NETSTACK_ROUTING.node_is_reachable() &&
	 NETSTACK_ROUTING.get_root_ipaddr(&dest_ipaddr)) {
	if(!on_network){
	  
	  /** Synchronization : all nodes boot at any time **/
	  /** save to the universal network time from TSCH  **/
	  /** All nodes will wake up at same absolute time **/
	  
	  network_uptime = tsch_get_network_uptime_ticks();
	  offset = 0;
	  rtcc_sec_to_date(simple_td, network_uptime/CLOCK_SECOND);
	  
	  // example with the next round five minutes
	  // since the start of the network
	  // i.e. one of :
	  // 00, 05, 10, 15, ... 55
	  // At least after the time for network coverage
	  // i.e. : DEEP_MAX * JOIN_DURATION

	  int minutes_nc = (DEEP_MAX * JOIN_DURATION) / 60;

	  if(minutes_nc < 2)
	    minutes_nc = 2;
	  else if (minutes_nc < 5)
	    minutes_nc = 5;
	  else if (minutes_nc < 10)
	    minutes_nc = 10;
	  else if (minutes_nc < 15)
	    minutes_nc = 15;
	  else
	    minutes_nc = 20;
	  
	  uint8_t minutes_to_wait = simple_td->minutes % minutes_nc;
	  uint8_t seconds_to_wait = simple_td->seconds % 60;
	  
	  offset = ((minutes_nc - 1 - minutes_to_wait) * 60) + (60 - seconds_to_wait);
	  
	  etimer_set(&shutdown_timer, NETWORKING_DURATION);
	  LOG_INFO("Networking during %d sec\n",NETWORKING_DURATION/CLOCK_SECOND);
	  etimer_set(&sending_led_timer, CLOCK_SECOND);
	  leds_off(LEDS_ALL);
	  etimer_stop(&init_led_timer);
	  
	  if(first_step()){
	    init_data(network_uptime, offset);
	    record_stats_data();
	    inc_step();
	  }
	  
	  record_sensor_data();
	  
	  on_network = 1;
	}	

	update_stats_data(); // update network stats
	
	if (clear_to_send) {
	  if (!resetted){
	    raz_step();    // by the way, we are at the first step
	    resetted = 1;  // but after the next boot
	  }
	  
	  
	  /** Build the data packets from sensor data file  **/
	  /** each packet has one sensor data **/
	  /** with time related header and sequence number **/
	  
	  
	  char data[100];
	  uint16_t sent_pkts = 0;
	  uint64_t n_time = 0;
	  uint32_t n_ofst = 0;
	  int fdata = cfs_open("root/data", CFS_READ);	  
	  cfs_read(fdata, &n_time, sizeof(uint64_t));
	  cfs_read(fdata, &n_ofst, sizeof(uint32_t));
	  while(sent_pkts < tot_steps){
	    memset(&data, 0, 100);
	    data[0] = 'D';
	    memcpy(&data[1], &n_time, sizeof(uint64_t));
	    memcpy(&data[1 + sizeof(uint64_t)], &n_ofst, sizeof(uint32_t));
	    memcpy(&data[1 + sizeof(uint64_t) + sizeof(uint32_t)], &sent_pkts, sizeof(uint16_t));
	    cfs_read(fdata,
		     &data[1 + sizeof(uint64_t) +
			   sizeof(uint32_t) + sizeof(uint16_t)],
		     sizeof(ltdata_t));
	    
	  
	    for(int i = 0; i < sizeof(uint64_t) + sizeof(uint32_t) + sizeof(uint16_t) + sizeof(ltdata_t);i++)
	      printf("%d ", data[i]);
	    printf("\n");
	  
	    /** Send one data packet **/
	  
	    simple_udp_sendto(&client_conn,
			      &data,
			      1 + sizeof(uint64_t) + sizeof(uint32_t) + sizeof(uint16_t) + sizeof(ltdata_t) ,
			      &dest_ipaddr);
	    LOG_INFO("Sent packet %u timestamp %llu offset %lu to : ", sent_pkts, n_time, n_ofst);
	    LOG_INFO_6ADDR(&dest_ipaddr);
	    LOG_INFO_("\n");
	    
	    sent_pkts++;
	  }
	  cfs_close(fdata);
	  
#if defined(UIP_STATS) || defined(RPL_STATS)
	  int fstats = cfs_open("root/stats", CFS_READ);
	  char stats[100];
	  memset(&stats, 0, 100);
	  stats[0] = 'S';
	  memcpy(&stats[1], &n_time, sizeof(uint64_t));
	  memcpy(&stats[1 + sizeof(uint64_t)], &n_ofst, sizeof(uint32_t));
	  cfs_read(fstats, &stats[1 + sizeof(uint64_t) + sizeof(uint32_t)],sizeof(ltstatsdata_t));
	  
	    /** Send the stats packet **/
	  
	    simple_udp_sendto(&client_conn,
			      &stats,
			      1 + sizeof(uint64_t) + sizeof(uint32_t) + sizeof(ltstatsdata_t),
			      &dest_ipaddr);
	    LOG_INFO("Sent stats packet timestamp %llu", n_time);
	    LOG_INFO_6ADDR(&dest_ipaddr);
	    LOG_INFO_("\n");
	    cfs_close(fstats);
#endif	  
	}
	
      } else {
	LOG_INFO("Not reachable yet\n");
      }
      etimer_set(&periodic_timer, SEND_INTERVAL
		 - CLOCK_SECOND + (random_rand() % (2 * CLOCK_SECOND)));
    }
  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
