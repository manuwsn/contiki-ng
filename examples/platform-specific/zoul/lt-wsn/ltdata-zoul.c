#include "collect-view.h"
#include "dev/leds.h"
#include "dev/battery-sensor.h"
#include "sys/rtimer.h"
#ifdef CONF_POWERMGMT
#include "power-mgmt.h"
#endif
#include "ltdata-zoul.h"
#include "routing/rpl-lite/rpl.h"
#include "ipv6/uip.h"

#ifdef SENSOR_CONF_BMP180
#include "dev/bmpx8x.h"
#endif  /** SENSOR_CONF_BMP180 **/

#ifdef SENSOR_CONF_PIR
#include "dev/motion-sensor.h"

static int pir_activated = 0;
static int motion_detected = 0;

static void
presence_callback(uint8_t value)
{
  motion_detected = 1;
}
#endif   /** SENSOR_CONF_PIR **/

#ifdef SENSOR_CONF_SHT
#include "dev/sht25.h"
#endif   /** SENSOR_CONF_SHT **/

#ifdef SENSOR_CONF_LDR
#include "dev/adc-sensors.h"
#endif   /** SENSOR_CONF_LDR **/


#define TEST_LEDS_FAIL                    leds_off(LEDS_ALL); \
                                          leds_on(LEDS_RED);
					  
/*---------------------------------------------------------------------------*/
#define PM_EXPECTED_VERSION               0x20

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

/*---------------------------------------------------------------------------*/
void
ltdata_arch_read_sensors(ltdata_t *msg)
{

#ifdef SENSOR_BMP180
  static uint16_t pressure;
  static uint16_t temperature_bmp;

  /* Use Contiki's sensor macro to enable the sensor */
  SENSORS_ACTIVATE(bmpx8x);
  
  pressure = bmpx8x.value(BMPx8x_READ_PRESSURE);
  temperature_bmp = bmpx8x.value(BMPx8x_READ_TEMP);
  
  if((pressure != BMPx8x_ERROR) && (temperature_bmp != BMPx8x_ERROR)) {
    //printf("Pressure = %u(hPa), ", pressure);
    //printf("Temperature = %d (ºC)\n", temperature_bmp);
    
    msg->sensors[TEMPERATURE_SENSOR] = temperature_bmp;
    msg->sensors[PRESSION_SENSOR] = pressure;
    
  }
#endif
  
#ifdef SENSOR_PIR
  if (!pir_activated){
    MOTION_REGISTER_INT(presence_callback);
    SENSORS_ACTIVATE(motion_sensor);
    pir_activated=1;
  }
  if (motion_detected){
    //printf("Motion detected\n");
    msg->sensors[BATTERY_INDICATOR] = 1;
    motion_detected=0;
  } else {
    //printf("no motion\n");
    msg->sensors[BATTERY_INDICATOR] = 0;
  }
#endif
  
#ifdef SENSOR_SHT
  uint16_t temperature, humidity;
  SENSORS_ACTIVATE(sht25);
  if(!sht25.value(SHT25_VOLTAGE_ALARM)) {
    sht25.configure(SHT25_RESOLUTION, SHT2X_RES_14T_12RH);
    temperature = sht25.value(SHT25_VAL_TEMP);
    //printf("Temperature %d ºC, ", temperature);
    humidity = sht25.value(SHT25_VAL_HUM);
    //printf("Humidity %d RH\n", humidity);
    
    msg->sensors[TEMPERATURE_SENSOR] = temperature;
    msg->sensors[HUMIDITY_SENSOR] = humidity;
  }  
  SENSORS_DEACTIVATE(sht25);
#endif
  
#ifdef SENSOR_CONF_LDR
  uint16_t ldr;
  adc_sensors.configure(ANALOG_GROVE_LIGHT, 5);
  ldr = adc_sensors.value(ANALOG_GROVE_LIGHT);
  if(ldr != ADC_WRAPPER_ERROR) {
    //printf("LDR (resistor) = %u\n", ldr);
    msg->sensors[LIGHT2_SENSOR] = ldr;
  }
  /*
else {
      printf("Error, enable the DEBUG flag in adc-wrapper.c for info\n");
    }
  */
#endif
  
#ifdef RPL_STATS
  msg->rpl_stats[SEND_DIO] = rpl_stats.send_dios;
  msg->rpl_stats[RCV_DIO] = rpl_stats.received_dios;
  msg->rpl_stats[SEND_DAO] = rpl_stats.send_daos;
  msg->rpl_stats[RCV_DAO] = rpl_stats.received_daos;
  msg->rpl_stats[SEND_DIS] = rpl_stats.send_diss;
  msg->rpl_stats[RCV_DIS] = rpl_stats.received_diss;
  msg->rpl_stats[SEND_ACKDAO] = rpl_stats.send_ackdaos;
  msg->rpl_stats[RCV_ACKDAO] = rpl_stats.received_ackdaos;
#endif
  
#ifdef UIP_STATS
  msg->uip_stats[SEND_IP] = (uint16_t) uip_stAat.ip.sent;
  msg->uip_stats[RCV_IP] =  (uint16_t) uip_stat.ip.recv;
  msg->uip_stats[FWD_IP] = (uint16_t)  uip_stat.ip.forwarded;
  msg->uip_stats[DROP_IP] = (uint16_t) uip_stat.ip.drop;
#endif

}
/*---------------------------------------------------------------------------*/
