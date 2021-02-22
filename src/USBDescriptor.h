/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Phil Schatzmann
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */


/**
 * @brief It is too hard to experiment and define new USB descriptors. In order to simplify the task
 * I have just written some simple API which should help to set up a new USB device.
 * The focus was on the user friedliness on the cost of memory efficiency.
 * 
 * No memory is released because the functionality is supposed to be used for setting up the USB descriptors which
 * need to be available all the times. The USBConfigurationDescriptorData Buffer can not grow dynamically, so we need to make
 * sure that it is big enough in the beginning - but not too big so that we don't wast memory.  Call USBDevice.descriptorTotalSize()
 * to set the proper size.
 * 
 * 
 * @version 0.1
 * @date 2021-02-20
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#pragma once
#include "tusb.h"
#include <stdarg.h>

/**
 * @brief Constants
 * 
 */
#define DEFAULT_LANGUAGE  0x0409

// forward declarations  USBDevice -> USBConfiguration -> USBInterface -> USBEndpoint
class USBConfiguration;
class USBDevice;
class USBInterface;
class USBEndpoint;

// Some enums which are relevant for the endpoint definition
enum TransferType {Control=0b00,Isochronous=0b01,Bulk=0b10,Interrupt=0b11}; 
enum SynchronisationType {NoSynchonisation=0b0, Asynchronous=0b01, Adaptive=0b10, Synchronous=0b11};
enum UsageType {DataEndPoint=0b00, FeedbackEndpoint=0b01, ExplicitFeedbackDataEndpoint=0b10,Reserved=0b11 };

/**
 * @brief Simple dynamic array - A std::vector would have been perfect but it is not available
 * in all environments
 * 
 * @tparam T 
 */
template<class T> 
class Vector {
    public:
        Vector(){
             this->increment_by = 5;
             grow(5);
         }

        Vector(const T empty, int initial_size = 5, int incrementBy = 5){
            this->empty = empty;
            this->increment_by = incrementBy;
            // allocate intial memory
            grow(initial_size);
        }

        void append(T value){
            if (grow(actual_size)){
                data_[actual_size] = value;
                actual_size++;
            }
        }

        int size() {
            return actual_size;
        }

        T& get(int index){
            if (index>=actual_size){
                return empty;
            }
            return data_[index];
        }

        T& operator[]( uint16_t pos ){
            return get(pos);
        }

        T* data() {
            return data_;
        }

        void clear() {
            actual_size = 0;
        }

        bool resize(int newSize){
            return grow(newSize);
        }

        bool checkSize(int size){
            return size< max_size;
        }

    protected:
        int max_size = 0;
        int actual_size = 0;
        int increment_by = 0;
        T *data_ = nullptr;
        T empty;

        bool grow(int newSize){
            bool result = true;
            if (newSize>max_size) {
                int newSizeCalculated  = newSize;
                if (increment_by>0){
                    newSizeCalculated = ((newSize / increment_by)+1) * increment_by;
                }
                T* new_data = new T[newSizeCalculated];
                if (data_!=nullptr && new_data!=nullptr) {
                    // recover old data
                    memcpy(new_data, data_, actual_size*sizeof(T));
                    // release old memory
                    delete[] data_;
                }
                if (new_data!=nullptr){
                    max_size = newSizeCalculated;
                    data_ = new_data;
                } else {
                    // out of memory
                    result = false;
                }
            }
            return result;
        }
};

/**
 * @brief Data for the USBConfiguration and dependent configurations. We use this separate class to get the dependency
 * restrictions of the header only approach out of the way.
 * 
 */
class USBConfigurationDescriptorData {
    public:
        static USBConfigurationDescriptorData &instance(){
            static USBConfigurationDescriptorData inst;
            return inst;
        }

