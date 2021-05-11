#!/bin/bash

i=0
time=( "0" "0" "0" "0" "0" "0" "0" "0" )
hum=( "0" "0" )
for v in $(grep "$1" tmp | cut -d ' ' -f4-21)
do
    case $i in
	0)
	    time[7]=$v
	    ;;
	[1-7])
	    j=$((7-$i))
	    time[$j]=$v
	    ;;
	16)
	    hum[1]=$v
	    ;;
	17)
	    hum[0]=$v
	    ;;
	
    esac
    i=$(((i+1)%18))
    if [ $i -eq 0 ]
    then
	echo  -n $((  ((( ${time[7]} + ${time[6]}*256 ) + ${time[5]} * 256*256) + ${time[4]} * 256*256*256) )) " "
	echo $(( ${hum[1]} + (${hum[0]}*256) ))
    fi
done
