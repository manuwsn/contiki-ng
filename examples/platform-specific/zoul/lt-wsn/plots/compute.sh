#!/bin/bash
i=0

dating=$2

for a in $(cat sensors-in-test)
do
    if grep  "^[0-9]* : $a " $1 > /dev/null
    then
	### Rx Time
	grep "^[0-9]* : $a " $1 | cut -d ' ' -f1,4 | sort -uh > $a.rxt
	
	### Cycles timestamps
	echo "'^[0-9]* : [0-9]* ' $1 $a" | xargs ./Cycles.sh > $a.cyl
	
	## Network trace
	echo "'^[0-9]* : $a (66|68|71|81) ' $1 $dating " |xargs ./Rx.sh > $a.rx
	
	## All data contains volts info
	echo "'^[0-9]* : $a 6[68]' $1 $dating" |xargs ./Volts.sh > $a.vlt
	
	## Case for energest data
	echo "'^[0-9]* : $a [78]1' $1 $dating" |xargs ./Energest.sh > $a.nrg
	
	## UIP and RPL stats
	echo "'^[0-9]* : $a 83' $1 $dating" |xargs ./UipRpl.sh > $a.uip
	
	## Analysis of each sensor
	
	## 2 0 : dht22
	if grep -E "^[0-9]* : $a 6[68] ([0-9]+ ){20}2 0" $1 > /dev/null
	   then
	       echo "'^[0-9]* : $a 6[68]' $1" |xargs ./TempDht22.sh > $a.dht22
	fi
	
	## 3 0 Analog light sensor
	if grep -E "^[0-9]* : $a 6[68] ([0-9]+ ){20}3 0" $1 > /dev/null
	   then
	       echo "'^[0-9]* : $a 6[68]' $1" |xargs ./LightA.sh > $a.lux
	fi
    fi
done



## plotting RPL UIP stats
if ls *.uip &> /dev/null
then
    uiprpl=""
    uipfiles=$(ls *.uip)
    uipfiles=$(echo $uipfiles | sed s/' '/'"'' using 1:2','"'/g)
    uipfiles="\"$uipfiles\" using 1:2"
    uiprpl="$uipfiles"
    uipfiles=$(ls *.uip)
    uipfiles=$(echo $uipfiles | sed s/' '/'"'' using 1:3','"'/g)
    uipfiles="\"$uipfiles\" using 1:3"
    uiprpl="$uiprpl, $uipfiles"
    uipfiles=$(ls *.uip)
    uipfiles=$(echo $uipfiles | sed s/' '/'"'' using 1:4','"'/g)
    uipfiles="\"$uipfiles\" using 1:4"
    uiprpl="$uiprpl, $uipfiles"
    uipfiles=$(ls *.uip)
    uipfiles=$(echo $uipfiles | sed s/' '/'"'' using 1:5','"'/g)
    uipfiles="\"$uipfiles\" using 1:5"
    uiprpl="$uiprpl, $uipfiles"
    uipfiles=$(ls *.uip)
    uipfiles=$(echo $uipfiles | sed s/' '/'"'' using 1:6','"'/g)
    uipfiles="\"$uipfiles\" using 1:6"
    uiprpl="$uiprpl, $uipfiles"
    uipfiles=$(ls *.uip)
    uipfiles=$(echo $uipfiles | sed s/' '/'"'' using 1:7','"'/g)
    uipfiles="\"$uipfiles\" using 1:7"
    uiprpl="$uiprpl, $uipfiles"
    uipfiles=$(ls *.uip)
    uipfiles=$(echo $uipfiles | sed s/' '/'"'' using 1:8','"'/g)
    uipfiles="\"$uipfiles\" using 1:8"
    uiprpl="$uiprpl, $uipfiles"
    uipfiles=$(ls *.uip)
    uipfiles=$(echo $uipfiles | sed s/' '/'"'' using 1:9','"'/g)
    uipfiles="\"$uipfiles\" using 1:9"
    uiprpl="$uiprpl, $uipfiles"
    uipfiles=$(ls *.uip)
    uipfiles=$(echo $uipfiles | sed s/' '/'"'' using 1:10','"'/g)
    uipfiles="\"$uipfiles\" using 1:10"
    uiprpl="$uiprpl, $uipfiles"
    uipfiles=$(ls *.uip)
    uipfiles=$(echo $uipfiles | sed s/' '/'"'' using 1:11','"'/g)
    uipfiles="\"$uipfiles\" using 1:11"
    uiprpl="$uiprpl, $uipfiles"
    uipfiles=$(ls *.uip)
    uipfiles=$(echo $uipfiles | sed s/' '/'"'' using 1:12','"'/g)
    uipfiles="\"$uipfiles\" using 1:12"
    uiprpl="$uiprpl, $uipfiles"
    if [ $dating ]
    then
	gnuplot <<EOF
