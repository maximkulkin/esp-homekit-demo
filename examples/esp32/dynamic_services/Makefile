PROJECT_NAME = dynamic_services

CFLAGS += -I$(abspath ../../..) -DHOMEKIT_SHORT_APPLE_UUIDS

EXTRA_COMPONENT_DIRS += \
  $(abspath ../../../components/common) \
  $(abspath ../../../components/esp-32)

include $(IDF_PATH)/make/project.mk
