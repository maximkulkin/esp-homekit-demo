cmake_minimum_required(VERSION 3.5)

set(EXTRA_COMPONENT_DIRS
    ../../../components/common
    ../../../components/esp-idf
)

include_directories(../../../)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(dynamic_services)
