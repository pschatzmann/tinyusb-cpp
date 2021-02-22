add_library (tinyusb-cpp STATIC ${CMAKE_CURRENT_SOURCE_DIR}/src/midi/usb_main.cpp)
target_include_directories (tinyusb-cpp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/midi)
