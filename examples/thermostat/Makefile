PROGRAM = thermostat

EXTRA_COMPONENTS = \
	extras/dht \
	extras/http-parser \
	extras/ssd1306 \
	extras/dhcpserver \
	extras/i2c \
	extras/fonts \
	$(abspath ../../components/esp8266-open-rtos/cJSON) \
	$(abspath ../../components/esp8266-open-rtos/wifi_config) \
	$(abspath ../../components/common/wolfssl) \
	$(abspath ../../components/common/homekit) \
	$(abspath ../../components/common/button) 

# include a lot of builtin fonts

FONTS_TERMINUS_6X12_ISO8859_1 = 1
FONTS_TERMINUS_BOLD_12X24_ISO8859_1 = 1
FONTS_TERMINUS_BOLD_14X28_ISO8859_1 = 1
FONTS_TERMINUS_BOLD_16X32_ISO8859_1 = 1

FLASH_SIZE ?= 32

EXTRA_CFLAGS += -I../.. -DHOMEKIT_SHORT_APPLE_UUIDS

include $(SDK_PATH)/common.mk

monitor:
	$(FILTEROUTPUT) --port $(ESPPORT) --baud 115200 --elf $(PROGRAM_OUT)
