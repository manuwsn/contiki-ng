#!/bin/bash

oldv=0
for f in $*
do
    echo $f
    i=0
    for v in $(cat $f)
    do
	if [ $i -eq 0 ]
	then
	    if [ $(($v - $oldv)) -gt 120 ]
	       then
		   echo "$oldv > $v " $(($v - $oldv))
		   oldv=$v
	    fi
	fi
	i=$((($i+1)%2))
    done
done
