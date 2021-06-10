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
#include "sys/energest.h"

#include "dev/watchdog.h"

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
#include <string.h>
#include <unistd.h>

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
					  //PROCESS_EXIT();
/*---------------------------------------------------------------------------*/
#define UDP_CLIENT_PORT	8765
#define UDP_SERVER_PORT	5678
#define RDV_CLIENT_PORT	8766
#define RDV_SERVER_PORT	5679
#define MCAST_SINK_UDP_PORT 3001 /* Host byte order */
/*---------------------------------------------------------------------------*/
#define DATA_ACK_MAX_TIME 20
#define RECORD_DATA_SIZE 100
/*---------------------------------------------------------------------------*/
static struct simple_udp_connection udp_conn;//, rdv_conn;
/*---------------------------------------------------------------------------*/
static uint8_t rtc_buffer[sizeof(simple_td_map)];
static simple_td_map *simple_td = (simple_td_map *)rtc_buffer;
/*--------------------------------------------------------------------*/
static struct etimer ack_timer;
static struct etimer onesec_timer;
static struct etimer rdv_timer;
static struct etimer tx_timer;
static struct etimer tx_guard_timer;
static struct etimer rdv_guard_timer;
static uint64_t tx_time_slot = NETWORKING_TIME_SLOT;
static uint64_t rdv_time_slot = DEFAULT_RDV_TIME_SLOT;
static uint64_t rdv_guard_time;
/*--------------------------------------------------------------------*/
static uint8_t msg_acked;
static uint8_t files_sended;
static uint8_t rdv;
static uint8_t rdv_req[9];
static int max_int_tx;
/*--------------------------------------------------------------------*/
struct data_file_handler {
  int fd;
  int rs;
  char type;
  char name[15];
  void (*backup)(void);
};
static char CFS_EOF = (char) -2;
static struct data_file_handler data_files[4];
static uint8_t i_file;
static int file_read;
/*--------------------------------------------------------------------*/
static uint64_t cycle_duration;
static uint64_t time_to_send;
static uint32_t sense_count;
static uint32_t sense_nb;
static uint64_t start_time;
static uint64_t start_of_rdv;
static uint64_t start_of_tx;
//static uint8_t state;
/*--------------------------------------------------------------------*/
static uip_ipaddr_t dest_ipaddr;
/*--------------------------------------------------------------------*/
static 	char send_data[RECORD_DATA_SIZE];
//static uint8_t msg_seqno;
//static uint64_t itts;
//static int ite2;
/*---------------------------------------------------------------------------*/
PROCESS(node_process, "LT-WSN Node");
PROCESS(rdv_process, "Rdv");
PROCESS(tx_process, "Tx");
AUTOSTART_PROCESSES(&node_process);
/*---------------------------------------------------------------------------*/

