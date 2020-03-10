
#include "contiki.h"
#include "dev/button-hal.h"
#include "net/routing/routing.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include "net/mac/tsch/tsch.h"
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


/*---------------------------------------------------------------------------*/
static struct etimer shutdown_timer,
  init_led_timer, sending_led_timer, et;
/*---------------------------------------------------------------------------*/
/* RE-Mote revision B, low-power PIC version */
#define PM_EXPECTED_VERSION               0x20
/*---------------------------------------------------------------------------*/
#define TEST_LEDS_FAIL                    leds_off(LEDS_ALL); \
                                          leds_on(LEDS_RED);  \
                                          PROCESS_EXIT();
/*---------------------------------------------------------------------------*/
#define TEST_ALARM_SECOND                 3600
/*---------------------------------------------------------------------------*/
static uint8_t rtc_buffer[sizeof(simple_td_map)];
static simple_td_map *simple_td = (simple_td_map *)rtc_buffer;

static uint16_t voltage;
static uint64_t network_uptime;
static int offset;

static int tot_steps, cur_step, clear_to_send, on_network, next_awake_set, resetted;

static char CFS_EOF = (char) -1, conf[4];
static int fdata, fconf;
/*---------------------------------------------------------------------------*/
static struct simple_udp_connection client_conn;

PROCESS(node_process, "RPL Node");
AUTOSTART_PROCESSES(&node_process);
/*---------------------------------------------------------------------------*/
static uint16_t get_voltage() {
  
  static uint8_t aux;
  static uint16_t voltage;
  
  aux = 0;
  
  /* Initialize the power management block and signal the low-power PIC */
  if(pm_enable() != PM_SUCCESS) {
    printf("PM: Failed to initialize\n");
    return 0;
  }
  /* Retrieve the firmware version and check expected */
  if((pm_get_fw_ver(&aux) == PM_ERROR) ||
     (aux != PM_EXPECTED_VERSION)) {
    printf("PM: unexpected version 0x%02X\n", aux);
    return 0;
  }
  
  printf("PM: firmware version 0x%02X OK\n", aux);
  
  /* Read the battery voltage level */
  if(pm_get_voltage(&voltage) != PM_SUCCESS) {
    printf("PM: error retrieving voltage\n");
    return 0;
  }
  
  printf("PM: Voltage (raw) = %u\n", voltage);
  return voltage;
}

int first_step(){
  return cur_step == 1;
}
int last_step(){
  return cur_step == tot_steps;
}
void inc_step(){
  cur_step++;
  fconf = cfs_open("root/run", CFS_WRITE);   
  cfs_seek(fconf, 4, CFS_SEEK_SET);
  memset(conf,0,4);
  memcpy(conf, &cur_step,4);
  cfs_write(fconf, conf, 4);
  cfs_write(fconf, &CFS_EOF, 1);
  cfs_close(fconf);
}

