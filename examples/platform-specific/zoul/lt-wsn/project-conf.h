
#ifndef __PROJECT_CONF_H__
#define __PROJECT_CONF_H__

/*******************************************************/
/******************* Configure coffee ******************/
/*******************************************************/


#if CONTIKI_TARGET_CC2538DK || CONTIKI_TARGET_OPENMOTE_CC2538 || \
    CONTIKI_TARGET_ZOUL

#define COFFEE_CONF_SIZE              (CC2538_DEV_FLASH_SIZE / 2)
#define COFFEE_CONF_MICRO_LOGS        1
#define COFFEE_CONF_APPEND_ONLY       0
#define DATAFILE "lt-data"

/*******************************************************/
/******************* Configure cycles ******************/
/*******************************************************/

#ifdef CYCLE_CONF_DURATION
#define CYCLE_DURATION CYCLE_CONF_DURATION
#else
#define CYCLE_DURATION 300
#endif

#ifdef SLEEP_CONF_FREQUENCY
#define SLEEP_FREQUENCY SLEEP_CONF_FREQUENCY
#else
#define SLEEP_FREQUENCY 2
#endif

#ifdef NETWORKING_CONF_DURATION
#define NETWORKING_DURATION NETWORKING_CONF_DURATION
#else 
#define NETWORKING_DURATION (120 * CLOCK_SECOND)
#endif

#define SLEEP_DURATION (CYCLE_DURATION / SLEEP_FREQUENCY)

 

#define DATE_SECONDS DATE_CONF_SECONDS

/*******************************************************/
/******************* Configure rtcc  *******************/
/*******************************************************/



#define RTC_CONF_INIT           1
#define RTC_CONF_SET_FROM_SYS   1


#endif /* CONTIKI_TARGET_CC2538DK || CONTIKI_TARGET_ZOUL */



/* Set to enable TSCH security */
#ifndef WITH_SECURITY
#define WITH_SECURITY 0
#endif /* WITH_SECURITY */


/* USB serial takes space, free more space elsewhere */
#define SICSLOWPAN_CONF_FRAG 0
#define UIP_CONF_BUFFER_SIZE 160

/*******************************************************/
/******************* Configure TSCH ********************/
/*******************************************************/

/* IEEE802.15.4 PANID */
#define IEEE802154_CONF_PANID 0x81a5

/* Do not start TSCH at init, wait for NETSTACK_MAC.on() */
#define TSCH_CONF_AUTOSTART 0

/* 6TiSCH minimal schedule length.
 * Larger values result in less frequent active slots: reduces capacity and saves energy. */
#define TSCH_SCHEDULE_CONF_DEFAULT_LENGTH 3

#define TSCH_CONF_EB_PERIOD 10
#define TSCH_CONF_MAX_EB_PERIOD 20

#if WITH_SECURITY

/* Enable security */
#define LLSEC802154_CONF_ENABLED 1

#endif /* WITH_SECURITY */

/*******************************************************/
/************* Other system configuration **************/
/*******************************************************/

/* Logging */
#define LOG_CONF_LEVEL_RPL                         LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_TCPIP                       LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_IPV6                        LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_6LOWPAN                     LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_MAC                         LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_FRAMER                      LOG_LEVEL_WARN
#define TSCH_LOG_CONF_PER_SLOT                     0

#endif /* __PROJECT_CONF_H__ */
