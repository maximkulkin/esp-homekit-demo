PROGRAM = temperature_sensor_oled

EXTRA_COMPONENTS = \
	extras/dht \
	extras/http-parser \
	extras/ssd1306 \
	extras/i2c \
	extras/fonts \
	$(abspath ../../components/esp-8266/cJSON) \
	$(abspath ../../components/common/wolfssl) \
	$(abspath ../../components/common/homekit)

# Enable fonts provided by extras/fonts package
FONTS_TERMINUS_6X12_ISO8859_1 = 1
FONTS_TERMINUS_BOLD_6X12_ISO8859_1 = 1
FONTS_TERMINUS_BOLD_12X24_ISO8859_1 = 1
FONTS_TERMINUS_BOLD_14X28_ISO8859_1 = 1
FONTS_TERMINUS_BOLD_16X32_ISO8859_1 = 1

# DHT11 sensor pin
SENSOR_PIN ?= 0

FLASH_SIZE ?= 32

EXTRA_CFLAGS += -I../.. -DHOMEKIT_SHORT_APPLE_UUIDS -DSENSOR_PIN=$(SENSOR_PIN)

include $(SDK_PATH)/common.mk

monitor:
	$(FILTEROUTPUT) --port $(ESPPORT) --baud 115200 --elf $(PROGRAM_OUT)
