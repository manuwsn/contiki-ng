
#include "contiki.h"
#include "dev/button-hal.h"
#include "net/routing/routing.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include "net/mac/tsch/tsch.h"
#include "lib/random.h"
#include "storage/cfs/cfs.h"
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
static struct etimer shutdown_timer,  init_led_timer, sending_led_timer, et;
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
  
  printf("PM: enabled!\n");
  
  
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

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(node_process, ev, data)
{
  button_hal_button_t *btn;
  static uint8_t initialized, blue,red, awake_initialized;
  static struct etimer periodic_timer;
  uip_ipaddr_t dest_ipaddr;
  static int fd;
  static struct cfs_dir dirp;
  static struct cfs_dirent dirent;
  
  PROCESS_BEGIN();
  LOG_INFO("START\n");
  /* Test the presence of the DATAFILE file
     If not present that is the first boot and 
     waits button pressed few seconds */

  fd = cfs_opendir(&dirp,"root");
  fd = cfs_readdir(&dirp, &dirent);
  if (fd < 0) {    // First boot
    LOG_INFO("First boot\n");
    /* Waiting for init button */
  
    btn = button_hal_get_by_index(0);
    initialized = 0;
    blue = 0;
    while(!initialized) {
      PROCESS_YIELD();
      if(ev == button_hal_periodic_event) {
	btn = (button_hal_button_t *)data;
	if(btn->press_duration_seconds > 3) {
	  leds_on(LEDS_BLUE);
	  blue=1;
	  initialized = 1;
	  etimer_set(&init_led_timer, CLOCK_SECOND);
	  fd = cfs_open("root/ltdata", CFS_WRITE | CFS_READ | CFS_APPEND);
	}
      }
    }
  }
  else {
    LOG_INFO("Reboot\n");
    fd = cfs_readdir(&dirp, &dirent);
  }
  etimer_set(&periodic_timer, random_rand() % SEND_INTERVAL);
  red=0;
  awake_initialized = 0;
  
  /* Initialize UDP connections */
  simple_udp_register(&client_conn, UDP_CLIENT_PORT, NULL,
                      UDP_SERVER_PORT, NULL);

  NETSTACK_MAC.on();

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
      leds_off(LEDS_ALL);
      leds_on(LEDS_BLUE);
      etimer_set(&et, CLOCK_SECOND * 3);
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
      if(pm_shutdown_now(PM_HARD_SLEEP_CONFIG) == PM_SUCCESS) {
	printf("PM: good night!\n");
      } else {
	printf("PM: error shutting down the system!\n");
	TEST_LEDS_FAIL;
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
      if(!awake_initialized){
	if(NETSTACK_ROUTING.node_is_reachable() && NETSTACK_ROUTING.get_root_ipaddr(&dest_ipaddr)) {
	  leds_off(LEDS_ALL);
	  etimer_stop(&init_led_timer);	
	  rtcc_sec_to_date(simple_td, network_uptime/CLOCK_SECOND);
	  rtcc_set_time_date(simple_td);
	  
	  uint8_t ttw = simple_td->minutes % 5;
	  uint8_t sectw = simple_td->seconds % 60;
	  rtcc_print(RTCC_PRINT_DATE_DEC);
	  
	  LOG_INFO("%d min %d sec\n", (4 - ttw) * 60, 60 - sectw);
	  rtcc_date_increment_seconds(simple_td, ((4 - ttw) * 60) + (60 - sectw));
	  pm_set_timeout(0x00);
	  rtcc_set_alarm_time_date(simple_td, RTCC_ALARM_ON, RTCC_REPEAT_DAY,
				   RTCC_TRIGGER_INT2);
	  LOG_INFO("Reveil à \n");
	  rtcc_print(RTCC_PRINT_ALARM_DEC);
	  
	  etimer_set(&shutdown_timer, NETWORKING_DURATION);
	  etimer_set(&sending_led_timer, CLOCK_SECOND);
	  
	  awake_initialized = 1;
	} else {
	  LOG_INFO("Not reachable yet\n");
	}
      } else {
	NETSTACK_ROUTING.get_root_ipaddr(&dest_ipaddr);
	voltage = get_voltage();
	/* Send network uptime timestamp to the DAG root */
	network_uptime = tsch_get_network_uptime_ticks();
	ltdata_t ltdata;
	ltdata.voltage = voltage;
	ltdata.time = network_uptime;
	simple_udp_sendto(&client_conn, &ltdata, sizeof(ltdata), &dest_ipaddr);
	LOG_INFO("Sent network uptime timestamp %lu and voltage  %u to ", (unsigned long)network_uptime, voltage);
	LOG_INFO_6ADDR(&dest_ipaddr);
	LOG_INFO_("\n");
      } 
      etimer_set(&periodic_timer, SEND_INTERVAL
		 - CLOCK_SECOND + (random_rand() % (2 * CLOCK_SECOND)));
    }
  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