        void clear() {
            buffer()->clear();
            length = 0;
        }
        // we add some descriptor information to the buffer
        uint8_t* addDescriptor(const uint8_t* ptr, int size_in){
            int size = size_in;

            // if the size is not indicated we use it from the data
            if (size==0){
                size = ptr[0];
            }

            // make sure we have enough space
            if (!buffer()->checkSize(totalSize() + size_in)){
                return nullptr;
            }

            uint8_t* result = buffer()->data() + length; // current position
            if (ptr!=nullptr){
                // make sure there is enough space
                memcpy(result, ptr, size);
            }
            length += size;
            return result;
        }

        uint8_t* data() {
            return buffer()->data();
        }

        uint16_t totalSize() {
            return length;
        }

    protected:
        uint8_t EMPTY=0;
        Vector<uint8_t> *buffer_ptr = nullptr;
        uint16_t length;

        // use as singleton -> prevent instaniation 
        USBConfigurationDescriptorData(){}

        Vector<uint8_t> *buffer() {
            // data is allocated the first time it is used
            if (buffer_ptr==nullptr){
                buffer_ptr = new Vector<uint8_t>(EMPTY, 256,0);
            }
            return buffer_ptr;
        }

        friend class USBDevice;
};


/**
 * @brief USB String Descriptors are accessed with the help of a index id. The strings are
 * available starting from index 1.
 * 
 * Index 0 is used for the language.
 */

class USBStrings {
    public:
        static USBStrings &instance() {
            static USBStrings inst;
            return inst;
        }

        // adds an ascii string and provides the resulting new index id
        uint8_t add(const char* str){
            char_array.append(str);
            return char_array.size();
        }

        // provides the USB string descriptor in UTF
        uint16_t* string(int index){
            if (index==0){
                return language;
            }
            return toUtf(get(index));
        }
    
        // returns the ascii string 
        const char* get(int index){
            return char_array[index-1];
        }
    
        int size(){
            return char_array.size();
        }

        void setLanguage(uint16_t lang){
            uint8_t *byte_ptr = (uint8_t *) language;
            byte_ptr[0] = 4;
            byte_ptr[1] = 0x03;       
            language[1] = lang;           
        }

        static bool equals(const uint16_t* str1, const uint16_t*str2){
            uint8_t* char_ptr1 = (uint8_t*)str1;
            uint8_t* char_ptr2 = (uint8_t*)str2;
            // compare length
            if (char_ptr1[0]!=char_ptr2[0])
                return false;
            uint8_t len = char_ptr1[0];
            return memcmp(str1, str2, len);
        }

        void clear() {
            char_array.clear();
        }

    protected:
        Vector<const char*> char_array = Vector<const char*>(nullptr, 5, 5);
        uint16_t result[32];
        uint16_t language[2];

        USBStrings() {
            setLanguage(DEFAULT_LANGUAGE);
        }

        uint16_t* toUtf(const char *str){
            if (str==nullptr){
                return nullptr;
            }
            // Cap at max char
            int len = strlen(str);
            if ( len > 31 ) 
                len = 31;

            // Convert ASCII string into UTF-16
            for(uint8_t i=0; i<len; i++){
                result[1+i] = str[i];
            }
            
            // first byte is length (including header), second byte is string type
            uint8_t *byte_ptr = (uint8_t *) result;
            byte_ptr[0] = (2*len + 2);
            byte_ptr[1] = 0x03;
            return result;
        }

};

/**
 * @brief Shared functionality of all USB Descriptor Mgmmt classes. Currently it it just provides
 * you the possibility to mark a descriptor as defined, so that you can use this in your logic
 * e.g. to avoid double intitializations.
 * 
 */
class USBBase {
    public:
        // just mark the devie as defined
        USBBase &done(bool isDone){
            is_done = isDone;
            return *this;
        }

        // check if we have defined all relevant parameters
        bool isDone(){
            return is_done;
        }

