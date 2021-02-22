# TinyUSB C++ Extension

I really like the [TinyUSB project](https://github.com/hathach/tinyusb). It allows you to extend and use the USB stack if you have a Microcontroller that supports this.

However, it is C only and not too easy to use: I wanted to have some functionality where I can experiment with the USB descriptors and where I can add and combine Descriptors as easy as possible. So here is the result.

I am using it mainly together with the Raspberry Pico (hence the cmake), but it should work with other processors as well.
The [class documentation can be found in the doc folder](./doc/index.html).

Setting up a USB device descriptor can be done with one line of code:

```
USBDevice::instance().idVendor(0xCafe).idProduct(0x0001).bcdDevice(0x0100).manufacturer("TinyUSB").product("TinyUSB Device").serialNumber("123456";
```

Defining the Configuration descriptor and the related strings also usually only 2 lines of code. Here is a MIDI example:
```
USBConfiguration* config = USBDevice::instance().setConfigurationDescriptor(TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN,TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100)); 
config->addDescriptor(TUD_MIDI_DESCRIPTOR(ITF_NUM_MIDI, 0, EPNUM_MIDI, 0x80 | EPNUM_MIDI, 64));
```

To make the descriptors finally available to the TinyUSB callbacks can be done like this:

```
const uint8_t * tud_descriptor_device_cb(void) {
  return (const uint8_t*) USBDevice::instance().deviceDescriptor();
}

const uint8_t * tud_descriptor_configuration_cb(uint8_t index){
  return USBDevice::instance().configurationDescriptor(index);
}

const uint16_t* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  return USBDevice::instance().string(index);
}

```
