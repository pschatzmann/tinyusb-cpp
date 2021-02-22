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


#pragma once
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "device/usbd.h"
#include "bsp/board.h"
#include "tusb.h"
#include "PicoSemaphore.h"
#include "PicoTimer.h"
#include "USBDescriptor.h"

/* A combination of interfaces must have a unique product id, since PC will save device driver after the first plug.
 * Same VID/PID with different interface e.g MSC (first), then CDC (later) will possibly cause system error on PC.
 *
 * Auto ProductID layout's Bitmap:
 *   [MSB]       MIDI | HID | MSC | CDC          [LSB]
 */
#define _PID_MAP(itf, n)  ( (CFG_TUD_##itf) << (n) )
#define USB_PID           (0x4000 | _PID_MAP(CDC, 0) | _PID_MAP(MSC, 1) | _PID_MAP(HID, 2) | _PID_MAP(MIDI, 3) | _PID_MAP(VENDOR, 4) )
#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_MIDI_DESC_LEN)
#if CFG_TUSB_MCU == OPT_MCU_LPC175X_6X || CFG_TUSB_MCU == OPT_MCU_LPC177X_8X || CFG_TUSB_MCU == OPT_MCU_LPC40XX
  // LPC 17xx and 40xx endpoint type (bulk/interrupt/iso) are fixed by its number
  // 0 control, 1 In, 2 Bulk, 3 Iso, 4 In etc ...
  #define EPNUM_MIDI   0x02
#else
  #define EPNUM_MIDI   0x01
#endif

#define MIDI_TASK_INTERVALL 10

//extern "C" void tud_task();

/**
 * @brief Simple USB MIDI API
 * 
 */
class USBMidi {
    public: 
        // USBMidi is a singleton - we provide access to the instance
        static USBMidi& instance() {
            static USBMidi instance_;
            return instance_;
        }

        // starts the USB Midi processing
        void begin(int task_interval = MIDI_TASK_INTERVALL) {
            board_init();
            print("begin...");
            if (tusb_init()!=TUSB_ERROR_NONE){
               print("tusb_init failed");
            }

            timer.start(loopTask, task_interval);
            print("task started");
            active = true;
        }

        // stops the USB midi processing
        void stop(){
            timer.stop();
            active = false;
        }


        void write(uint8_t channel, uint8_t cmd, uint8_t note, uint8_t velocity){
            semaphore().aquire();
            tudi_midi_write24(channel, cmd, note, velocity);
            semaphore().release();
        }

        void noteOn(uint8_t note, uint8_t velocity=127, uint8_t channel=0){
            tudi_midi_write24(channel, 0x90, note, velocity);
        }

        void noteOff(uint8_t note, uint8_t channel = 0){
            tudi_midi_write24(channel, 0x80, note, 0);           
        }

        bool read(uint8_t &command, uint8_t &note, uint8_t &velocity, uint8_t &channel){
            uint8_t packet[4];
            semaphore().aquire();
            bool result =  tud_midi_receive(packet);
            semaphore().release();
            channel = packet[0];
            command = packet[1];
            note = packet[2];
            velocity = packet[3];
            return result;
        }

        bool send(uint8_t const packet[4]){
            semaphore().aquire();
            bool result = tud_midi_send(packet);
            semaphore().release();
            return result;
        }

        bool receive(uint8_t packet[4]){
            semaphore().aquire();
            bool result = tud_midi_receive(packet);
            semaphore().release();
            return result;
        }

        uint32_t available() {
            semaphore().aquire();
            uint32_t result = tud_midi_available();
            semaphore().release();
            return result;
        }

        // void setOnRxCallback(void (*fp)()){
        //      __setOnRxCallback(fp);
        // }

        bool isActive() {
            return active;
        }

        //--------------------------------------------------------------------+
        // Device callbacks
        //--------------------------------------------------------------------+

        // Invoked when device is mounted
        virtual void onMount(void){
            blinkInteval(BLINK_MOUNTED);
        }

        // Invoked when device is unmounted
        virtual void onUnmount(void) {
            blinkInteval(BLINK_NOT_MOUNTED);
        }

        // Invoked when usb bus is suspended
        // remote_wakeup_en : if host allow us  to perform remote wakeup
        // Within 7ms, device must draw an average of current less than 2.5 mA from bus
        virtual void onSuspend(bool remote_wakeup_en){
            (void) remote_wakeup_en;
            blinkInteval(BLINK_SUSPENDED);
        }

        // Invoked when usb bus is resumed
        virtual void onResume(void) {
            blinkInteval(BLINK_MOUNTED);
        }

        // returns a semaphore 
        static Semaphore &semaphore(){
            static Semaphore result;
            return result;
        }

    protected:
        bool active;
        TimerAlarmRepeating timer;
        uint16_t _desc_str[32];

        /* Blink pattern
        * - 250 ms  : device not mounted
        * - 1000 ms : device mounted
        * - 2500 ms : device is suspended
        */

        enum  {
            BLINK_NOT_MOUNTED = 250,
            BLINK_MOUNTED = 1000,
            BLINK_SUSPENDED = 2500,
        };

        enum {
            ITF_NUM_MIDI = 0,
            ITF_NUM_MIDI_STREAMING,
            ITF_NUM_TOTAL
        };

        USBMidi(){
            USBDevice dev = USBDevice::instance();
            // setup device descriptor
            dev.idVendor(0xCafe).idProduct(0x0001).bcdDevice(0x0100).manufacturer("TinyUSB").product("TinyUSB Device").serialNumber("123456";
            // setup configuration descriptor and others
            USBConfiguration* config = dev.setConfigurationDescriptor(TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN,TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100)); 
            config->addDescriptor(TUD_MIDI_DESCRIPTOR(ITF_NUM_MIDI, 0, EPNUM_MIDI, 0x80 | EPNUM_MIDI, 64));
        }

        //--------------------------------------------------------------------+
        // Task for processor 1 
        //--------------------------------------------------------------------+
        static bool loopTask(repeating_timer_t *rt){
            USBMidi usb = USBMidi::instance();
            USBMidi::semaphore().aquire();
            usb.doLoop();
            USBMidi::semaphore().release();
            return true;
        }

        // call this in your loop if begin was started on core 0
        virtual void doLoop() {
            tud_task(); // tinyusb device task
            led_blinking_task();
        }

        //--------------------------------------------------------------------+
        // BLINKING TASK
        //--------------------------------------------------------------------+
        virtual void led_blinking_task(void) {
            static uint32_t start_ms = 0;
            static bool led_state = false;

            // Blink every interval ms
            if ( board_millis() - start_ms < blinkInteval()) return; // not enough time
            start_ms += blinkInteval();

            board_led_write(led_state);
            led_state = 1 - led_state; // toggle
        }

        // setter/getter for blink intervall
        virtual uint32_t blinkInteval(uint32_t value=-1) {
            static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;
            if (value!=-1){
                blink_interval_ms = value;
            }
            return blink_interval_ms;
        }

        // prints a message to the uart
        void print(const char* str){
            board_uart_write(str, strlen(str));
        }

};

