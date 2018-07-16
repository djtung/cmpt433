rmmod morsecode
insmod morsecode.ko
echo morse-code > /sys/class/leds/beaglebone:green:usr0/trigger
cat /sys/class/leds/beaglebone:green:usr0/trigger