void raz_step(){
  cur_step = 1;
  fconf = cfs_open("root/run", CFS_WRITE);   
  cfs_seek(fconf, 4, CFS_SEEK_SET);
  memset(conf,0,4);
  memcpy(conf, &cur_step,4);
  cfs_write(fconf, conf, 4);
  cfs_write(fconf, &CFS_EOF, 1);
  cfs_close(fconf);
}
void init_data(uint64_t net_time, int ofst){
  fdata = cfs_open("root/data", CFS_WRITE|CFS_APPEND);
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
void record_data(){  
  fdata = cfs_open("root/data", CFS_WRITE |  CFS_APPEND);
  cfs_seek(fdata, -1, CFS_SEEK_END);
  uint16_t vlt = get_voltage();
  char char_vlt[2];
  memset(char_vlt, 0, 2);
  memcpy(char_vlt, &vlt,2);
  cfs_write(fdata, char_vlt,2);
  cfs_write(fdata, &CFS_EOF, 1);
  cfs_close(fdata);
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
    printf("PM: good night!\n");
  } else {
    printf("PM: error shutting down the system!\n");
  }
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(node_process, ev, data)
{
  button_hal_button_t *btn;
  static uint8_t initialized, blue,red;
  static struct etimer periodic_timer;
  uip_ipaddr_t dest_ipaddr;
  static struct cfs_dir dirp;
  static struct cfs_dirent dirent;
  static int sleep_frequency = SLEEP_FREQUENCY;
  
  PROCESS_BEGIN();
  LOG_INFO("START\n");
  /* Test the presence of the DATAFILE file
     If not present that is the first boot and 
     waits button pressed few seconds */

  fconf = cfs_opendir(&dirp,"root");
  fconf = cfs_readdir(&dirp, &dirent);

  /** First Boot and never seen a network **/
  
  if (fconf < 0) {    // First boot
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
	  //cfs_coffee_format();
	  leds_on(LEDS_BLUE);
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
	}
      }
    }
  }
  
  /** Read the current and total steps **/
  
  fconf = cfs_open("root/run", CFS_READ);
  memset(conf,0,4);
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
      record_data();
      inc_step();      
      leds_on(LEDS_GREEN);
      etimer_set(&et, CLOCK_SECOND /2);
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
      shutdown_to_next_step();
    }
    else {  // sensing and transmit
      record_data();
      clear_to_send = 1;
    }
  }
  etimer_set(&init_led_timer, CLOCK_SECOND);
  
  LOG_INFO("First or last step\n");
  
  
  /* Initialize UDP connections */
  simple_udp_register(&client_conn, UDP_CLIENT_PORT, NULL,
                      UDP_SERVER_PORT, NULL);
  LOG_INFO("Start network\n");
  NETSTACK_MAC.on();
  LOG_INFO("Network started\n");
  on_network = 0;
  next_awake_set = 0;
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
	printf("PM: good night!\n");
      } else {
	printf("PM: error shutting down the system!\n");
      }
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
      LOG_INFO("Periodic timer\n");
      if(NETSTACK_ROUTING.node_is_reachable() && NETSTACK_ROUTING.get_root_ipaddr(&dest_ipaddr)) {
	if(!on_network){
	  network_uptime = tsch_get_network_uptime_ticks();
	  offset = 0;
	  etimer_set(&shutdown_timer, NETWORKING_DURATION);
	  LOG_INFO("Networking during %d sec\n",NETWORKING_DURATION/CLOCK_SECOND);
	  etimer_set(&sending_led_timer, CLOCK_SECOND);
	  leds_off(LEDS_ALL);
	  etimer_stop(&init_led_timer);
	  on_network = 1;
	}
	
	if (clear_to_send) {
	  if (!resetted){
	    raz_step();    
	    resetted = 1;
	  }	  
	  
	  /** Build the data packet from sensor data file  **/
	  
	  fdata = cfs_open("root/data", CFS_READ);
	  ltdata_t data;
	  memset(&data, 0, sizeof(ltdata_t));
	  cfs_read(fdata, &data, sizeof(ltdata_t));
	  cfs_close(fdata);

	  /** Send the datapacket **/
	  
	  simple_udp_sendto(&client_conn, &data, sizeof(ltdata_t), &dest_ipaddr);
	  LOG_INFO("Sent network uptime timestamp %lu and voltage  %u to ", (unsigned long)network_uptime, voltage);
	  LOG_INFO_6ADDR(&dest_ipaddr);
	  LOG_INFO_("\n");
	  
	}
	
	/* First network access : store the time
	   and the firsts sensors data */
	
	if (first_step() && !resetted){
	  
	  /** Synchronization : all nodes boot at any time **/
	  /** save to the universal network time from TSCH  **/
	  /** All nodes will wake up at same absolute time **/
	  
	  
	  rtcc_sec_to_date(simple_td, network_uptime/CLOCK_SECOND);
	  
	  
	  // example with the next round five minutes
	  // i.e. one of :
	  // 00, 05, 10, 15, ... 55
	  
	  uint8_t ttw = simple_td->minutes % 2;
	  uint8_t sectw = simple_td->seconds % 60;
	  
	  offset = ((1 - ttw) * 60) + (60 - sectw);
	  
	  init_data(network_uptime, offset);
	  record_data();	  
	  inc_step();
	  LOG_INFO("Init data offset %d\n", offset);
	  
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
