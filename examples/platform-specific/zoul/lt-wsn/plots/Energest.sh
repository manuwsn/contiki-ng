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

for v in $(grep "$1" $2 | cut -d ' ' -f13-53)
do
    case $i in
	[0-6])
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
	echo $time $cpu $lpm $tx $rx >> $$.nrg.tmp.1
	time=0
	cpu=0
	lpm=0
	tx=0
	rx=0
    fi
done

sort -u $$.nrg.tmp.1 > $$.nrg.tmp.sorted
rm  $$.nrg.tmp.1
i=0
for v in $(cat $$.nrg.tmp.sorted)
do
    case $i in
	0) time=$v;;
	1) cpu=$v;;
	2) lpm=$v;;
	3) tx=$v;;
	4) rx=$v;;
    esac
    i=$((($i+1)%5))
    if [ $i -eq 0 ]
    then
	#echo $time $cpu $lpm $tx $rx
	c=$(echo "($cpu * 20 + $tx * 40 + $rx * 19)/(32768*3600)" | bc -l)
	tps=$(echo "($cpu+$lpm)/(32768*3600)" | bc -l)
	#echo $tps $c
	d=$(echo "$C/$c" | bc -l)
	prc=$(echo "$prc + $tps*100/$d" | bc -l)
	
	ecu=$(echo "100-$prc" | bc -l)
	
	echo $(date -d @$time +%D:%H:%M:%S) $ecu >> $$.nrg.tmp
    fi
done
cat $$.nrg.tmp
rm $$.nrg.tmp*