        // Add a descriptor define as it is usually used in TinyUSB
        uint8_t* addDescriptor(uint8_t len, ...){
            uint8_t tmp[len];
            tmp[0]=len;
            va_list ap;
            va_start(ap, len); 
            for (int i = 1; i < len; i++){
                tmp[i] = va_arg(ap, uint8_t);
            }
            va_end(ap);
            return USBConfigurationDescriptorData::instance().addDescriptor(tmp,len);
        }

        // Add a descriptor as array
        uint8_t* addDescriptor(const uint8_t* desc, int len){
            return USBConfigurationDescriptorData::instance().addDescriptor(desc, len);
        }


    protected:
        bool is_done; // just a single flag to record if we have defined all parameters

};


/**
 * @brief Endpoint descriptors are used to describe endpoints other than endpoint zero. Endpoint zero is always assumed to be a control endpoint and is 
 * configured before any descriptors are even requested. The host will use the information returned from these descriptors to determine the 
 * bandwidth requirements of the bus.
 */

class USBEndpoint : public USBBase {
    public:
        // Maximum Packet Size this endpoint is capable of sending or receiving
        USBEndpoint& wMaxPacketSize(uint16_t val){
            descriptor()->bInterval = val;
            return *this;
        }

        // Interval for polling endpoint data transfers. Value in frame counts. Ignored for Bulk & Control Endpoints. Isochronous must equal 1 and field may range from 1 to 255 for interrupt endpoints.
        USBEndpoint& bInterval(uint8_t val){
            descriptor()->bInterval = val;
            return *this;
        }
        
        int size() {
            return descriptor()->bLength;
        }

        tusb_desc_endpoint_t* descriptor() {
            return descriptor_data;
        }


    protected:
        USBInterface *parent;
        tusb_desc_endpoint_t* descriptor_data; // if assigned direcly 

        USBEndpoint(USBInterface *parent, int endpointNumber, bool isInput, TransferType xfer ){
            this->parent = parent;
            descriptor_data =  (tusb_desc_endpoint_t*) USBConfigurationDescriptorData::instance().addDescriptor(nullptr, sizeof(tusb_desc_endpoint_t));
            descriptor_data->bLength = sizeof(tusb_desc_endpoint_t)         ; ///< Size of this descriptor in bytes
            descriptor_data->bDescriptorType = 0x05 ; ///< ENDPOINT Descriptor Type

            ///< The address of the endpoint on the USB device described by this descriptor. The address is encoded as follows: \n Bit 3...0: The endpoint number \n Bit 6...4: Reserved, reset to zero \n 
            // Bit 7: Direction, ignored for control endpoints 0 = OUT endpoint 1 = IN endpoint.
            descriptor_data->bEndpointAddress = endpointNumber & 0b00000111 | isInput<7 ; 
            descriptor_data->bmAttributes.xfer = xfer;
            descriptor_data->bmAttributes.sync = 0x00;     // 00 = No Synchonisation
            descriptor_data->bmAttributes.usage = 0x00 ;   // 00 = Data Endpoint
            descriptor_data->wMaxPacketSize.size =  64;     // Maximum Packet Size (only high speed support up to 512)
            descriptor_data->bInterval  = 1;               // Interval for polling endpoint data transfers.
        }

        USBEndpoint(USBInterface *parent, tusb_desc_endpoint_t *data){
            this->parent = parent;
            this->descriptor_data = data;
        }

        friend class USBInterface;
        friend class USBConfiguration;

};

/**
 * @brief  The interface descriptor could be seen as a header or grouping of the endpoints into a functional group performing a single feature of the device. 
 * 
 */
class USBInterface : public USBBase {
    public:
        // creats a new endpoint
        USBEndpoint& createEndpoint(bool isInput, TransferType xfer=Isochronous) {
            USBEndpoint *result = new USBEndpoint(this, usbEndpointCount(), isInput, xfer);
            descriptor()->bNumEndpoints++;
            endpoints.append(result);
            return *result;
        }

