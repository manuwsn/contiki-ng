#!/bin/bash

i=0
time=0
volt=0
for v in $(grep "$1" $2 | cut -d ' ' -f4-15)
do
    case $i in
	[0-7])
	    time=$(($time + ($v << ($i * 8)) ))
	    ;;
	8|9|1[0-1])
	    volt=$(($volt + ($v << (($i - 8) * 8)) ))
	    ;;
    esac
    i=$(((i+1)%12))
    if [ $i -eq 0 ]
    then
	echo $(date -d @$time +%D:%H:%M:%S) $volt
	time=0
	volt=0
    fi
done
