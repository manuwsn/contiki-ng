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


#if !NETSTACK_CONF_WITH_IPV6 || !UIP_CONF_ROUTER || !UIP_IPV6_MULTICAST || !UIP_CONF_IPV6_RPL
#error "This example can not work with the current contiki configuration"
#error "Check the values of: NETSTACK_CONF_WITH_IPV6, UIP_CONF_ROUTER, UIP_CONF_IPV6_RPL"
#endif

/*---------------------------------------------------------------------------*/
/* RE-Mote revision B, low-power PIC version */
#define PM_EXPECTED_VERSION               0x20
/*---------------------------------------------------------------------------*/
#define TEST_LEDS_FAIL                    leds_off(LEDS_ALL); \
                                          leds_on(LEDS_BLUE); \
					  PROCESS_EXIT();
/*---------------------------------------------------------------------------*/
#define UDP_CLIENT_PORT	8765
#define UDP_SERVER_PORT	5678
#define MCAST_SINK_UDP_PORT 3001 /* Host byte order */
/*---------------------------------------------------------------------------*/
#define NETWORKING_DURATION (DEEP_MAX * JOIN_DURATION)
#define DATA_ACK_MAX_TIME 20
#define RECORD_DATA_SIZE 100
/*---------------------------------------------------------------------------*/
static struct simple_udp_connection client_conn;
static struct uip_udp_conn *sink_conn;
/*---------------------------------------------------------------------------*/
static uint8_t rtc_buffer[sizeof(simple_td_map)];
static simple_td_map *simple_td = (simple_td_map *)rtc_buffer;
/*--------------------------------------------------------------------*/
static struct etimer init_led_timer;
static struct etimer waiting_mcst_timer;
static struct etimer wait_network_timer;
static struct etimer scan_network_timer;
static struct etimer timeout_tx_timer;
static struct etimer ack_timer;
static struct etimer onesec_timer;
/*--------------------------------------------------------------------*/
static uint8_t rendezvous;
static uint8_t initialized;
static uint8_t sended;
static uint8_t msg_acked;
/*--------------------------------------------------------------------*/
static uint8_t blue, red;
/*--------------------------------------------------------------------*/
static char CFS_EOF = (char) -2;
static int fconf;
static int fdata;
static int fbckp;
static int read;
/*--------------------------------------------------------------------*/
static uint64_t cycle_duration;
static uint64_t time_to_send;
static uint64_t time_to_sense;
static uint32_t sense_count;
static uint32_t sense_nb;
static uint64_t start_time;
static uint64_t time_offset;
/*--------------------------------------------------------------------*/
static uip_ipaddr_t dest_ipaddr;
/*--------------------------------------------------------------------*/

