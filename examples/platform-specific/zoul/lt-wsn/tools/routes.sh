#!/bin/bash
links=$(grep -n "links: [1-9]" $1 | cut -d ':' -f1,4 | cut -d ' ' -f1,2 | sed s/':'//)
i=0
for v in $links
do
    case $i in
	0)
	    line=$v
	    ;;
	1)
	    nbroutes=$v
	    ;;
    esac
    i=$((($i+1)%2))
    echo ------------------------------------------------------------------------
    head -n $(($line+$nbroutes)) $1 | tail -n $nbroutes
done

