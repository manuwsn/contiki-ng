#!/bin/bash

for a in $(cat sensors-adresses)
do
    if grep  "^[0-9]* $a " $1 > /dev/null
    then
	## All data contains volts info
	echo "'^[0-9]* $a 6[68]' $1" |xargs ./Volts.sh > $a.vlt
	## Case for energest data
	echo "'^[0-9]* $a 82' $1" |xargs ./Energest.sh > $a.nrg
	## Analysis of each sensor
	## 2 0 : dht22
	if grep -E "^[0-9]* $a 6[68] ([0-9]+ ){12}2 0" $1 > /dev/null
	   then
	       echo "'^[0-9]* $a 6[68]' $1" |xargs ./TempDht22.sh > $a.dht22
	fi
	## 3 0 Analog light sensor
	if grep -E "^[0-9]* $a 6[68] ([0-9]+ ){12}3 0" $1 > /dev/null
	   then
	       echo "'^[0-9]* $a 6[68]' $1" |xargs ./LightA.sh > $a.lux
	fi
	

	
	
    fi
done

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

## Plotting Energy

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


