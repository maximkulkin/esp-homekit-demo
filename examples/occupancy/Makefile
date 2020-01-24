PROGRAM = occupancy

EXTRA_COMPONENTS = \
	extras/http-parser \
	extras/dhcpserver \
	$(abspath ../../components/esp8266-open-rtos/cJSON) \
	$(abspath ../../components/esp8266-open-rtos/wifi_config) \
	$(abspath ../../components/common/button) \
	$(abspath ../../components/common/wolfssl) \
	$(abspath ../../components/common/homekit)

SENSOR_PIN ?= 4

FLASH_SIZE ?= 32

EXTRA_CFLAGS += -I../.. -DHOMEKIT_SHORT_APPLE_UUIDS -DSENSOR_PIN=$(SENSOR_PIN)

include $(SDK_PATH)/common.mk

monitor:
	$(FILTEROUTPUT) --port $(ESPPORT) --baud 115200 --elf $(PROGRAM_OUT)
