cmake_minimum_required(VERSION 3.19)

include(GoogleTest)

project(ArduinoPicoTests)

set(PICO_SDK_ARDUINO_PATH $ENV{PICO_SDK_ARDUINO_PATH})
set(ARDUINO_USB_PATH ${PICO_SDK_ARDUINO_PATH}/Arduino/USB)
set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
set(TINYUSB_PATH ${PICO_SDK_PATH}/lib/tinyusb/src)

find_package(GTest CONFIG REQUIRED) 

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ARDUINO_USB_PATH}
    ${TINYUSB_PATH}
)

set(default_build_type "Debug")

add_executable(USBTest USBTest.cxx)
gtest_add_tests(TARGET      USBTest
                TEST_SUFFIX .noArgs
                TEST_LIST   noArgsTests
)

target_link_libraries(USBTest PRIVATE
    GTest::gtest 
)

set_tests_properties(${noArgsTests}   PROPERTIES TIMEOUT 10)