static uint64_t rdv_timestp_req, rdv_timestp_rsp;
static void
rdv_callback(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{
  if (!rdv){
    
    memcpy(&rdv_timestp_rsp,&data[5 * sizeof(uint64_t) + sizeof(uint32_t)], sizeof(uint64_t));
    if (rdv_timestp_rsp != rdv_timestp_req){
      rdv_timestp_rsp = 0;
      return;
    }

    memcpy(&start_time,     data, sizeof(uint64_t));
    memcpy(&time_to_send,   &data[sizeof(uint64_t)], sizeof(uint64_t));
    memcpy(&cycle_duration, &data[2 * sizeof(uint64_t)], sizeof(uint64_t));
    memcpy(&sense_nb,       &data[3 * sizeof(uint64_t)], sizeof(uint32_t));
    memcpy(&rdv_time_slot,  &data[3 * sizeof(uint64_t) + sizeof(uint32_t)], sizeof(uint64_t));
    memcpy(&tx_time_slot,   &data[4 * sizeof(uint64_t) + sizeof(uint32_t)], sizeof(uint64_t));
    memcpy(&rdv_timestp_rsp,&data[5 * sizeof(uint64_t) + sizeof(uint32_t)], sizeof(uint64_t));

    LOG_INFO("tts %llu sense nb  %lu\n",time_to_send,sense_nb);
    
    uint64_t now = clock_time();
    
    if (now > rdv_timestp_req) {
      uint64_t delta = (now - rdv_timestp_req)/(2*CLOCK_SECOND);
      if(time_to_send > delta) {
	time_to_send = time_to_send - delta;
	//LOG_INFO("tts ajusté %llu de %llu\n",time_to_send, delta);
      }
      else {
	time_to_send = 0;
      }
    }

    now = now / CLOCK_SECOND;
    uint64_t srdv = etimer_start_time(&rdv_timer) / CLOCK_SECOND;
    uint64_t erdv = srdv + rdv_time_slot;
    uint64_t ttsend = now + time_to_send;

    //LOG_INFO("At %llu , from %llu until %llu\n", now, srdv, erdv);
    //LOG_INFO("Send at %llu\n", ttsend);
    
    if (erdv > ttsend){
      int adjust = erdv - ttsend;
      etimer_adjust(&rdv_timer, -adjust*CLOCK_SECOND);
      sense_nb = 0;  // we have no time to sense and sleep before tx
      time_to_send = 0;

      //LOG_INFO("Adjust of %d\n", -adjust);
    }
    else {
      if (erdv > now && time_to_send > (erdv - now))   // To be absolutly certain
	time_to_send = time_to_send - (erdv - now);
      //LOG_INFO("time to send reduce of %llu, is : %llu\n", erdv - now, time_to_send);
    }
    
    sense_count = 0;
    rdv = 1;
  }
}
/*---------------------------------------------------------------------------*/
static void
tx_callback(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{
    msg_acked = 1;
}
/*---------------------------------------------------------------------------*/
void record_energest_data(){
  uint8_t nrg[32];
  uint64_t nrgtime = 0;
  memset(nrg, 0, 4 * sizeof(uint64_t));
  int fnrg = cfs_open("energest", CFS_WRITE | CFS_APPEND);
  cfs_seek(fnrg, -1, CFS_SEEK_END);
  
  if (sense_nb == 0){
    cfs_write(fnrg, &start_time, sizeof(uint64_t));
  }
  else {
    uint64_t K = time_to_send / sense_nb;
    uint64_t vtime = start_time + (K * sense_count);
    cfs_write(fnrg, &vtime, sizeof(uint64_t));
  }
  energest_flush();
  nrgtime = energest_type_time(ENERGEST_TYPE_CPU);
  memcpy(nrg, &nrgtime, sizeof(uint64_t));
  nrgtime = energest_type_time(ENERGEST_TYPE_LPM);
  memcpy(&nrg[sizeof(uint64_t)], &nrgtime, sizeof(uint64_t));
  nrgtime = energest_type_time(ENERGEST_TYPE_LISTEN);
  memcpy(&nrg[2 * sizeof(uint64_t)], &nrgtime, sizeof(uint64_t));
  nrgtime = energest_type_time(ENERGEST_TYPE_TRANSMIT);
  memcpy(&nrg[3 * sizeof(uint64_t)], &nrgtime, sizeof(uint64_t));
  cfs_write(fnrg, nrg ,4 * sizeof(uint64_t));
  cfs_write(fnrg, &CFS_EOF, 1);
  cfs_close(fnrg);
} 
/*---------------------------------------------------------------------------*/
void record_sensor_data(){
  int fldata = cfs_open("lt-data", CFS_WRITE | CFS_APPEND);
  cfs_seek(fldata, -1, CFS_SEEK_END);
  ltdata_t ltdata;
  memset(&ltdata, 0, sizeof(ltdata_t));
  ltdata_read_sensor_data(&ltdata);

  if (sense_nb == 0){
    cfs_write(fldata, &start_time, sizeof(uint64_t));
  }
  else {
    uint64_t K = time_to_send / sense_nb;
    uint64_t vtime = start_time + (K * sense_count);
    cfs_write(fldata, &vtime, sizeof(uint64_t));
  }
  cfs_write(fldata, &ltdata,sizeof(ltdata_t));
  cfs_write(fldata, &CFS_EOF, 1);
  cfs_close(fldata);
}
/*---------------------------------------------------------------------------*/
void backup_data(){
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
  cfs_remove("lt-data");
}
/*---------------------------------------------------------------------------*/
void backup_nrg(){
  int bck = cfs_open("energest-bckp", CFS_WRITE | CFS_APPEND);
  int flnrg = cfs_open("energest", CFS_READ);
  cfs_seek(bck, -1, CFS_SEEK_END);
  uint8_t record[40];
  memset(record, 0, 40);
  int lread = cfs_read(flnrg, record, 5 * sizeof(uint64_t));
  while (lread > 1){
    cfs_write(bck, record, 5 * sizeof(uint64_t));
    memset(record, 0, 40);
    lread = cfs_read(flnrg, record, 5 * sizeof(uint64_t));
  }
  cfs_write(bck, &CFS_EOF, 1);
  cfs_close(bck);
  cfs_close(flnrg);
  cfs_remove("energest");
}
/*---------------------------------------------------------------------------*/
void backup_none(){  // function of backup for backuped files :
                     // do nothing to leave the file as is
}
/*---------------------------------------------------------------------------*/
static void backup(){
  for (i_file = 0; i_file < 4; i_file++)
    if (data_files[i_file].fd != -1)
      data_files[i_file].backup();
}
/*---------------------------------------------------------------------------*/
static int read_conf(){
  uint8_t buf[48];
  int fconf = cfs_open("conf", CFS_READ);
  if (fconf == -1)
    return 0;
  cfs_read(fconf, buf, 48 * sizeof(uint8_t));
  cfs_close(fconf);
  memcpy(&start_time,    buf, sizeof(uint64_t));
  memcpy(&time_to_send,  &buf[sizeof(uint64_t)], sizeof(uint64_t));
  memcpy(&cycle_duration,&buf[2 * sizeof(uint64_t)], sizeof(uint64_t));
  memcpy(&sense_nb,      &buf[3 * sizeof(uint64_t)], sizeof(uint32_t));
  memcpy(&sense_count,   &buf[3 * sizeof(uint64_t) + sizeof(uint32_t)], sizeof(uint32_t));
  memcpy(&rdv_time_slot, &buf[3 * sizeof(uint64_t) + 2 * sizeof(uint32_t)], sizeof(uint64_t));
  memcpy(&tx_time_slot,  &buf[4 * sizeof(uint64_t) + 2 * sizeof(uint32_t)], sizeof(uint64_t));
  return 1;
}
/*---------------------------------------------------------------------------*/
static int write_conf(){
  cfs_remove("conf");
  uint8_t buf[48];
  memcpy(buf                   , &start_time, sizeof(uint64_t));
  memcpy(&buf[sizeof(uint64_t)], &time_to_send, sizeof(uint64_t));
  memcpy(&buf[2 * sizeof(uint64_t)], &cycle_duration, sizeof(uint64_t));
  memcpy(&buf[3 * sizeof(uint64_t)], &sense_nb, sizeof(uint32_t));
  memcpy(&buf[3 * sizeof(uint64_t) + sizeof(uint32_t)], &sense_count, sizeof(uint32_t));
  memcpy(&buf[3 * sizeof(uint64_t) + 2 * sizeof(uint32_t)], &rdv_time_slot, sizeof(uint64_t));
  memcpy(&buf[4 * sizeof(uint64_t) + 2 * sizeof(uint32_t)], &tx_time_slot, sizeof(uint64_t));
  int fconf = cfs_open("conf", CFS_WRITE);
  if (fconf == -1)
    return 0;
  cfs_write(fconf, buf, 48 * sizeof(uint8_t));
  cfs_write(fconf, &CFS_EOF, 1);
  cfs_close(fconf);
  return 1;
}
/*---------------------------------------------------------------------------*/
static int its_time_to_send(){
  return sense_count > sense_nb;
}
/*---------------------------------------------------------------------------*/
static void sensing(){
  record_sensor_data();
  record_energest_data();
  sense_count = sense_count + 1;
  write_conf();
}
/*---------------------------------------------------------------------------*/

static void sense_and_sleep(){
  uint64_t ttsleep;

  sensing();
  
  if (sense_nb == 0){
    ttsleep = time_to_send;
  }
  else {
    if (sense_count < sense_nb) {
      ttsleep = time_to_send / sense_nb;
    }
    else {
      ttsleep = (time_to_send / sense_nb) + (time_to_send % sense_nb);
    }
  }
  
  if(ttsleep < 2)
    ttsleep = 2;
    
  rtcc_get_time_date(simple_td);
  rtcc_date_increment_seconds(simple_td, ttsleep);
  pm_set_timeout(0x00);
  rtcc_set_alarm_time_date(simple_td, RTCC_ALARM_ON, RTCC_REPEAT_DAY,
			   RTCC_TRIGGER_INT2);
  if(pm_shutdown_now(PM_HARD_SLEEP_CONFIG) != PM_SUCCESS) {
    leds_off(LEDS_ALL); 
    leds_on(LEDS_BLUE);
  }
}
/*---------------------------------------------------------------------------*/
static void reset_conf(){
  cfs_remove("conf");
  backup();
}
/*---------------------------------------------------------------------------*/
static process_event_t tx_event;
static uint8_t tx_ok;

PROCESS_THREAD(tx_process, ev, data)
{
  PROCESS_BEGIN();
    
    data_files[0].fd =  cfs_open("lt-bckp", CFS_READ);
    data_files[0].rs = sizeof(ltdata_t) + sizeof(uint64_t);
    data_files[0].type='B';
    strcpy(data_files[0].name, "lt-bckp");
    data_files[0].backup = backup_none;
    
    data_files[1].fd =  cfs_open("energest-bckp", CFS_READ);
    data_files[1].rs = 40;
    data_files[1].type='Q';
    strcpy(data_files[1].name, "energest-bckp");
    data_files[1].backup = backup_none;

    data_files[2].fd =  cfs_open("lt-data", CFS_READ);
    cfs_read(data_files[2].fd, &start_time, sizeof(uint64_t)); // current time
    start_time = start_time + cycle_duration;                              // for the uip rpl stats
    cfs_close(data_files[2].fd);                              
    
    data_files[2].fd =  cfs_open("lt-data", CFS_READ);
    data_files[2].rs = sizeof(ltdata_t) + sizeof(uint64_t);
    data_files[2].type='D';
    strcpy(data_files[2].name, "lt-data");
    data_files[2].backup = backup_data;
    
    data_files[3].fd =  cfs_open("energest", CFS_READ);
    data_files[3].rs = 40;
    data_files[3].type='G';
    strcpy(data_files[3].name, "energest");
    data_files[3].backup = backup_nrg;
    
    files_sended = 0;
    
    uint8_t pkts_nb = 0;
    if(sense_nb != 0){
      pkts_nb = sense_nb * 2;
      if (data_files[0].fd != -1)
	pkts_nb = pkts_nb + sense_nb;
      if (data_files[1].fd != -1)
	pkts_nb = pkts_nb + sense_nb;
#if defined(UIP_STATS) || defined(RPL_STATS)
      pkts_nb = pkts_nb + 1;
#endif
    }
    else {
      pkts_nb = 2;
      if (data_files[0].fd != -1)
	pkts_nb = pkts_nb + 1;
      if (data_files[1].fd != -1)
	pkts_nb = pkts_nb + 1;
#if defined(UIP_STATS) || defined(RPL_STATS)
      pkts_nb = pkts_nb + 1;
#endif
    }
    max_int_tx = tx_time_slot / pkts_nb;
      
    LOG_INFO("max int %d\n",  max_int_tx);
    
    start_of_tx = clock_time();
    
    etimer_set(&tx_timer, tx_time_slot * CLOCK_SECOND);
    NETSTACK_MAC.on();
    
    simple_udp_register(&udp_conn, UDP_CLIENT_PORT, NULL,
			UDP_SERVER_PORT,tx_callback);
    
    
    while(!files_sended){
      leds_off(LEDS_ALL);
      if(NETSTACK_ROUTING.node_is_reachable() &&
	 NETSTACK_ROUTING.get_root_ipaddr(&dest_ipaddr)) {
	
	memset(&send_data, 0, RECORD_DATA_SIZE);
	
	uint64_t netuse = clock_time() - start_of_tx;
	netuse /= CLOCK_SECOND;
	memcpy(&send_data[1],&netuse, sizeof(uint64_t));
	start_of_tx = clock_time();
	      
	random_init((uint16_t)start_of_tx);
	
	i_file = 0;
	for (i_file = 0; i_file < 4; i_file++){
	  if (data_files[i_file].fd != -1){	    
	    file_read = cfs_read(data_files[i_file].fd, &send_data[1 + sizeof(uint64_t)], data_files[i_file].rs);
	    
	    
	    while(file_read > 1){     
	      send_data[0] = data_files[i_file].type;
	      msg_acked = 0;

	      etimer_set(&tx_guard_timer, random_rand() % (max_int_tx * CLOCK_SECOND));
	      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&tx_guard_timer));
	      
	      simple_udp_sendto(&udp_conn,  send_data,  1 + sizeof(uint64_t) + data_files[i_file].rs, &dest_ipaddr);
	      etimer_set(&ack_timer, CLOCK_SECOND/4);
	      while(!msg_acked){
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&ack_timer) || etimer_expired(&tx_timer));
		if (etimer_expired(&tx_timer)) {
		  etimer_stop(&ack_timer);
		  backup();
		  tx_ok = 0;
		  process_post(&node_process,tx_event, &tx_ok);
		  PROCESS_EXIT();
		}
		if (etimer_expired(&ack_timer))
		  etimer_restart(&ack_timer);
	      }
	      etimer_stop(&ack_timer);
	      memset(&send_data, 0, RECORD_DATA_SIZE);
	      file_read = cfs_read(data_files[i_file].fd, &send_data[1 + sizeof(uint64_t)],  data_files[i_file].rs);
	    }
	    cfs_close(data_files[i_file].fd);
	    cfs_remove(data_files[i_file].name);
	    data_files[i_file].fd = -1;
	  }
	}
	files_sended=1;
#if defined(UIP_STATS) || defined(RPL_STATS)
	memset(send_data, 0, RECORD_DATA_SIZE);
	send_data[0] = 'S';
	memcpy(&send_data[1], &start_time, sizeof(uint64_t));
	ltstatsdata_t s;
	ltdata_read_stats_data(&s);
	memcpy(&send_data[1 + sizeof(uint64_t)], &s, sizeof(ltstatsdata_t));
	etimer_set(&ack_timer, CLOCK_SECOND);
	msg_acked = 0;
	while(!msg_acked){
	  simple_udp_sendto(&udp_conn,  send_data,  1 + sizeof(uint64_t) + sizeof(ltstatsdata_t), &dest_ipaddr);
	  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&ack_timer));
	  if (etimer_expired(&ack_timer))
	    etimer_restart(&ack_timer);
	}
	etimer_stop(&ack_timer);
