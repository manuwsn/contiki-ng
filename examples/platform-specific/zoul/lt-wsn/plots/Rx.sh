#!/bin/bash

i=0
tx=0
time=0

for v in $(grep "$1" $2 | cut -d ' ' -f5-20)
do
    case $i in
	[0-7])
	    tx=$(($tx + ($v << ($i * 8)) ))
	    ;;
	8|9|1[0-5])
	    time=$(($time + ($v << ($i * 8)) ))
	    ;;
    esac
    
    i=$(((i+1)%16))
    if [ $i -eq 0 ]
    then
	if [ $tx -ne 0 ]
	   then
	       echo $(date -d @$time +%D:%H:%M:%S) $tx
	fi
	tx=0
	time=0
    fi
done