        // creates a new endpoint from the external data
        USBEndpoint& createEndpoint(tusb_desc_endpoint_t *data) {
            USBEndpoint *result = new USBEndpoint(this, data);
            descriptor()->bNumEndpoints++;
            endpoints.append(result);
            return *result;
        }
       
        USBEndpoint& controlEndpoint() {
            return *endpoints[0];
        }

        USBEndpoint &usbEndpoint(int index){
            return *endpoints[index];
        }

        // string descriptor describing this interface
        USBInterface &name(char *name){
            descriptor()->iInterface = USBStrings::instance().add(name);
            return *this;
        }

        //if you manage the descriptors separatly
        USBInterface &iInterface(uint16_t idx){
            descriptor()->iInterface = idx;
            return *this;
        }
        ///< Value used to select this alternate setting for the interface identified in the bInterfaceNumber
        USBInterface &bAlternateSetting(uint8_t value){
            descriptor()->bAlternateSetting = value;
            return *this;
        }

        ///< Class code (assigned by the USB-IF). \li A value of zero is reserved for future standardization. \li If this field is set to FFH, the interface class is vendor-specific. \li All other values are reserved for assignment by the USB-IF.
        USBInterface &bInterfaceClass(uint8_t value){
            descriptor()->bInterfaceClass = value;
            return *this;
        }

        ///< Subclass code (assigned by the USB-IF). \n These codes are qualified by the value of the bInterfaceClass field. \li If the bInterfaceClass field is reset to zero, this field must also be reset to zero. \li If the bInterfaceClass field is not set to FFH, all values are reserved for assignment by the USB-IF. 
        USBInterface &bInterfaceSubClass(uint8_t value){
            descriptor()->bInterfaceSubClass = value;
            return *this;
        }

        ///< Protocol code (assigned by the USB). \n These codes are qualified by the value of the bInterfaceClass and the bInterfaceSubClass fields. If an interface supports class-specific requests, this code identifies the protocols that the device uses as defined by the specification of the device class. \li If this field is reset to zero, the device does not use a class-specific protocol on this interface. \li If this field is set to FFH, the device uses a vendor-specific protocol for this interface.
        USBInterface &bInterfaceProtocol(uint8_t value){
            descriptor()->bInterfaceProtocol = value;
            return *this;
        }

        int usbEndpointCount() {
            return endpoints.size();
        }

        USBConfiguration *usbConfiguration() {
            return parent;
        }

        // size of descriptor in bytes
        int size() {
            return descriptor()->bLength;
        }

        tusb_desc_interface_t* descriptor() {
            return (tusb_desc_interface_t* )descriptor_data;
        }


    protected:
        USBConfiguration *parent;
        Vector<USBEndpoint*> endpoints;
        tusb_desc_interface_t *descriptor_data;

        USBInterface(USBConfiguration *parent, int interfaceNumber){
            this->parent = parent;
            descriptor_data = (tusb_desc_interface_t*) USBConfigurationDescriptorData::instance().addDescriptor(nullptr, sizeof(tusb_desc_interface_t));

            descriptor()->bLength = sizeof(tusb_desc_interface_t)           ; ///< Size of this descriptor in bytes
            descriptor()->bDescriptorType = 0x04; ///< INTERFACE Descriptor Type
            descriptor()->bInterfaceNumber  = interfaceNumber; ///< Number of this interface. Zero-based value identifying the index in the char_array of concurrent interfaces supported by this configuration.
            descriptor()->bAlternateSetting = 0 ; ///< Value used to select this alternate setting for the interface identified in the prior field
            descriptor()->bNumEndpoints  = 0; ///< Number of endpoints used by this interface (excluding endpoint zero). If this value is zero, this interface only uses the Default Control Pipe.
            descriptor()->bInterfaceClass = 0; ///< Class code (assigned by the USB-IF). \li A value of zero is reserved for future standardization. \li If this field is set to FFH, the interface class is vendor-specific. \li All other values are reserved for assignment by the USB-IF.
            descriptor()->bInterfaceSubClass = 0; ///< Subclass code (assigned by the USB-IF). \n These codes are qualified by the value of the bInterfaceClass field. \li If the bInterfaceClass field is reset to zero, this field must also be reset to zero. \li If the bInterfaceClass field is not set to FFH, all values are reserved for assignment by the USB-IF.
            descriptor()->bInterfaceProtocol = 0; ///< Protocol code (assigned by the USB). \n These codes are qualified by the value of the bInterfaceClass and the bInterfaceSubClass fields. If an interface supports class-specific requests, this code identifies the protocols that the device uses as defined by the specification of the device class. \li If this field is reset to zero, the device does not use a class-specific protocol on this interface. \li If this field is set to FFH, the device uses a vendor-specific protocol for this interface.
            descriptor()->iInterface = 0 ; ///< Index of string descriptor describing this interface

            // create the default control interface - the isInput is ignored by control type
            createEndpoint(true, Control);
        } 

