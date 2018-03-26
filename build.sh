#!/bin/bash 
# Uncomment the next line to debug this script
# set -x
clear
echo "This script is a helper to make and upload esp-homekit-demo"
echo "Set the script as executable and run with syntax \"./build.sh [target] [tty]\""
echo "For example \"./build.sh examples/led /dev/ttyUSB0\""
echo "Remember to setup your SSID and password in wifi.h in the example directory."
# --------------------
# Set RAM size
read -t5 -n1 -r -p "Is your ESP8266 flash RAM size only 1MB? [Y or N] : " RAM_VAL
: "${RAM_VAL:=N}"
echo
if [ $RAM_VAL = "Y" ] || [ $RAM_VAL = "y" ]
then
    export FLASH_SIZE=8
    export HOMEKIT_SPI_FLASH_BASE_ADDR=0x7a000
    echo "Flash size = $FLASH_SIZE address = $HOMEKIT_SPI_FLASH_BASE_ADDR"
else
    unset FLASH_SIZE
    unset HOMEKIT_SPI_FLASH_BASE_ADDR
    echo "Using default flash RAM size"
fi
# --------------------
# Set debugging
read -t5 -n1 -r -p "Do you want debugging on? [Y or N] : " DEBUG_VAL
: "${DEBUG_VAL:=N}"
echo
if [ $DEBUG_VAL = "Y" ] || [ $DEBUG_VAL = "y" ]
then
    export HOMEKIT_DEBUG=1
    echo "Debug mode is on"
else
    unset HOMEKIT_DEBUG
    echo "Debug mode is off"
fi
# --------------------
# Check whether to erase firmware (this value is used later on)
echo "Erasing the firmware is recommended, however will also reset the accessory"
echo "serial number and other parameters. You will need to re-pair with your Homekit app"
echo "Do you want to erase previous firmware? [Y or N] : "
read -t5 -n1 -r ERASE_FIRMWARE_VAL
: "${ERASE_FIRMWARE_VAL:=N}"
# --------------------
# Check whether  build parameters are OK
if [ -z "$1" ] || [ -z "$2" ]
then
    echo "Nothing to build or incorrect parameters"
    echo "Try again with syntax \"./build.sh [target] [tty]\""
    echo "For example \"./build.sh examples/led /dev/ttyUSB0\""
    exit 1
fi
# --------------------
# Set the SDK path
export SDK_PATH="$HOME/esp-open-rtos"
echo "SDK path set to $SDK_PATH"
# --------------------
# From this point exit the script on error
set -e
# --------------------
# Set the serial port
export ESPPORT="$2"
# The next line is a workaround - refer to:
# https://askubuntu.com/questions/210177/serial-port-terminal-cannot-open-dev-ttys0-permission-denied
sudo chmod 666 "$2"
echo "ESP port set to $ESPPORT"
# --------------------
# Commence the build!
echo "Commencing build of $1..."
make -C $1 all
# --------------------
# Erase flash if needed
if [ $ERASE_FIRMWARE_VAL = "Y" ] || [ $ERASE_FIRMWARE_VAL = "y" ]
then
    make -C $1 erase_flash
fi
# --------------------
# Upload to flash
echo "Uploading firmware to flash..."
make -C $1 flash
echo "All done. Press a key within 10 sec to monitor, or run \"make -C $1 monitor\"..."
read -t10 -n1 -r KEY_VAL
make -C $1 monitor
exit 0