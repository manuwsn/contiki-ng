
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
/******************* Configure Multicast ***************/
/*******************************************************/

#ifndef UIP_MCAST6_CONF_ENGINE
#define UIP_MCAST6_CONF_ENGINE UIP_MCAST6_ENGINE_MPL
#endif

#define UIP_MCAST6_ROUTE_CONF_ROUTES 1

#define LPM_CONF_MAX_PM 1

/*******************************************************/
/******************* Configure cycles ******************/
/*******************************************************/           

#ifdef DEFAULT_RDV_CONF_TIME_SLOT
#define DEFAULT_RDV_TIME_SLOT DEFAULT_RDV_CONF_TIME_SLOT
#else
#define DEFAULT_RDV_TIME_SLOT 30
#endif

#define RETRY_RDV_TIME 10
#define MAX_ATTEMPT_RDV 300   


#ifdef NETWORKING_CONF_TIME_SLOT
#define NETWORKING_TIME_SLOT NETWORKING_CONF_TIME_SLOT
#else
#define NETWORKING_TIME_SLOT 30
#endif

#ifdef DEEP_CONF_MAX
#define DEEP_MAX DEEP_CONF_MAX
#else
#define DEEP_MAX 1
#endif


#ifdef CYCLE_CONF_DURATION
#define CYCLE_DURATION CYCLE_CONF_DURATION
#else
#define CYCLE_DURATION 60
#endif

#ifdef SLEEP_CONF_FREQUENCY
#define SLEEP_FREQUENCY SLEEP_CONF_FREQUENCY
#else
#define SLEEP_FREQUENCY 6
#endif

#define CURRENT_DATE CURRENT_CONF_DATE


/*******************************************************/
/******************* Configure rtcc  *******************/
/*******************************************************/



#define RTC_CONF_INIT           1
#define RTC_CONF_SET_FROM_SYS   1


#endif /* CONTIKI_TARGET_CC2538DK || CONTIKI_TARGET_ZOUL */


/*******************************************************/
/******************* Configure sensors *****************/
/*******************************************************/
#define ENERGEST_CONF_ON 1

#ifdef SENSOR_CONF_BMP180
#define SENSOR_BMP180 SENSOR_CONF_BMP180
#endif
#ifdef SENSOR_CONF_SHT25
#define SENSOR_SHT25 SENSOR_CONF_SHT25
#endif
#ifdef SENSOR_CONF_DHT22
#define SENSOR_DHT22 SENSOR_CONF_DHT22
#endif
#ifdef SENSOR_CONF_LDR
#define SENSOR_LDR SENSOR_CONF_LDR
#endif
#ifdef SENSOR_CONF_DLS
#define SENSOR_DLS SENSOR_CONF_DLS
#endif
#ifdef NO_SENSOR_CONF
#define NO_SENSOR NO_SENSOR_CONF
#endif

#ifdef RPL_CONF_STATS
#define RPL_STATS RPL_CONF_STATS
#endif

#ifdef UIP_CONF_STATS
#define UIP_STATS UIP_CONF_STATS
#define UIP_CONF_STATISTICS UIP_CONF_STATS
#endif


/* Pin definition for the test-motion example, for the RE-Mote it uses the
 * ADC1 pin
 */
#define MOTION_SENSOR_PORT       GPIO_A_NUM
#define MOTION_SENSOR_PIN        5
#define MOTION_SENSOR_VECTOR     GPIO_A_IRQn

/* Specify the digital light sensor model to use: TSL2561 (default) or TSL2563 */
#define TSL256X_CONF_REF         TSL2561_SENSOR_REF

/* Use the following I2C address for the BME280 sensor (from MikroElektronika) */
#define BME280_CONF_ADDR         0x76



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

#define  TSCH_CONF_DEFAULT_HOPPING_SEQUENCE TSCH_HOPPING_SEQUENCE_16_16

/* Set to enable TSCH security */
#ifndef WITH_SECURITY
#define WITH_SECURITY 0
#endif /* WITH_SECURITY */

#if WITH_SECURITY

/* Enable security */
#define LLSEC802154_CONF_ENABLED 1

#endif /* WITH_SECURITY */

/*******************************************************/
/********************** RPL configuration **************/
/*******************************************************/
#define RPL_CONF_DIO_INTERVAL_MIN 10
#define RPL_CONF_DIO_INTERVAL_DOUBLINGS 4 
#define  RPL_CONF_PROBING_INTERVAL (20 * CLOCK_SECOND)
#define RPL_CONF_DIS_INTERVAL (10 * CLOCK_SECOND)
#define RPL_CONF_DIS_INTERVAL (10 * CLOCK_SECOND)
/*******************************************************/
/************* Other system configuration **************/
/*******************************************************/
#define SIMPLE_ENERGEST_CONF_PERIOD (CLOCK_SECOND * 5)

/* Logging */
#define LOG_CONF_LEVEL_RPL                         LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_TCPIP                       LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_IPV6                        LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_6LOWPAN                     LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_MAC                         LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_FRAMER                      LOG_LEVEL_WARN
#define TSCH_LOG_CONF_PER_SLOT                     0

#endif /* __PROJECT_CONF_H__ */