        USBInterface(USBConfiguration *parent, tusb_desc_interface_t* data){
            this->parent = parent;
            descriptor_data = data;
        } 


        friend class USBConfiguration;   
        friend class USBEndpoint;   
};

/**
 * @brief The configuration descriptor specifies values such as the amount of power this particular configuration uses, if the device is self or bus powered
 *  and the number of interfaces it has. When a device is enumerated, the host reads the device descriptors and can make a decision of which configuration 
 * to enable. It can only enable one configuration at a time.
 * 
 */

class USBConfiguration  : public USBBase {
    public:
        // creates a new interface with some default values set
        USBInterface *createInterface(){
            descriptor()->bNumInterfaces++;
            USBInterface* result = new USBInterface(this, usbInterfaceCount());
            interfaces.append(result);
            return result;
        }

        // creats a new interface using the provided external data
        USBInterface *createInterface(tusb_desc_interface_t *data){
            USBInterface* result = new USBInterface(this, data);
            interfaces.append(result);
            descriptor()->bNumInterfaces = interfaces.size();
            return result;
        }


        // We might already have the descriptors from some examples
        void setConfigurationDescriptor(const uint8_t* desc, int len,  bool parse=false){
            this->descriptor_data = (tusb_desc_configuration_t*)  USBConfigurationDescriptorData::instance().addDescriptor(desc, len);
            //this->descriptor_data = (tusb_desc_configuration_t*) USBConfigurationDescriptorData::instance().data();
            if (parse){
                parseDescriptor((uint8_t *) this->descriptor_data, len);
            }
        }

        // Maximum power consumption of the USB device from the bus in this specific configuration when the device is fully operational. Expressed in mA units 
        USBConfiguration& bMaxPower(uint8_t mAmp){
            // (i.e., 50 = 100 mA).
            descriptor()->bMaxPower = mAmp / 2;
            return *this;
        }

        // D7 Reserved, set to 1. (USB 1.0 Bus Powered) D6 Self Powered D5 Remote Wakeup D4..0 Reserved, set to 0.
        USBConfiguration& bmAttributes(uint8_t value){
            descriptor()->bmAttributes = value;
            return *this;
        }

        // size in bytes
        int size() {
            return descriptor()->bLength;
        }

        int totalSize() {
            return descriptor()->wTotalLength;
        }

        USBDevice* usbDevice(){
            return parent;
        }

        int usbInterfaceCount() {
            return interfaces.size();
        }

        USBInterface* usbInterface(int idx){
            return interfaces[idx];
        }

        // provides access to the combined descriptor
        uint8_t* configurationDescriptor() {
            USBConfigurationDescriptorData data = USBConfigurationDescriptorData::instance();
            //descriptor()->wTotalLength = data.totalSize();
            return  data.data();
        }