//static uint8_t msg_seqno;
//static uint64_t itts;
//static int ite2;
/*---------------------------------------------------------------------------*/
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

    msg_acked = 1;
  //LOG_INFO("recu %d pour %d donc %d\n", ack_rcv, msg_seqno, msg_acked);
}
/*---------------------------------------------------------------------------*/
static void
tcpip_handler(void)
{
  if(uip_newdata()) {
    memcpy(&start_time, (uint8_t*)uip_appdata, sizeof(uint64_t));
    memcpy(&time_to_send, &((uint8_t*)uip_appdata)[sizeof(uint64_t)], sizeof(uint64_t));
    memcpy(&cycle_duration,  &((uint8_t*)uip_appdata)[2 * sizeof(uint64_t)], sizeof(uint64_t));
    memcpy(&sense_nb,  &((uint8_t*)uip_appdata)[3 * sizeof(uint64_t)], sizeof(uint32_t));

    LOG_INFO("tts %llu cd %llu fs %lu\n", time_to_send,cycle_duration,sense_nb );
    
    sense_count = 0;
    rendezvous = 1;
  }
  return;
}
/*---------------------------------------------------------------------------*/
void record_sensor_data(){
  int fldata = cfs_open("lt-data", CFS_WRITE | CFS_APPEND);
  cfs_seek(fldata, -1, CFS_SEEK_END);
  ltdata_t ltdata;
  memset(&ltdata, 0, sizeof(ltdata_t));
  ltdata_read_sensor_data(&ltdata);
  if(start_time != 0)
    cfs_write(fldata, &start_time, sizeof(uint64_t));
  else
    cfs_write(fldata, &time_offset, sizeof(uint64_t));
  cfs_write(fldata, &ltdata,sizeof(ltdata_t));
  cfs_write(fldata, &CFS_EOF, 1);
  cfs_close(fldata);
}
/*---------------------------------------------------------------------------*/
void backup(){
  int bck = cfs_open("lt-bckp", CFS_WRITE | CFS_APPEND);
  int fldata = cfs_open("lt-data", CFS_READ);
  cfs_seek(bck, -1, CFS_SEEK_END);
  uint8_t record[RECORD_DATA_SIZE];
  memset(record, 0, RECORD_DATA_SIZE);
  int lread = cfs_read(fldata, record, sizeof(uint64_t) + sizeof(ltdata_t));
  while (lread > 1){
    cfs_write(bck, record, sizeof(uint64_t) + sizeof(ltdata_t));
    memset(record, 0, RECORD_DATA_SIZE);
    lread = cfs_read(fldata, record, sizeof(uint64_t) + sizeof(ltdata_t));
  }
  cfs_write(bck, &CFS_EOF, 1);
  cfs_close(bck);
  cfs_close(fldata);
}
/*---------------------------------------------------------------------------*/
void awake_time(uint64_t tts){
  rtcc_sec_to_date(simple_td, 0);
  rtcc_set_time_date(simple_td);
  rtcc_date_increment_seconds(simple_td, tts);
  rtcc_set_alarm_time_date(simple_td, RTCC_ALARM_ON, RTCC_REPEAT_DAY,
			   RTCC_TRIGGER_INT2);
}
/*---------------------------------------------------------------------------*/
/*
static void shutdown_now(){
  pm_set_timeout(0x00);
  
  if(pm_shutdown_now(PM_HARD_SLEEP_CONFIG) != PM_SUCCESS) 
    TEST_LEDS_FAIL;
    }*/
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(node_process, ev, data)
{
  
  button_hal_button_t *btn;
  
  PROCESS_BEGIN();
  LOG_INFO("START\n");
  
  pm_enable();
  /*
  LOG_INFO("cycle %lu\n", pm_get_num_cycles());
  fdata = cfs_open("lt-data", CFS_READ);
  if (fdata < 0)
    LOG_INFO("No lt-data file\n");
  else {
    LOG_INFO("Yes lt-data file\n");
    cfs_close(fdata);
  }
  */
  
  fconf = cfs_open("conf", CFS_READ);
  
  if (fconf < 0) {
    
  /** first boot after flashing or too much reboot **/

    //LOG_INFO("First boot\n");
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

  // conf file exist
  

  // read the current conf
  
  fconf = cfs_open("conf", CFS_READ);
  uint8_t conf_buf[2];
  cfs_read(fconf, conf_buf, 2 * sizeof(uint8_t));
  cfs_close(fconf);

  
  if (conf_buf[0] == 0){

     // Always waiting for mcst
    
    if (conf_buf[1] < MAX_ATTEMPT_MCST){      // try again
	conf_buf[1]++;                          
      fconf = cfs_open("conf", CFS_WRITE);
      cfs_write(fconf, conf_buf, 2 * sizeof(uint8_t));
      cfs_write(fconf, &CFS_EOF, 1);
      cfs_close(fconf);
    }
    else {             // too much tries, remove conf and shutdwown
      cfs_remove("conf");
      
      leds_off(LEDS_ALL);
      leds_on(LEDS_GREEN);
      etimer_set(&onesec_timer, CLOCK_SECOND*2);
      PROCESS_WAIT_UNTIL(etimer_expired(&onesec_timer));
      leds_off(LEDS_ALL);

      awake_time(5);
      pm_set_timeout(0x00);
      if(pm_shutdown_now(PM_HARD_SLEEP_CONFIG) == PM_SUCCESS) {
	printf("PM: good night for %d\n",2);
      } else{
	TEST_LEDS_FAIL;
      }
      
    }
    
    // Multicast communication try 
    // start network
  
    NETSTACK_MAC.on();
    
    etimer_set(&waiting_mcst_timer, WAIT_MCST_TIME * CLOCK_SECOND);
    
    sink_conn = udp_new(NULL, UIP_HTONS(0), NULL);
    udp_bind(sink_conn, UIP_HTONS(MCAST_SINK_UDP_PORT));

    rendezvous = 0;
    LOG_INFO("Waiting multicast\n");
    while(!rendezvous) {
      PROCESS_WAIT_EVENT_UNTIL(ev == tcpip_event || ev == PROCESS_EVENT_TIMER);
      if (ev == tcpip_event)
	tcpip_handler();
      if (ev == PROCESS_EVENT_TIMER){
	if (data == &init_led_timer){
	  if (blue){              // blue led blinking
	    leds_off(LEDS_ALL);
	    blue=0;
	  }
	  else {
	    leds_on(LEDS_BLUE);
	    blue=1;
	  }
	  etimer_reset(&init_led_timer);
	}
	if (data == &waiting_mcst_timer){

	  leds_off(LEDS_ALL);
	  leds_on(LEDS_RED);
	  etimer_set(&onesec_timer, CLOCK_SECOND*2);
	  PROCESS_WAIT_UNTIL(etimer_expired(&onesec_timer));
	  leds_on(LEDS_BLUE);
	  etimer_set(&onesec_timer, CLOCK_SECOND*2);
	  PROCESS_WAIT_UNTIL(etimer_expired(&onesec_timer));
	  leds_off(LEDS_ALL);
	  
	  awake_time((uint64_t) RETRY_MCST_TIME);
	 
	  pm_set_timeout(0x00);
	  if(pm_shutdown_now(PM_HARD_SLEEP_CONFIG) == PM_SUCCESS) {
	    printf("PM: good night for %d\n",RETRY_MCST_TIME);
	  } else{
	    TEST_LEDS_FAIL;
	  }
	}
      }
	 
    }  // end while !rendezvous
    
    leds_off(LEDS_ALL);  // clear the blue led
    
    // Writing conf file with received parameters
    
    //LOG_INFO("receive parameter\n");
    
    cfs_remove("conf");
    uint8_t buf[25];
    buf[0] = 1;  // conf ok
    memcpy(&buf[sizeof(uint8_t)], &time_to_send, sizeof(uint64_t));
    memcpy(&buf[sizeof(uint8_t) + sizeof(uint64_t)], &cycle_duration, sizeof(uint64_t));
    memcpy(&buf[sizeof(uint8_t) + 2 * sizeof(uint64_t)], &sense_nb, sizeof(uint32_t));
    memcpy(&buf[sizeof(uint8_t) + 2 * sizeof(uint64_t) + sizeof(uint32_t)], &sense_count, sizeof(uint32_t));
    fconf = cfs_open("conf", CFS_WRITE);
    cfs_write(fconf, buf, 25 * sizeof(uint8_t));
    cfs_write(fconf, &CFS_EOF, 1);
    cfs_close(fconf);
    
    LOG_INFO("receive parameter\nttsend %llu\ncycle duration %llu\nsense_nb %lu\nsense count %lu\n",time_to_send,cycle_duration,sense_nb,sense_count);
    
    // Lets the multicast network living for other nodes
    if (!etimer_expired(&waiting_mcst_timer))
      PROCESS_WAIT_UNTIL(etimer_expired(&waiting_mcst_timer));
    
    //LOG_INFO("Stop waiting Mcst\n");
  }

  // A complete conf file exist, read it
    
  uint8_t buf[25];
  fconf = cfs_open("conf", CFS_READ);
  cfs_read(fconf, buf, 25*sizeof(uint8_t));
  cfs_close(fconf);
  memcpy(&time_to_send,   &buf[sizeof(uint8_t)], sizeof(uint64_t));
  memcpy(&cycle_duration, &buf[sizeof(uint8_t)+ sizeof(uint64_t)], sizeof(uint64_t));
  memcpy(&sense_nb,       &buf[sizeof(uint8_t)+ (2 * sizeof(uint64_t))], sizeof(uint32_t));
  memcpy(&sense_count,    &buf[sizeof(uint8_t)+ (2 * sizeof(uint64_t)) + sizeof(uint32_t)], sizeof(uint32_t));

    LOG_INFO("read parameters\nttsend %llu\ncycle duration %llu\nsense_nb %lu\nsense count %lu\n",time_to_send,cycle_duration,sense_nb,sense_count);
  if (sense_nb == 0) {    // end of cycle period : have to clear conf file and wait again
    
      cfs_remove("conf");
      fconf = cfs_open("conf", CFS_WRITE);
      uint8_t buf[2];
      buf[0] = 0;
      buf[1] = 0;
      memset(buf, 0, 2 * sizeof(uint8_t));
      cfs_write(fconf, buf, 2 * sizeof(uint8_t));
      cfs_write(fconf, &CFS_EOF, 1);
      cfs_close(fconf);
    
      awake_time(2);
      pm_set_timeout(0x00);
      leds_off(LEDS_ALL);
      if(pm_shutdown_now(PM_HARD_SLEEP_CONFIG) == PM_SUCCESS) {
	printf("PM: good night for %d\n",2);
      } else{
	TEST_LEDS_FAIL;
      }
  }
  
  /********************************************************************/
  /****************************  SENSING PART *************************/
  /********************************************************************/
  
  // sense_count is initialized to 0
  // So first we increment sense_count

  sense_count++;

  if (sense_count == 1 && sense_nb == 1) {// special case one sensing
    
    time_offset = 0;                 // record the date
      
    record_sensor_data();
      
    time_to_sense = time_to_send / sense_nb;    // for the next awake
    time_to_sense = time_to_sense + (time_to_send - time_to_sense * sense_nb);
    
    cfs_remove("conf");
    memcpy(&buf[sizeof(uint8_t) + (2 * sizeof(uint64_t)) +  sizeof(uint32_t)], &sense_count, sizeof(uint32_t));
    fconf = cfs_open("conf", CFS_WRITE);
    cfs_write(fconf, buf, 25 * sizeof(uint8_t));
    cfs_write(fconf, &CFS_EOF, 1);
    cfs_close(fconf);

    /*
    itts = 0;
    for (itts = 0; itts < time_to_sense;itts++){
      if (itts % 2 == 0)
	leds_off(LEDS_ALL);
      else
	leds_on(LEDS_GREEN);
      etimer_set(&onesec_timer, CLOCK_SECOND/3);
      PROCESS_WAIT_UNTIL(etimer_expired(&onesec_timer));
    }
    */
    awake_time(time_to_sense);
   
    pm_set_timeout(0x00);
    leds_off(LEDS_ALL);
    if(pm_shutdown_now(PM_HARD_SLEEP_CONFIG) == PM_SUCCESS) {
      printf("PM: good night for %llu\n",time_to_sense);
    } else{
      TEST_LEDS_FAIL;
    }
  }
    
  if (sense_count < sense_nb) {  // not the last awake before sending   
      
    time_to_sense = time_to_send / sense_nb;
    
    if (sense_count == 1){             // if first sensing
      time_offset = 0;                 // record the date
    }
    else {                             // other sensing       
      fdata = cfs_open("lt-data", CFS_READ);	  
      cfs_read(fdata, &start_time, sizeof(uint64_t));
      cfs_close(fdata);
      time_offset = start_time + (time_to_sense * sense_count);
      start_time = 0;                  // record current time, not start time
    }
    if(sense_count == sense_nb - 1) { // for the last one
      time_to_sense = time_to_sense + (time_to_send - time_to_sense * sense_nb);
    }
    record_sensor_data();             // Sense and record
                                      // update conf file
    cfs_remove("conf");
    memcpy(&buf[sizeof(uint8_t) + (2 * sizeof(uint64_t)) + sizeof(uint32_t)], &sense_count, sizeof(uint32_t));
    fconf = cfs_open("conf", CFS_WRITE);
    cfs_write(fconf, buf, 25 * sizeof(uint8_t));
    cfs_write(fconf, &CFS_EOF, 1);
    cfs_close(fconf);
    /*
    itts = 0;
    for (itts = 0; itts < time_to_sense;itts++){
      if (itts % 2 == 0)
	leds_off(LEDS_ALL);
      else
	leds_on(LEDS_GREEN);
      etimer_set(&onesec_timer, CLOCK_SECOND/3);
      PROCESS_WAIT_UNTIL(etimer_expired(&onesec_timer));
    }
    
    if (time_to_sense == 0){
      for (itts = 0; itts < 5;itts++){
	if (itts % 2 == 0)
	  leds_off(LEDS_ALL);
	else
	  leds_on(LEDS_BLUE);
      etimer_set(&onesec_timer, CLOCK_SECOND/3);
      PROCESS_WAIT_UNTIL(etimer_expired(&onesec_timer));
      }
    }
    */
    LOG_INFO("Conf %llu %llu %llu %llu %lu\n", start_time, cycle_duration, time_to_send, time_to_sense, sense_nb);
    awake_time(time_to_sense);   
    pm_set_timeout(0x00);
    leds_off(LEDS_ALL);
    if(pm_shutdown_now(PM_HARD_SLEEP_CONFIG) == PM_SUCCESS) {
      printf("PM: good night for %llu\n",time_to_sense);
    } else{
      TEST_LEDS_FAIL;
    }
  }
  else {                            
    /********************************************************************/
    /****************************  SENDING DATA PART ********************/
    /********************************************************************/
    // send data or backup it

    NETSTACK_MAC.on();
    
    simple_udp_register(&client_conn, UDP_CLIENT_PORT, NULL,
			UDP_SERVER_PORT,udp_rx_callback);

    red = 1;
    
    sended = 0;
    etimer_set(&wait_network_timer, CLOCK_SECOND * NETWORKING_DURATION);
    etimer_set(&scan_network_timer, CLOCK_SECOND);
    
    while (!sended){
      
      if(NETSTACK_ROUTING.node_is_reachable() &&
	 NETSTACK_ROUTING.get_root_ipaddr(&dest_ipaddr)) {

	char send_data[RECORD_DATA_SIZE];
	memset(&send_data, 0, RECORD_DATA_SIZE);
	
	fbckp = cfs_open("lt-bckp", CFS_READ);
	if (fbckp > 0){
	  read = cfs_read(fbckp, &send_data[1], sizeof(uint64_t) + sizeof(ltdata_t));
	
	  while(read > 1){
	    send_data[0] = 'B';
	    msg_acked = 0;

	    etimer_set(&timeout_tx_timer, DATA_ACK_MAX_TIME * CLOCK_SECOND);
	    simple_udp_sendto(&client_conn,  send_data, 1 + sizeof(uint64_t) + sizeof(ltdata_t), &dest_ipaddr);
	    etimer_set(&ack_timer, CLOCK_SECOND/4);
	    
	    while(!msg_acked){
	      PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER);
	      if (data == &ack_timer){
		etimer_restart(&ack_timer);
	      }
	      else if (data == &timeout_tx_timer){
		// reboot to try again as we was transmiting
		// until wait network expired
		// at least some records will be duplicated
		awake_time(2);
		pm_set_timeout(0x00);
		leds_off(LEDS_ALL);
		if(pm_shutdown_now(PM_HARD_SLEEP_CONFIG) == PM_SUCCESS) {
		  printf("PM: good night for %d\n",2);
		} else{
		  TEST_LEDS_FAIL;
		}
	      }
	    }
	    // Message sended and acknowledged
	    memset(&send_data, 0, RECORD_DATA_SIZE);
	    // next record
	    read = cfs_read(fbckp, &send_data[1], sizeof(uint64_t) + sizeof(ltdata_t));
	  }
	  cfs_close(fbckp);
	  cfs_remove("lt-bckp");
	}
	
	fdata = cfs_open("lt-data", CFS_READ);
	
	read = cfs_read(fdata, &send_data[1], sizeof(uint64_t) + sizeof(ltdata_t));
	
	while(read > 1){
	  send_data[0] = 'D';
	  msg_acked = 0;

	  etimer_set(&timeout_tx_timer, DATA_ACK_MAX_TIME * CLOCK_SECOND);
	  simple_udp_sendto(&client_conn,  send_data, 1 + sizeof(uint64_t) + sizeof(ltdata_t), &dest_ipaddr);
	  etimer_set(&ack_timer, CLOCK_SECOND/4);

	  while(!msg_acked){
	    PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER);
	    if (data == &ack_timer){
	      etimer_restart(&ack_timer);
	    }
	    else if (data == &timeout_tx_timer){
	      // reboot to try again as we was transmiting
	      // until wait network expired
	      // at least some records will be duplicated
	      awake_time(2);
	      pm_set_timeout(0x00);
	      leds_off(LEDS_ALL);
	      if(pm_shutdown_now(PM_HARD_SLEEP_CONFIG) == PM_SUCCESS) {
		printf("PM: good night for %d\n",2);
	      } else{
		TEST_LEDS_FAIL;
	      }
	    }
	  }
	  // Message sended and acknowledged
	  memset(&send_data, 0, RECORD_DATA_SIZE);
	  // next record
	  read = cfs_read(fdata, &send_data[1], sizeof(uint64_t) + sizeof(ltdata_t));
	}
	cfs_close(fdata);
	sended=1;
      }                                  // pas ou plus de réseaux
      else {
	PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER);
	if (data == &scan_network_timer){            // scan pulse
	  etimer_restart(&scan_network_timer);         // re generation
	  if (etimer_expired(&wait_network_timer))   // Perte de réseau
	    etimer_restart(&wait_network_timer);       // on tente à nouveau car on l'avait avant
	}
	else if (data == &wait_network_timer){      // end of wainting network

	  // backup data file
	  
	  backup();
	  cfs_remove("lt-data");

	  // update the conf file
	  // to a computed time to sense from current one and cycle duration
	  // compute a new sense number and set sense count to 0

	  uint64_t k = time_to_send / sense_nb;
	  
	  if (cycle_duration < NETWORKING_DURATION) // should not occurs
	    cycle_duration = NETWORKING_DURATION * 2;  // arbitrary cycle
	  time_to_send = cycle_duration - NETWORKING_DURATION;

	  sense_nb = time_to_send / k;
	  sense_count = 0;
	  
	  cfs_remove("conf");
	  uint8_t buf[25];
	  buf[0] = 1;  // conf ok
	  memcpy(&buf[sizeof(uint8_t)], &time_to_send, sizeof(uint64_t));
	  memcpy(&buf[sizeof(uint8_t) + sizeof(uint64_t)], &cycle_duration, sizeof(uint64_t));
	  memcpy(&buf[sizeof(uint8_t) + 2 * sizeof(uint64_t)], &sense_nb, sizeof(uint32_t));
	  memcpy(&buf[sizeof(uint8_t) + 2 * sizeof(uint64_t) + sizeof(uint32_t)], &sense_count, sizeof(uint32_t));
	  fconf = cfs_open("conf", CFS_WRITE);
	  cfs_write(fconf, buf, 25 * sizeof(uint8_t));
	  cfs_write(fconf, &CFS_EOF, 1);
	  cfs_close(fconf);

	  // awake for a next sensing
	  time_to_sense = time_to_send / sense_nb;
	  
	  awake_time(time_to_sense);
	  pm_set_timeout(0x00);
	  leds_off(LEDS_ALL);
	  if(pm_shutdown_now(PM_HARD_SLEEP_CONFIG) == PM_SUCCESS) {
	    printf("PM: good night for %d\n",2);
	  } else{
	    TEST_LEDS_FAIL;
	  }
	  
	}
	if (red){
	  leds_off(LEDS_ALL);
	  leds_on(LEDS_RED);
	  etimer_set(&onesec_timer, CLOCK_SECOND/4);
	  PROCESS_WAIT_UNTIL(etimer_expired(&onesec_timer));
	  red = 0;
	}else{
	  leds_off(LEDS_ALL);
	  etimer_set(&onesec_timer, CLOCK_SECOND);
	  PROCESS_WAIT_UNTIL(etimer_expired(&onesec_timer));
	  red = 1;
	}
      }
    }
    
    leds_off(LEDS_ALL);
	  
    // send backup if any
    // remove data file

    cfs_remove("lt-data");

    // networking the remaining time

    if (!etimer_expired(&wait_network_timer))
      PROCESS_WAIT_UNTIL(etimer_expired(&wait_network_timer));

    // Prepare a new cycle with a blank conf file
    
      cfs_remove("conf");
      fconf = cfs_open("conf", CFS_WRITE);
      uint8_t buf[2];
      buf[0] = 0;
      buf[1] = 0;
      memset(buf, 0, 2 * sizeof(uint8_t));
      cfs_write(fconf, buf, 2 * sizeof(uint8_t));
      cfs_write(fconf, &CFS_EOF, 1);
      cfs_close(fconf);
    
      awake_time(2);
      pm_set_timeout(0x00);
      leds_off(LEDS_ALL);
      if(pm_shutdown_now(PM_HARD_SLEEP_CONFIG) == PM_SUCCESS) {
	printf("PM: good night for %d\n",2);
      } else{
	TEST_LEDS_FAIL;
      }
  }
  /*
  leds_off(LEDS_ALL);
  leds_on(LEDS_BLUE);
  etimer_set(&onesec_timer, CLOCK_SECOND*3);
  PROCESS_WAIT_UNTIL(etimer_expired(&onesec_timer));
  leds_off(LEDS_ALL);
  */
  PROCESS_END();
}
