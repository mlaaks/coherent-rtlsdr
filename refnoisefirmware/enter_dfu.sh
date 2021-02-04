#!/bin/bash
#stty -F /dev/ttyACM0 9600 -parity cs8 -cstopb
#echo -e "I_AM_A_WIZARD" > /dev/ttyACM0

#problem is the above command will send multi-character packets. let's send one char at a time:
echo "I" > /dev/ttyACM0
echo "_" > /dev/ttyACM0
echo "A" > /dev/ttyACM0
echo "M" > /dev/ttyACM0
echo "_" > /dev/ttyACM0
echo "A" > /dev/ttyACM0
echo "_" > /dev/ttyACM0
echo "W" > /dev/ttyACM0
echo "I" > /dev/ttyACM0
echo "Z" > /dev/ttyACM0
echo "A" > /dev/ttyACM0
echo "R" > /dev/ttyACM0
echo "D" > /dev/ttyACM0
sudo ./resetusb.sh