#endif	
      }
      else{
	etimer_set(&onesec_timer, CLOCK_SECOND);
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&onesec_timer)|| etimer_expired(&tx_timer));
	if (etimer_expired(&tx_timer)){
	  etimer_stop(&onesec_timer);
	  backup();
	  tx_ok = 0;
	  process_post(&node_process,tx_event, &tx_ok);
	  PROCESS_EXIT();
	}
	leds_on(LEDS_RED);
	etimer_set(&onesec_timer, CLOCK_SECOND/4);
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&onesec_timer));
	leds_off(LEDS_ALL);
	etimer_set(&onesec_timer, CLOCK_SECOND/4);
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&onesec_timer));
      }
    }
    etimer_stop(&onesec_timer);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&tx_timer));
    tx_ok = 1;
    reset_conf();
    process_post(&node_process,tx_event, &tx_ok);
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
static process_event_t rdv_event;
static uint8_t rdv_ok;

PROCESS_THREAD(rdv_process, ev, data)
{
  PROCESS_BEGIN();
  
  start_of_rdv = clock_time();
  etimer_set(&rdv_timer, rdv_time_slot * CLOCK_SECOND);
  rdv_guard_time = rdv_time_slot - (rdv_time_slot / 4);
  NETSTACK_MAC.on();
  simple_udp_register(&udp_conn, UDP_CLIENT_PORT, NULL,
		      UDP_SERVER_PORT,rdv_callback);

  rdv = 0;
  while(!rdv){
    leds_off(LEDS_ALL);
    if(NETSTACK_ROUTING.node_is_reachable() &&
       NETSTACK_ROUTING.get_root_ipaddr(&dest_ipaddr)) {
      uint64_t ct = clock_time();
      random_init((uint16_t)ct);
      LOG_INFO("start networking at %llu with %u\n", ct, (uint16_t)ct);
      rdv_req[0] = 'R';
      rdv_timestp_req = ct;
      memcpy(&rdv_req[1], &rdv_timestp_req, sizeof(uint64_t));
      etimer_set(&rdv_guard_timer, random_rand() % (rdv_guard_time * CLOCK_SECOND));
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&rdv_guard_timer));
      etimer_set(&ack_timer, 2 * CLOCK_SECOND);
      simple_udp_sendto(&udp_conn,  rdv_req,  sizeof(uint8_t) + sizeof(uint64_t), &dest_ipaddr);
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&ack_timer) || etimer_expired(&rdv_timer));
      if (etimer_expired(&rdv_timer)) {
	 etimer_stop(&ack_timer);
	 etimer_stop(&onesec_timer);
	 rdv_ok = 0;
	 process_post(&node_process, rdv_event, &rdv_ok);
	 PROCESS_EXIT();
      }	
      if (etimer_expired(&ack_timer))
	etimer_restart(&ack_timer);
    }
    else{
      LOG_INFO("No Network\n");
      etimer_set(&onesec_timer, CLOCK_SECOND);
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&onesec_timer) || etimer_expired(&rdv_timer));
      if (data == &onesec_timer) {
	leds_on(LEDS_BLUE);
	etimer_restart(&onesec_timer);
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&onesec_timer)) ;
      }
      else if (data == &rdv_timer) {
	rdv_ok = 0;
	 etimer_stop(&ack_timer);
	 etimer_stop(&onesec_timer);
	 process_post(&node_process, rdv_event, &rdv_ok);
	 PROCESS_EXIT();
      }
    }
  }
  leds_off(LEDS_ALL);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&rdv_timer));
  rdv = 0;
  rdv_ok = 1;
  etimer_stop(&ack_timer);
  etimer_stop(&onesec_timer);
  etimer_stop(&rdv_timer);
  write_conf();
  process_post(&node_process, rdv_event, &rdv_ok);
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/

