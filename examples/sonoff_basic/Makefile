PROGRAM = sonoff_basic

# Set to true to use the hardcoded wifi configuration in wifi.h
HARDCODE_WIFI = true

EXTRA_COMPONENTS = \
	extras/http-parser \
	$(abspath ../../components/wolfssl) \
	$(abspath ../../components/cJSON) \
	$(abspath ../../components/homekit)

ifeq "$(HARDCODE_WIFI)" "true"
	EXTRA_CFLAGS += -DHARDCODE_WIFI
else
	EXTRA_COMPONENTS = \
		extras/dhcpserver \
		$(abspath ../../components/wifi_config)
endif


FLASH_SIZE = 8
FLASH_MODE = dout
FLASH_SPEED = 40
HOMEKIT_SPI_FLASH_BASE_ADDR = 0x7A000

EXTRA_CFLAGS += -I../.. -DHOMEKIT_SHORT_APPLE_UUIDS

include $(SDK_PATH)/common.mk

monitor:
	$(FILTEROUTPUT) --port $(ESPPORT) --baud 115200 --elf $(PROGRAM_OUT)
