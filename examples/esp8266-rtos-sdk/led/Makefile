PROJECT_NAME = led

EXTRA_CFLAGS += -I$(abspath ../../..) -DHOMEKIT_SHORT_APPLE_UUIDS

EXTRA_COMPONENT_DIRS += \
  $(abspath ../../../components/common) \
  $(abspath ../../../components/esp8266-rtos-sdk)

include $(IDF_PATH)/make/project.mk
