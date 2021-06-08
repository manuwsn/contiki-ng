#!/bin/bash
i=0
for a in $(cat sensors-adresses)
do
    if grep  "^[0-9]* : $a " $1 > /dev/null
    then
	### Rx Time
	grep "^[0-9]* : $a " $1 | cut -d ' ' -f1,4 | sort -uh > $a.rxt
	### Cycles timestamps
	echo "'^[0-9]* : [0-9]* ' $1 $a" | xargs ./Cycles.sh > $a.cyl
	## Network trace
	echo "'^[0-9]* : $a' $1 $i" |xargs ./Rx.sh > $a.tx
	i=$(($i+1))
	## All data contains volts info
	echo "'^[0-9]* : $a 6[68]' $1" |xargs ./Volts.sh > $a.vlt
	## Case for energest data
	echo "'^[0-9]* : $a [78]1' $1" |xargs ./Energest.sh > $a.nrg
	## Analysis of each sensor
	## 2 0 : dht22
	if grep -E "^[0-9]* : $a 6[68] ([0-9]+ ){12}2 0" $1 > /dev/null
	   then
	       echo "'^[0-9]* : $a 6[68]' $1" |xargs ./TempDht22.sh > $a.dht22
	fi
	## 3 0 Analog light sensor
	if grep -E "^[0-9]* : $a 6[68] ([0-9]+ ){12}3 0" $1 > /dev/null
	   then
	       echo "'^[0-9]* : $a 6[68]' $1" |xargs ./LightA.sh > $a.lux
	fi
    fi
done


## plotting Rx Times
if ls *.rxt &> /dev/null
then
    rxtfiles=$(ls *.rxt)
    rxtfiles=$(echo $rxtfiles | sed s/' '/'"'' using 1:2','"'/g)
    rxtfiles="\"$rxtfiles\" using 1:2"
    gnuplot <<EOF
set terminal png size 1200,800
set output "rxtime.png"
set grid
set xlabel "Temps"
set ylabel "Type"
set title "Reception Time"
plot $rxtfiles
EOF
    eog rxtime.png
fi

## Plotting voltage
if ls *.vlt &> /dev/null
   then
       voltsfiles=$(ls *.vlt)
       voltsfiles=$(echo $voltsfiles | sed s/' '/'"'' using 1:2 with lines','"'/g)
       voltsfiles="\"$voltsfiles\" using 1:2 with lines"
       
       gnuplot <<EOF
set terminal png size 1200,800
set output "volts.png"
set xdata time
set timefmt "%m/%d/%y:%H:%M:%S"
set format x "%m-%d\n%H:%M"
set grid
set xlabel "Temps"
set ylabel "Volts"
set title "Voltage"
plot $voltsfiles 
EOF
       eog volts.png&
fi

## Plotting Tx time
if ls *.tx &> /dev/null
   then
       txfiles=$(ls *.tx)
       txfiles=$(echo $txfiles | sed s/' '/'"'' using 1:2','"'/g)
       txfiles="\"$txfiles\" using 1:2"
       
       gnuplot <<EOF
set terminal png size 1200,800
set output "tx.png"
set xdata time
set timefmt "%m/%d/%y:%H:%M:%S"
set format x "%m-%d\n%H:%M"
set grid
set xlabel "Temps"
set ylabel "sec"
set title "Temps accès réseau"
plot $txfiles 
EOF
       eog tx.png&
fi

## Plotting Energy

if ls *.nrg &> /dev/null
then
    nrgfiles=$(ls *.nrg)
    nrgfiles=$(echo $nrgfiles | sed s/' '/'"'' using 1:2 with lines','"'/g)
    nrgfiles="\"$nrgfiles\" using 1:2 with lines"
    
    gnuplot <<EOF
set terminal png size 1200,800
set output "energy.png"
set xdata time
set timefmt "%m/%d/%y:%H:%M:%S"
set format x "%m-%d\n%H:%M"
set grid
set xlabel "Temps"
set ylabel "mA"
set title "Energy"
plot $nrgfiles
EOF
    eog energy.png&
fi

## Plotting cycles

if ls *.cyl &> /dev/null
then
    cyclefiles=$(ls *.cyl)
    cyclefiles=$(echo $cyclefiles | sed s/' '/'"'' using 1:2','"'/g)
    cyclefiles="\"$cyclefiles\" using 1:2"
    
    gnuplot <<EOF
set terminal png size 1200,800
set output "cycles.png"
set grid
set xlabel "Temps"
set ylabel "Tmps"
set title "Cycles"
plot $cyclefiles
EOF
    eog cycles.png&
fi

## Plotting Temperature
if ls *.dht22 &> /dev/null
then
    tempfiles=$(ls *.dht22)
    tempfiles=$(echo $tempfiles | sed s/' '/'"'' using 1:2','"'/g)
    tempfiles="\"$tempfiles\" using 1:2"
    
    gnuplot <<EOF
set terminal png size 1200,800
set output "temperatures.png"
set xdata time
set timefmt "%m/%d/%y:%H:%M:%S"
set format x "%m-%d\n%H:%M"
set grid
set xlabel "Temps"
set ylabel "Temp"
set title "Temperature"
plot $tempfiles
EOF
    eog temperatures.png&
fi

## Plotting light

if ls *.lux 2> /dev/null
then
    lightfiles=$(ls *.lux)
    voltsfiles=$(echo $lightfiles | sed s/' '/'"'' using 1:2','"'/g)
    lightfiles="\"$lightfiles\" using 1:2"

    gnuplot <<EOF
set terminal png size 1200,800
set output "lights.png"
set xdata time
set timefmt "%m/%d/%y:%H:%M:%S"
set format x "%m-%d\n%H:%M"
set grid
set xlabel "Temps"
set ylabel "Lux"
set title "Light"
plot $lightfiles
EOF
    eog lights.png&
fi


