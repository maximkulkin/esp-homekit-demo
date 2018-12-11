PROGRAM = led_strip_button

EXTRA_COMPONENTS = \
	extras/http-parser \
	extras/i2s_dma \
	extras/ws2812_i2s \
	extras/dhcpserver \
	$(abspath ../../components/wifi_config) \
	$(abspath ../../components/wolfssl) \
	$(abspath ../../components/cJSON) \
	$(abspath ../../components/homekit)

FLASH_SIZE ?= 32
# FLASH_SIZE ?= 8
# HOMEKIT_SPI_FLASH_BASE_ADDR ?= 0x7A000

EXTRA_CFLAGS += -I../.. -DHOMEKIT_SHORT_APPLE_UUIDS

include $(SDK_PATH)/common.mk

LIBS += m

monitor:
	$(FILTEROUTPUT) --port $(ESPPORT) --baud 115200 --elf $(PROGRAM_OUT)
