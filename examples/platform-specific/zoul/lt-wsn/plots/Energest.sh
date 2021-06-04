#!/bin/bash
i=0
time=0
oldtime=0
cpu=0
lpm=0
tx=0
rx=0
nrg=0
C=$((600))
ecu=0
prc=0
rm $$.nrg.tmp &> /dev/null

for v in $(grep "$1" $2 | cut -d ' ' -f13-53)
do
    case $i in
	[0-7])
	    time=$(($time + ($v << ($i * 8)) ));;
	8|9|1[0-1])
	    cpu=$(($cpu +   ($v << (($i - 8) * 8)) ));; 
	1[6-9])
	    lpm=$(($lpm +   ($v << (($i - 16) * 8)) ));;
	2[4-9]|3[0-1])
	    tx=$(($tx +     ($v << (($i - 24) * 8)) ));;
	3[2-9])
	    rx=$(($rx +     ($v << (($i - 32) * 8)) ));;	 
    esac
    i=$((($i+1)%40))
    if [ $i -eq 0 ]
    then
	#echo $cpu $lpm $tx $rx
	c=$(echo "($cpu * 20 + $tx * 40 + $rx * 19)/(32768*3600)" | bc -l)
	tps=$(echo "($cpu+$lpm)/(32768*3600)" | bc -l)
	#echo $tps $c
	d=$(echo "$C/$c" | bc -l)
	prc=$(echo "$prc + $tps*100/$d" | bc -l)
	
	ecu=$(echo "100-$prc" | bc -l)
	
		echo $(date -d @$time +%D:%H:%M:%S) $ecu >> $$.nrg.tmp
	#	echo $time $ecu >> $$.nrg.tmp
	time=0
	cpu=0
	lpm=0
	tx=0
	rx=0
	
    fi
done

cat $$.nrg.tmp | sort -u

rm $$.nrg.tmp
