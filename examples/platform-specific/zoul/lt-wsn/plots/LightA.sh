#!/bin/bash

i=0
time=0
light=0
for v in $(grep "$1" $2 | cut -d ' ' -f4-19)
do
    echo $v
    case $i in
	[0-7])
	    time=$(($time + ($v << ($i * 8)) ))
	    ;;
	14|15)
	    light=$(($light + ($v << (($i - 14) * 8)) ))
	    ;;
    esac
    i=$(((i+1)%16))
    if [ $i -eq 0 ]
    then
	echo $(date -d @$time +%D:%H:%M:%S) $light
	time=0
	light=0
    fi
done
