#!/bin/bash

i=0
time=0
volt=0

rm $$.vlt.tmp &> /dev/null

for v in $(grep "$1" $2 | cut -d ' ' -f13-24)
do
    case $i in
	[0-6])
	    time=$(($time + ($v << ($i * 8)) ))
	    ;;
	8|9|1[01])
	    volt=$(($volt + ($v << (($i - 8) * 8)) ))
	    ;;
    esac
    i=$(((i+1)%12))
    if [ $i -eq 0 ]
    then
	    if date -d @$time +%D:%H:%M:%S &>/dev/null
	       then
		   echo $(date -d @$time +%D:%H:%M:%S) $volt >> $$.vlt.tmp
		   fi
	time=0
	volt=0
    fi
done

cat $$.vlt.tmp | sort -u
rm $$.vlt.tmp &> /dev/null
