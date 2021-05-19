#!/bin/bash

i=0
time=0
temp=0
hum=0
for v in $(grep "$1" $2 | cut -d ' ' -f4-21)
do
    case $i in
	[0-7])
	    time=$(($time + ($v << ($i * 8)) ));;
	14|15)
	    temp=$(($temp + ($v << (($i - 14) * 8)) ))
	    ;;
	16|17)
	    hum=$(($hum + ($v << (($i - 16) * 8)) ))
	    ;;
	
    esac
    i=$(((i+1)%18))
    if [ $i -eq 0 ]
    then
	echo $(date -d @$time +%D:%H:%M:%S) $temp $hum
	time=0
	temp=0
	hum=0
    fi
done
