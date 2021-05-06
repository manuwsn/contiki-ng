#include "dev/leds.h"
#include "dev/battery-sensor.h"
#include "sys/rtimer.h"
#include "power-mgmt.h"
#include "lt-data.h"
#include "lt-data-zoul.h"
#include "routing/rpl-lite/rpl.h"
#include "ipv6/uip.h"
#include "dev/i2c.h"

#ifdef SENSOR_BMP180
#include "dev/bmpx8x.h"
#endif  /** SENSOR_BMP180 **/

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

#ifdef SENSOR_SHT25
#include "dev/sht25.h"
#endif   /** SENSOR_CONF_SHT25 **/

#ifdef SENSOR_DHT22
#include "dev/dht22.h"
#endif   /** SENSOR_CONF_DHT22 **/

#ifdef SENSOR_LDR
#include "dev/adc-sensors.h"
#endif   /** SENSOR_CONF_LDR **/

#ifdef SENSOR_DLS
#include "dev/tsl256x.h"
#endif   /** SENSOR_CONF_DLS **/


#define TEST_LEDS_FAIL                    leds_off(LEDS_ALL); \
                                          leds_on(LEDS_RED);
					  
/*---------------------------------------------------------------------------*/
#define PM_EXPECTED_VERSION               0x20

static uint16_t get_voltage() {
  
  static uint8_t aux;
  static uint16_t voltage = 0;
  
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
  
  clock_delay_usec(10000);
  
  /* Read the battery voltage level */
  
  if(pm_get_voltage(&voltage) != PM_SUCCESS) {
    printf("PM: error retrieving voltage\n");
    return 0;
  }
  
  return voltage;
}

/*---------------------------------------------------------------------------*/
void
ltdata_arch_read_sensors(ltdata_t *msg)
{
#ifdef SENSOR_BMP180
  
   uint16_t pressure;
   uint16_t temperature_bmp;

  /* Use Contiki's sensor macro to enable the sensor */
  
  SENSORS_ACTIVATE(bmpx8x);
  
  pressure = bmpx8x.value(BMPx8x_READ_PRESSURE);
  temperature_bmp = bmpx8x.value(BMPx8x_READ_TEMP);
  
  if((pressure != BMPx8x_ERROR) && (temperature_bmp != BMPx8x_ERROR)) {
    printf("Pressure = %u(hPa), ", pressure);
    printf("Temperature = %d (ºC)\n", temperature_bmp);
    
    msg->sensors[SENSOR_TYPE] = BMP180;
    msg->sensors[TEMPERATURE_SENSOR] = temperature_bmp;
    msg->sensors[PRESSION_SENSOR] = pressure;
    
  }
  else printf("Erreur bmp180\n");
  SENSORS_DEACTIVATE(bmpx8x);
  
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
  
#ifdef SENSOR_SHT25
  uint16_t temperature, humidity;
  SENSORS_ACTIVATE(sht25);
  if(!sht25.value(SHT25_VOLTAGE_ALARM)) {
    sht25.configure(SHT25_RESOLUTION, SHT2X_RES_14T_12RH);
    clock_delay_usec(1000);
    temperature = sht25.value(SHT25_VAL_TEMP);
    printf("Temperature %d ºC, ", temperature);
    humidity = sht25.value(SHT25_VAL_HUM);
    printf("Humidity %d RH\n", humidity);
    
    msg->sensors[SENSOR_TYPE] = SHT25;
    msg->sensors[TEMPERATURE_SENSOR] = temperature;
    msg->sensors[HUMIDITY_SENSOR] = humidity;
  }  
  SENSORS_DEACTIVATE(sht25);
#endif

#ifdef SENSOR_DHT22
  int16_t temperature22, humidity22;
  SENSORS_ACTIVATE(dht22);
  //clock_delay_usec(1000);
  if(dht22_read_all(&temperature22, &humidity22) != DHT22_ERROR) {
    printf("Temperature %02d.%02d ºC, ", temperature22 / 10, temperature22 % 10);
    printf("Humidity %02d.%02d RH\n", humidity22 / 10, humidity22 % 10);
    msg->sensors[SENSOR_TYPE] = DHT22;
    msg->sensors[TEMPERATURE_SENSOR] = temperature22;
    msg->sensors[HUMIDITY_SENSOR] = humidity22;
  } else {	  
    printf("Failed to read the sensor\n");
  }
  SENSORS_DEACTIVATE(dht22);
#endif
  
#ifdef SENSOR_LDR
  uint16_t ldr;
  adc_sensors.configure(ANALOG_GROVE_LIGHT, 5);
  ldr = adc_sensors.value(ANALOG_GROVE_LIGHT);
  if(ldr != ADC_WRAPPER_ERROR) {
    //printf("LDR (resistor) = %u\n", ldr);
    msg->sensors[SENSOR_TYPE] = LDR;
    msg->sensors[LIGHT_SENSOR] = ldr;
  }
  /*
else {
      printf("Error, enable the DEBUG flag in adc-wrapper.c for info\n");
    }
  */
#endif
  
#ifdef SENSOR_DLS
  uint16_t light;
  SENSORS_ACTIVATE(tsl256x);
  tsl256x.configure(TSL256X_INT_OVER, 0x15B8);
  light = tsl256x.value(TSL256X_VAL_READ);
  if(light != TSL256X_ERROR) {
    printf("DLS = %u\n", light);
    msg->sensors[SENSOR_TYPE] = DLS;
    msg->sensors[LIGHT_SENSOR] = light;
    
    } else {
      printf("Error, enable the DEBUG flag in the tsl256x driver for info, ");
      printf("or check if the sensor is properly connected\n");
  }

#endif
 
    msg->voltage = get_voltage();


}
/*---------------------------------------------------------------------------*/
