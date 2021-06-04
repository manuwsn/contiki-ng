#!/bin/bash

i=0
rdvtime=0
txtime=0

filelength=$(cat $2 | wc -l)
rm $$.vlt.tmp &> /dev/null

for v in $(grep -n "$1" $2  | cut -d ':' -f 1,2| sed s/:/' '/)
do
    case $i in
	0)
	    line=$v;;
	1)
	    rdvtime=$v;;
    esac
    i=$((($i+1)%2))
    if [ $i -eq 0 ]
       then
	   line=$(($line + 1))
	   ip=$(head -$line $2 | tail -1 | grep -o "${3} $")
	   if [ "$ip" == "$3 " ]
	   then
	       line=$(($filelength - $line))
	       txtime=$(tail -$line $2 | grep "^[0-9]* : $3" |  head -1 | cut -d':' -f1)
	       if [ "$txtime" != "" ]
	       then
		   if [ $(($txtime - $rdvtime)) -gt 0 ]
		   then
		       echo $rdvtime $(($txtime - $rdvtime))
		   fi
	       fi
	       
	       
	   fi
    fi
done