set terminal png size 1200,800
set output "uiprpl.png"
set xdata time
set timefmt "%m/%d/%y:%H:%M:%S"
set format x "%m-%d\n%H:%M"
set grid
set xlabel "Temps"
set ylabel "Number"
set title "UIP and RPL stats"
plot $uiprpl
EOF
    else
	gnuplot <<EOF
set terminal png size 1200,800
set output "uiprpl.png"
set grid
set xlabel "Temps"
set ylabel "Number"
set title "UIP and RPL stats"
plot $uiprpl
EOF
    fi
    eog uiprpl.png
fi

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
       if [ $dating ]
       then
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
       else
	   gnuplot <<EOF
set terminal png size 1200,800
set output "volts.png"
set grid
set xlabel "Temps"
set ylabel "Volts"
set title "Voltage"
plot $voltsfiles 
EOF
       fi
       eog volts.png&
fi

## Plotting Rx time
if ls *.rx &> /dev/null
   then
       txfiles=$(ls *.rx)
       txfiles=$(echo $txfiles | sed s/' '/'"'' using 1:2','"'/g)
       txfiles="\"$txfiles\" using 1:2"
       if [ $dating ]
       then
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
       else 
	   gnuplot <<EOF
set terminal png size 1200,800
set output "tx.png"
set grid
set xlabel "Temps"
set ylabel "sec"
set title "Temps accès réseau"
plot $txfiles 
EOF
       fi
       eog tx.png&
fi

## Plotting Energy

if ls *.nrg &> /dev/null
then
    nrgfiles=$(ls *.nrg)
    nrgfiles=$(echo $nrgfiles | sed s/' '/'"'' using 1:2 with lines','"'/g)
    nrgfiles="\"$nrgfiles\" using 1:2 with lines"
    if [ $dating ]
    then
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
    else
	gnuplot <<EOF
set terminal png size 1200,800
set output "energy.png"
set grid
set xlabel "Temps"
set ylabel "mA"
set title "Energy"
plot $nrgfiles
EOF
    fi
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
    tmphmd=""
    dhtfiles=$(ls *.dht22)
    dhtfiles=$(echo $dhtfiles | sed s/' '/'"'' using 1:2 with lines','"'/g)
    dhtfiles="\"$dhtfiles\" using 1:2 with lines"
    tmphmd="$dhtfiles"
    dhtfiles=$(ls *.dht22)
    dhtfiles=$(echo $dhtfiles | sed s/' '/'"'' using 1:3 with lines','"'/g)
    dhtfiles="\"$dhtfiles\" using 1:3 with lines"
    tmphmd="$tmphmd, $dhtfiles"
    
    if [ $dating ]
    then
	gnuplot <<EOF
set terminal png size 1200,800
set output "temp-hmd.png"
set xdata time
set timefmt "%m/%d/%y:%H:%M:%S"
set format x "%m-%d\n%H:%M"
set grid
set xlabel "Temps"
set ylabel "%"
set title "Temperature and humidity"
plot $tmphmd
EOF
    else
	gnuplot <<EOF
set terminal png size 1200,800
set output "temp-hmd.png"
set grid
set xlabel "Temps"
set ylabel "%"
set title "Temperature and humidity"
plot $tmphmd
EOF
    fi
    eog temp-hmd.png&
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