        // provides access to the combined descriptor -adapts the packet size for high speed 
        uint8_t* configurationDescriptorExt(int packetSizeHighSpeed=512) {
            if (tud_speed_get() == TUSB_SPEED_HIGH){
                for (int j=0;j<interfaces.size();j++){
                    for (int i=0; i< interfaces[j]->endpoints.size();i++){
                        interfaces[j]->endpoints[i]->wMaxPacketSize(packetSizeHighSpeed);
                    }
                }
            }
            USBConfigurationDescriptorData data = USBConfigurationDescriptorData::instance();
            descriptor()->wTotalLength = data.totalSize();
            return  data.data();
        }

        // tries to find a descriptor in the memory buffer by id
        uint8_t * findDescriptor(uint8_t id, uint8_t idx){
            uint8_t *ptr = USBConfigurationDescriptorData::instance().data();
            uint8_t *end = ptr + USBConfigurationDescriptorData::instance().totalSize();
            int find_count=0;
            while(ptr<end){
                uint8_t len = ptr[0];
                if (ptr[1]==id){
                    if (find_count==idx){
                        return ptr;
                    }
                    find_count++;
                }
                ptr += len;
            }  
            return nullptr;
        }

    protected:
        USBDevice *parent;
        Vector<USBInterface*> interfaces;
        tusb_desc_configuration_t *descriptor_data;
        int id;

        tusb_desc_configuration_t* descriptor() {
            if (descriptor_data==nullptr){
                descriptor_data = (tusb_desc_configuration_t*) USBConfigurationDescriptorData::instance().addDescriptor(nullptr, sizeof(tusb_desc_configuration_t));
                descriptor_data->bLength = sizeof(tusb_desc_configuration_t); ///< Size of this descriptor in bytes
                descriptor_data->bDescriptorType = 0x02; ///< CONFIGURATION Descriptor Type
                descriptor_data->bConfigurationValue = id; //= parent->usbConfigurationCount()-1;   ///< Value to use as an argument to the SetConfiguration() request to select this configuration.
                descriptor_data->iConfiguration = 0;     ///< Index of string descriptor describing this configuration
                descriptor_data->bmAttributes = 0;        ///< Configuration characteristics \n D7: Reserved (set to one)\n D6: Self-powered \n D5: Remote Wakeup \n D4...0: Reserved (reset to zero) \n D7 is reserved and must be set to one for historical reasons. \n A device configuration that uses power from the bus and a local source reports a non-zero value in bMaxPower to indicate the amount of bus power required and sets D6. The actual power source at runtime may be determined using the GetStatus(DEVICE) request (see USB 2.0 spec Section 9.4.5). \n If a device configuration supports remote wakeup, D5 is set to one.
                descriptor_data->bMaxPower = 50;      
                descriptor_data->bNumInterfaces = 0;      ///< Number of interfaces supported by this configuration
            }
            return descriptor_data;
        }

        USBConfiguration(USBDevice *parent, int id){
            //descriptor_data = new tusb_desc_configuration_t();
            this->parent = parent;
            this->id = id;
        }

        // we parse the descriptor and allocate the objects so that we can access them with our API
        void parseDescriptor(uint8_t *data, int data_len) {
            uint8_t *ptr = data;
            uint8_t *end = data+data_len;
            USBInterface * actual_itf = nullptr;
            uint8_t len = ptr[0];
            while(ptr<end && len>0){
                switch(ptr[1]){
                    case 0x04: //interface
                        actual_itf = this->createInterface((tusb_desc_interface_t*)ptr);
                        break;
                    case 0x05: //endpoint
                        if (actual_itf!=nullptr){
                            USBEndpoint ep = actual_itf->createEndpoint((tusb_desc_endpoint_t*) ptr);
                            ep.descriptor_data = (tusb_desc_endpoint_t*)ptr;
                        }
                        break;
                    default:
                        // nothing to do
                        break;
                }
                // advance to next descriptor
                ptr += len;
                len = ptr[0];
            }
        }


        friend class USBDevice; 
};


