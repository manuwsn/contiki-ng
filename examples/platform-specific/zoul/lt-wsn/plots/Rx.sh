#!/bin/bash

i=0
rx=0
time=0
dating=$3

for v in $(grep -E "$1" $2 | cut -d ' ' -f5-20)
do
    case $i in
	[0-6])
	    rx=$(($rx + ($v << ($i * 8)) ))
	    ;;
	8|9|1[0-5])
	    time=$(($time + ($v << (($i-8) * 8)) ))
	    ;;
    esac
    
    i=$(((i+1)%16))
    if [ $i -eq 0 ]
    then
	if [ $rx -ne 0 ]
	then
	    if [ $dating ]
	    then
		echo $(date -d @$time +%D:%H:%M:%S) $rx
	    else
		echo $time $rx
	    fi
	fi
	rx=0
	time=0
    fi
done
