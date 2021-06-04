
for v in $(grep "$1" $2 | cut -d ' ' -f1)
do
    echo $v >> $$.txr.tmp
done
oldv=
time=0
oldori=0
for v in $(cat $$.txr.tmp)
do
    if [ $(($oldv+200)) -lt $v ]
    then
	echo $oldori $time
	oldori=$v
	time=$v
    else
	time=$(($v - $ori))
	oldv=$v
    fi
done
rm $$.txr.tmp
