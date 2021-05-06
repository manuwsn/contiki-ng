#!/bin/bash

nb=$(cat tmp| wc -l)
nb=$(($nb - 16))
i=0
time=( "0" "0" "0" "0" "0" "0" "0" "0" )
volt=( "0" "0" "0" "0" )
for v in $(tail -n $nb tmp | grep $1 | cut -d ' ' -f4-15)
do
    case $i in
	0)
	    time[7]=$v;;
	[1-7])
	    j=$((7-$i))
	    time[$j]=$v;;
	8)
	    volt[3]=$v;;
	9|1[0-2])
	    j=$((11-$i))
	    volt[$j]=$v;;
	
    esac
    i=$(((i+1)%12))
    if [ $i -eq 0 ]
    then
	echo  -n $((  ((( ${time[7]} + ${time[6]}*256 ) + ${time[5]} * 256*256) + ${time[4]} * 256*256*256) )) " "
	echo $((  ((( ${volt[3]} + ${volt[2]}*256 ) + ${volt[1]} * 256*256) + ${volt[0]} * 256*256*256) ))
    fi
done
