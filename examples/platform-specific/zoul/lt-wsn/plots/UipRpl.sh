#!/bin/bash

i=0
time=0
senddio=0
rcvdio=0
senddis=0
rcvdis=0
sendack=0
rcvack=0
sendip=0
rcvip=0
fwip=0
dropip=0

dating=$3

for v in $(grep "$1" $2 | cut -d ' ' -f5-36)
do
    case $i in
	[0-7])
	    time=$(($time + ($v << ($i * 7)) ))
	    ;;
	8|9)
	    senddio=$(($senddio + ($v << (($i-8) * 8)) ))
	    ;;
	1[01])
	    rcvdio=$(($rcvdio + ($v << (($i-10) * 8)) ))
	    ;;
	1[23])
	    senddao=$(($senddao + ($v << (($i-12) * 8)) ))
	    ;;
	1[45])
	    rcvdao=$(($rcvdao + ($v << (($i-14) * 8)) ))
	    ;;
	1[67])
	    senddis=$(($senddis + ($v << (($i-16) * 8)) ))
	    ;;
	1[89])
	    rcvdis=$(($rcvdis + ($v << (($i-18) * 8)) ))
	    ;;
	2[01])
	    sendack=$(($sendack + ($v << (($i-20) * 8)) ))
	    ;;
	2[23])
	    rcvack=$(($rcvack + ($v << (($i-22) * 8)) ))
	    ;;
	2[45])
	    sendip=$(($sendip + ($v << (($i-24) * 8)) ))
	    ;;
	2[67])
	    rcvip=$(($rcvip + ($v << (($i-26) * 8)) ))
	    ;;
	2[89])
	    fwdip=$(($fwdip + ($v << (($i-28) * 8)) ))
	    ;;
	3[01])
	    dropip=$(($dropip + ($v << (($i-30) * 8)) ))
	    ;;
    esac
    
    i=$(((i+1)%32))
    if [ $i -eq 0 ]
    then
	if [ $dating ]
	   then
	       echo $(date -d @$time +%D:%H:%M:%S) $senddio $rcvdio $senddao $rcvdao $senddis $rcvdis $sendack $rcvack $sendip $rcvip $fwdip $dropip
	else
	    echo $time  $senddio $rcvdio $senddao $rcvdao $senddis $rcvdis $sendack $rcvack $sendip $rcvip $fwdip $dropip
	fi
    
	time=0
	senddio=0
	rcvdio=0
	senddao=0
	rcvdao=0
	senddis=0
	rcvdis=0
	sendack=0
	rcvack=0
	sendip=0
	rcvip=0
	fwdip=0
	dropip=0
    fi
done