/**
 * @brief USB devices can only have one device descriptor. The device descriptor includes information such as what USB revision the device complies to, the Product and Vendor IDs 
 * used to load the appropriate drivers and the number of possible configurations the device can have. The number of configurations indicate how many configuration descriptors 
 * branches are to follow. The configuration descriptor specifies values such as the amount of power this particular configuration uses, if the device is self or bus 
 * powered and the number of interfaces it has. When a device is enumerated, the host reads the device descriptors and can make a decision of which configuration to enable. 
 * It can only enable one configuration at a time.
 * 
 */

class USBDevice  : public USBBase {
    public:
        // singleton - provides access to the object
        static USBDevice &instance(){
            static USBDevice device_instance;
            return device_instance;
        }

        // returns the device descriptor required by USB
        const tusb_desc_device_t* deviceDescriptor() {
            return (const tusb_desc_device_t*) descriptor_ptr();
        }

        // returns the device descriptor required by USB
        const tusb_desc_device_t* descriptor() {
            return (const tusb_desc_device_t*) descriptor_ptr();
        }

       // defines the device descriptor from external data
        void setDeviceDescriptor(void* ptr){
            descriptor_data = (tusb_desc_device_t*)ptr;
        }     

        // We can provides the full configuration descriptor for the indicated index
        uint8_t const* configurationDescriptor(int idx) {
            USBConfiguration *conf = configurations[idx];
            return conf->configurationDescriptor();           
        }

        // creates a new configuration descriptor
        USBConfiguration* createConfiguration() {
            descriptor_ptr()->bNumConfigurations++;
            USBConfiguration* result = new USBConfiguration(this, configurations.size());
            configurations.append(result);
            return result;            
        }

        // We might already have the configuration descriptors from some examples already
        USBConfiguration* setConfigurationDescriptor(const uint8_t* descriptors, int len=0, bool parse=false){
            //USBConfigurationDescriptorData::instance().addDescriptor(descriptors, len);
            USBConfiguration* config = singleConfiguration();
            config->setConfigurationDescriptor(descriptors, len, parse);
            return config;
        }

        // We might already have the configuration descriptors from some examples already
        USBConfiguration* setConfigurationDescriptor(uint8_t len ...){
            uint8_t tmp[len];
            tmp[0]=len;
            va_list ap;
            va_start(ap, len); 
            for (int i = 1; i < len; i++){
                tmp[i] = va_arg(ap, uint8_t);
            }
            va_end(ap);
            return setConfigurationDescriptor(tmp, len);
        }


        // usually we just want to have one single configuration. The following methods provides this
        USBConfiguration* singleConfiguration() {
            USBConfiguration *result;
            if (usbConfigurationCount()==0){
                result = createConfiguration();
            } else {
                result = usbConfiguration(0);
            }
            return result;
        }

        // USB Specification Number which device complies too. e.g. 0x0200 for 2.0
        USBDevice bcdUSB(uint16_t bcd){
            descriptor_ptr()->bcdUSB = bcd;
            return *this;
        }         
        // Class Code (Assigned by USB Org)   
        USBDevice bDeviceClass(uint8_t arg){
            descriptor_ptr()->bDeviceClass       = arg;
            return *this;
        }    
        // Subclass Code (Assigned by USB Org)              
        USBDevice bDeviceSubClass(uint8_t arg){
            descriptor_ptr()->bDeviceSubClass    = arg;
            return *this;
        }     
        // Protocol Code (Assigned by USB Org)          
        USBDevice bDeviceProtocol(uint8_t arg){
            descriptor_ptr()->bDeviceProtocol = arg;
            return *this;
        }  
        // Maximum Packet Size for Zero Endpoint. Valid Sizes are 8, 16, 32, 64
        USBDevice bMaxPacketSize0(uint8_t arg){
            descriptor_ptr()->bMaxPacketSize0 = arg;
            return *this;
        }        
        // Vendor ID (Assigned by USB Org)       
        USBDevice idVendor(uint16_t arg){
            descriptor_ptr()->idVendor = arg;
            return *this;
        }                
        // Product ID (Assigned by Manufacturer)      
        USBDevice idProduct(uint16_t arg){
            descriptor_ptr()->idProduct = arg;
            return *this;
        }                 
        // Device Release Number    
        USBDevice bcdDevice(uint16_t arg){
            descriptor_ptr()->bcdDevice = arg;
            return *this;
        }                     