#define BOOT 0
#define SENSING 1
#define RDV 2
#define TX 3
#define SLEEP 4

PROCESS_THREAD(node_process, ev, data)
{
  
  PROCESS_BEGIN();
  LOG_INFO("START\n");
  pm_enable();
  tx_event = process_alloc_event();
  rdv_event = process_alloc_event();
  tx_ok = 0;
  rdv_ok = 0;
  
  /****************************************************************/

  if (read_conf()){
    if (its_time_to_send()){
      // while(!tx_ok){
	process_start(&tx_process, NULL);
	PROCESS_WAIT_EVENT_UNTIL(ev == tx_event);
	//tx_ok = *((uint8_t*)data);
	//}
      
      leds_on(LEDS_BLUE);
      etimer_set(&onesec_timer, CLOCK_SECOND/2);
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&onesec_timer));
      leds_off(LEDS_ALL);
      
      while(!rdv_ok){
	process_start(&rdv_process, NULL);
	PROCESS_WAIT_EVENT_UNTIL(ev == rdv_event);
	rdv_ok = *((uint8_t *)data);
      }
      
      leds_on(LEDS_GREEN);
      etimer_set(&onesec_timer, CLOCK_SECOND/2);
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&onesec_timer));
      leds_off(LEDS_ALL);
      
      sense_and_sleep();
  }
    else {
      sense_and_sleep();
    }
  }
  else {
    while(!rdv_ok){
      process_start(&rdv_process, NULL);
      PROCESS_WAIT_EVENT_UNTIL(ev == rdv_event);
      rdv_ok = *((uint8_t *)data);
    }
    
    leds_on(LEDS_GREEN);
    etimer_set(&onesec_timer, CLOCK_SECOND/2);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&onesec_timer));
    leds_off(LEDS_ALL);
    
    sense_and_sleep();
  }
  /****************************************************************/
  PROCESS_END();
}
