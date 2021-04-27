#!/bin/bash

if [ $# -eq 0 ]
then
    echo ./upload sht25 \| dht22 \| bmp180 \| dls \|
    exit 1
fi

case $1 in
    sht25)
	make SENSOR=sht25 POWERMGMT=1 UIPSTATS=1 RPLSTATS=1 lt-sensor.upload
	;;    
    dht22)
	make SENSOR=dht22 POWERMGMT=1 UIPSTATS=1 RPLSTATS=1 lt-sensor.upload
	;;
    bmp180)
	make SENSOR=bmp180 POWERMGMT=1 UIPSTATS=1 RPLSTATS=1 lt-sensor.upload
	;;
    ldr)
	make SENSOR=ldr POWERMGMT=1 UIPSTATS=1 RPLSTATS=1 lt-sensor.upload
	;;
    dls)
	make SENSOR=dls POWERMGMT=1 UIPSTATS=1 RPLSTATS=1 lt-sensor.upload
	;;
    sink)
	make  POWERMGMT=1 UIPSTATS=1 RPLSTATS=1 lt-sink.upload
	;;
    *)
	echo not understant $1
	;;
esac
    
    
    
