#!/bin/bash
i=0
time=0
oldtime=0
cpu=0
lpm=0
tx=0
rx=0
nrg=0
for v in $(grep "$1" $2 | cut -d ' ' -f4-44)
do
    case $i in
	[0-7])
	    time=$(($time + ($v << ($i * 8)) ));;
	8|9|1[0-5])
	    cpu=$(($cpu +   ($v << (($i - 8) * 8)) ));; 
	1[6-9]|2[0-3])
	    lpm=$(($lpm +   ($v << (($i - 16) * 8)) ));;
	2[4-9]|3[0-1])
	    tx=$(($tx +     ($v << (($i - 24) * 8)) ));;
	3[2-9])
	    rx=$(($rx +     ($v << (($i - 32) * 8)) ));;	 
    esac
    i=$(((i+1)%40))
    if [ $i -eq 0 ]
    then
	nrg=$(($nrg + (($cpu * 10 + $tx * 19 + $rx * 18)/32768)))
	if [ $oldtime -lt $time ]
	then
	    if date -d @$time +%D:%H:%M:%S &>/dev/null
	    then
		echo $(date -d @$time +%D:%H:%M:%S) $nrg
		oldtime=$time
	    fi
	fi
	time=0
	cpu=0
	lpm=0
	tx=0
	rx=0
    fi
done
