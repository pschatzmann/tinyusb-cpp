if (DEFINED $ENV{TINYUSB_SDK_PATH})
    set(TINYUSB_SDK_PATH $ENV{TINYUSB_SDK_PATH})
    message("TinyUDB path is ${TINYUSB_SDK_PATH}")
elseif(DEFINED $ENV{PICO_SDK_PATH} )
    set(TINYUSB_SDK_PATH $ENV{PICO_SDK_PATH}/lib/tinyusb)
    message("TinyUDB path is ${TINYUSB_SDK_PATH}")
else()
    message(FATAL_ERROR "TinyUSB location was not defined")
endif()


target_include_directories (tinyusb-cpp PUBLIC 
    ${TINYUSB_SDK_PATH}/src 
    ${CMAKE_CURRENT_SOURCE_DIR}
)
