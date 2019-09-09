PROGRAM = sonoff_basic

EXTRA_COMPONENTS = \
	extras/http-parser \
	extras/dhcpserver \
	$(abspath ../../components/esp-8266/wifi_config) \
	$(abspath ../../components/esp-8266/cJSON) \
	$(abspath ../../components/common/wolfssl) \
	$(abspath ../../components/common/homekit)

FLASH_SIZE ?= 8
FLASH_MODE ?= dout
FLASH_SPEED ?= 40
HOMEKIT_SPI_FLASH_BASE_ADDR ?= 0x7A000

EXTRA_CFLAGS += -I../.. -DHOMEKIT_SHORT_APPLE_UUIDS

include $(SDK_PATH)/common.mk

monitor:
	$(FILTEROUTPUT) --port $(ESPPORT) --baud 115200 --elf $(PROGRAM_OUT)
