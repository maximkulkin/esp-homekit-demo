# esp-homekit-demo
Demo of [Apple HomeKit accessory server
library](https://github.com/maximkulkin/esp-homekit).

## Usage ESP32

1. Install [esp-idf](https://github.com/espressif/esp-idf) by following [instructions on esp-idf project page](https://github.com/espressif/esp-idf#setting-up-esp-idf). At the end you should have xtensa-esp32-elf toolchain in your path and IDF_PATH environment variable pointing to esp-idf directory.


2. Initialize and sync all submodules (recursively):
```shell
git submodule update --init --recursive
```
3. Copy wifi.h.sample -> wifi.h and edit it with correct WiFi SSID and password.

4. Configure project:
```
make -C examples/esp32/led menuconfig
```
There are many settings there, but at least you should configure "Serial flasher config -> Default serial port".
Also, check "Components -> HomeKit" menu section.

5. Build example:
```shell
make -C examples/esp32/led all
```
6. To prevent any effects from previous firmware (e.g. firmware crashing right at
   start), highly recommend to erase flash:
```shell
    make -C examples/esp32/led erase_flash
```
7. Upload firmware to ESP32:
```shell
    make -C examples/esp32/led flash
    make -C examples/esp32/led monitor
```

## Garage Door Opener

### GDO with 2 Contacts Switches

1. Switch at CLOSE position
2. Switch at OPEN position

