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
	    if [ $oldv -gt $v ]
	    then
		echo "$oldv > $v " $(($oldv - $v))
	    fi
	    oldv=$v
	fi
	i=$((($i+1)%2))
    done
done
