#!/bin/bash

i=0
time=0
temp=0
hum=0
for v in $(grep "$1" $2 | cut -d ' ' -f13-30)
do
    case $i in
	[0-6])
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
	if [ $temp -ne 0 ]
	then
	    if date -d @$time +%D:%H:%M:%S &>/dev/null
	    then
		echo $(date -d @$time +%D:%H:%M:%S) $temp $hum
	    fi
	fi
	time=0
	temp=0
	hum=0
    fi
done
