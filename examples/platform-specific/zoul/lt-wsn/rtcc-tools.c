#include "rtcc.h"

static uint8_t
check_leap_year(uint8_t val)
{
  return ((val % 4 == 0) && (val % 100 != 0)) || (val % 400 == 0);
}
int8_t rtcc_sec_to_date(simple_td_map *data, uint64_t time){

  memset(data,0,sizeof(simple_td_map));
  data->weekdays = 3; // Thuesday : first day of 1/1/70
  data->years = 70; // first year of unix date
  data->century = 19;
  /* time is in seconds, compute the seconds after the last minute */ 
  data->seconds = time % 60;
  // adjust time to a multiple of minutes
  time -= data->seconds;
  // convert time in minutes
  time /= 60;
  // then compute minutes after the last hour
  data->minutes = time % 60;
  //adjust time to a multiple of hours
  time -= data->minutes;
  // convert time in hours
  time /= 60;
  // then compute hours after the last day
  data->hours = time % 24;
  // adjust time to a multiple of days
  time -= data->hours;
  // convert time in days
  time /= 24;
  // week days are regular
  data->weekdays += time % 7;
  
  // Compute leap and non leap years
  uint64_t last_year = 0;
  uint64_t iterator_time = 0;

  while(iterator_time <= time){    
    if (check_leap_year(data->years))
      iterator_time += 366;
    else
      iterator_time += 365;
    
    if(iterator_time <= time)
      data->years++;
    else {
    if (check_leap_year(data->years))
      last_year = iterator_time - 366;
    else
      last_year = iterator_time - 365;
    }
  }
  // Adjust to remaining number of days
  // in the lat year
  time = time - last_year;
  iterator_time = 0;
  int last_day;
  while(iterator_time <= time){
    last_day = iterator_time;
    if(data->months == 0 || data->months == 2 ||
       data->months == 4 || data->months == 6 ||
       data->months == 7 || data->months == 9 ||
       data->months == 11)
      iterator_time += 31;
    else {
      if (data->months == 1){
	if  (check_leap_year(data->years))
	  iterator_time += 29;
	  else
	    iterator_time += 28;
      } else
	iterator_time += 30;
    }
    if (iterator_time < time)
      data->months++;
    else
      data->day = time - last_day;
  }
  data->century = RTCC_CENTURY_19XX_21XX;
  data->mode = RTCC_24H_MODE;
  return AB08_SUCCESS;
}