        // defines the manufacturer string
        USBDevice &manufacturer(const char* str){
            descriptor_ptr()->iManufacturer = USBStrings::instance().add(str);
            return *this;
        }

        // defines the product string
        USBDevice &product(const char* str){
            descriptor_ptr()->iProduct = USBStrings::instance().add(str);
            return *this;
        }

        // defines the chipID string
        USBDevice &serialNumber(const char* str){
            descriptor_ptr()->iSerialNumber = USBStrings::instance().add(str);
            return *this;
        }

        int size() {
            return descriptor_ptr()->bLength;
        }

        int usbConfigurationCount() {
            return configurations.size();
        }

        const uint16_t* string(int index){
            return USBStrings::instance().string(index);
        }

        USBConfiguration* usbConfiguration(int idx){
            return configurations[idx];
        }

        void clear() {
            configurations.clear();
            USBStrings::instance().clear();
            USBConfigurationDescriptorData::instance().clear();
        }

        // defines the total size available for the configuration descriptors and their dependent descriptors
        void descriptorTotalSize(int size){
            this->descriptor_total_size = size;
            USBConfigurationDescriptorData cd = USBConfigurationDescriptorData::instance();
            if (cd.buffer_ptr!=nullptr){
                delete cd.buffer_ptr;
            }
            cd.buffer_ptr = new Vector<uint8_t>(cd.EMPTY, size, 0);
        }

        int getDescriptorTotalSize(){
            return descriptor_total_size;
        }


    protected:
        tusb_desc_device_t *descriptor_data;
        Vector<USBConfiguration*> configurations = Vector<USBConfiguration*>(nullptr,1,1);
        int descriptor_total_size = 225;

        USBDevice() {}

        // returns access to the data
        tusb_desc_device_t *descriptor_ptr(){
            // make shure that we have some valid data
            if (descriptor_data==nullptr){
                descriptor_data = new tusb_desc_device_t();
                descriptor_data->bLength            = sizeof(tusb_desc_device_t);
                descriptor_data->bDescriptorType    = 0x01;
                descriptor_data->bcdUSB             = 0x0200;
                descriptor_data->bDeviceClass       = 0x00;
                descriptor_data->bDeviceSubClass    = 0x00;
                descriptor_data->bDeviceProtocol    = 0x00;
                descriptor_data->bMaxPacketSize0    = 64;
                descriptor_data->idVendor           = 0x0000;
                descriptor_data->idProduct          = 0x0001;
                descriptor_data->bcdDevice          = 0x0001;
                // Index of Manufacturer String Descriptor
                descriptor_data->iManufacturer      = 0x00;
                // Index of Product String Descriptor
                descriptor_data->iProduct           = 0x00;
                // Index of Serial Number String Descriptor
                descriptor_data->iSerialNumber      = 0x00;
                // Number of Possible Configurations
                descriptor_data->bNumConfigurations = 0x00;
            }
            return descriptor_data;
        }
};

#ifdef STREAM_SUPPORT
#include "Stream.h"

/**
 * @brief Dumps a descriptor to the indicated stream. 
 * 
 */
class USBDump {
    public:
        // dumps the descriptor to the indicated ouput stream
        static void dump(Stream &out, void ptr, int len){
            uint8_t *char_ptr = (uint8_t *)ptr;
            out.print("uint8_t descriptor[] = {");
            out.print("  ");
            for (int j=0;j<len-1;j++){
                out.print(char_ptr[j]);
                out.print(", ");
            }
            out.println(char_ptr[len-1]);
            out.println("};");
        }
};

#endif
