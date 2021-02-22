add_library (tinyusb-cpp  midi/usb_main.cpp)
target_include_directories (tinyusb-cpp PUBLIC ${CMAKE_CURRENT_DIR} ${CMAKE_CURRENT_DIR}/midi)
