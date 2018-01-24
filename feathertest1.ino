/*********************************************************************
 This is an example for our nRF51822 based Bluefruit LE modules

 Pick one up today in the adafruit shop!

 Adafruit invests time and resources providing this open source code,
 please support Adafruit and open-source hardware by purchasing
 products from Adafruit!

 MIT license, check LICENSE for more information
 All text above, and the splash screen below must be included in
 any redistribution
*********************************************************************/

/*
  This example shows how to send HID (keyboard/mouse/etc) data via BLE
  Note that not all devices support BLE keyboard! BLE Keyboard != Bluetooth Keyboard
*/

#include <Arduino.h>
#include <SPI.h>
#include "Adafruit_BLE.h"
#include "Adafruit_BluefruitLE_SPI.h"
#include "Adafruit_BluefruitLE_UART.h"

#include "BluefruitConfig.h"

#if SOFTWARE_SERIAL_AVAILABLE
  #include <SoftwareSerial.h>
#endif

/*=========================================================================
    APPLICATION SETTINGS

    FACTORYRESET_ENABLE       Perform a factory reset when running this sketch
   
                              Enabling this will put your Bluefruit LE module
                              in a 'known good' state and clear any config
                              data set in previous sketches or projects, so
                              running this at least once is a good idea.
   
                              When deploying your project, however, you will
                              want to disable factory reset by setting this
                              value to 0.  If you are making changes to your
                              Bluefruit LE device via AT commands, and those
                              changes aren't persisting across resets, this
                              is the reason why.  Factory reset will erase
                              the non-volatile memory where config data is
                              stored, setting it back to factory default
                              values.
       
                              Some sketches that require you to bond to a
                              central device (HID mouse, keyboard, etc.)
                              won't work at all with this feature enabled
                              since the factory reset will clear all of the
                              bonding data stored on the chip, meaning the
                              central device won't be able to reconnect.
    MINIMUM_FIRMWARE_VERSION  Minimum firmware version to have some new features
    -----------------------------------------------------------------------*/
    #define FACTORYRESET_ENABLE         0
    #define MINIMUM_FIRMWARE_VERSION    "0.6.6"
/*=========================================================================*/


// Create the bluefruit object, either software serial...uncomment these lines
/*
SoftwareSerial bluefruitSS = SoftwareSerial(BLUEFRUIT_SWUART_TXD_PIN, BLUEFRUIT_SWUART_RXD_PIN);

Adafruit_BluefruitLE_UART ble(bluefruitSS, BLUEFRUIT_UART_MODE_PIN,
                      BLUEFRUIT_UART_CTS_PIN, BLUEFRUIT_UART_RTS_PIN);
*/

/* ...or hardware serial, which does not need the RTS/CTS pins. Uncomment this line */
// Adafruit_BluefruitLE_UART ble(BLUEFRUIT_HWSERIAL_NAME, BLUEFRUIT_UART_MODE_PIN);

/* ...hardware SPI, using SCK/MOSI/MISO hardware SPI pins and then user selected CS/IRQ/RST */
Adafruit_BluefruitLE_SPI ble(BLUEFRUIT_SPI_CS, BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);

/* ...software SPI, using SCK/MOSI/MISO user-defined SPI pins and then user selected CS/IRQ/RST */
//Adafruit_BluefruitLE_SPI ble(BLUEFRUIT_SPI_SCK, BLUEFRUIT_SPI_MISO,
//                             BLUEFRUIT_SPI_MOSI, BLUEFRUIT_SPI_CS,
//                             BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);

// A small helper
void error(const __FlashStringHelper*err) {
  Serial.println(err);
  while (1);
}


unsigned long lastStart = 10000;
unsigned long lastConnectCheck = 0;
int maxQueueSize = 50;
char queue[50] = {0};
int insertIndex = 0;
int removeIndex = 0;
unsigned long nextQueueInsert = millis();

/**************************************************************************/
/*!
    @brief  Sets up the HW an the BLE module (this function is called
            automatically on startup)
*/
/**************************************************************************/
void setup(void)
{
  while (!Serial);  // required for Flora & Micro
  delay(500);

  Serial.begin(115200);
  Serial.println(F("Adafruit Bluefruit HID Keyboard Example"));
  Serial.println(F("---------------------------------------"));

  /* Initialise the module */
  Serial.print(F("Initialising the Bluefruit LE module: "));

  if ( !ble.begin(true) )
  {
    error(F("Couldn't find Bluefruit, make sure it's in CoMmanD mode & check wiring?"));
  }
  Serial.println( F("OK!") );

  if ( FACTORYRESET_ENABLE )
  {
    /* Perform a factory reset to make sure everything is in a known state */
    Serial.println(F("Performing a factory reset: "));
    if ( ! ble.factoryReset() ){
      error(F("Couldn't factory reset"));
    }
  }

  /* Disable command echo from Bluefruit */
  ble.echo(false);

  Serial.println("Requesting Bluefruit info:");
  /* Print Bluefruit information */
  ble.info();

  /* Change the device name to make it easier to find */
  Serial.println(F("Setting device name to 'Bluefruit Keyboard': "));
  if (! ble.sendCommandCheckOK(F( "AT+GAPDEVNAME=Bluefruit Keyboard" )) ) {
    error(F("Could not set device name?"));
  }

  /* Enable HID Service */
  Serial.println(F("Enable HID Service (including Keyboard): "));
  if ( ble.isVersionAtLeast(MINIMUM_FIRMWARE_VERSION) )
  {
    if ( !ble.sendCommandCheckOK(F( "AT+BleHIDEn=On" ))) {
      error(F("Could not enable Keyboard"));
    }
  }else
  {
    if (! ble.sendCommandCheckOK(F( "AT+BleKeyboardEn=On"  ))) {
      error(F("Could not enable Keyboard"));
    }
  }

  /* Add or remove service requires a reset */
  Serial.println(F("Performing a SW reset (service changes require a reset): "));
  if (! ble.reset() ) {
    error(F("Couldn't reset??"));
  }

  Serial.println();
  Serial.println(F("Go to your phone's Bluetooth settings to pair your device"));
  Serial.println(F("then open an application that accepts keyboard input"));

  Serial.println();
  Serial.println(F("Enter the character(s) to send:"));
  Serial.println(F("- \\r for Enter"));
  Serial.println(F("- \\n for newline"));
  Serial.println(F("- \\t for tab"));
  Serial.println(F("- \\b for backspace"));

  Serial.println();

  lastStart = millis() + 5000;
  memset(queue,0, maxQueueSize);
  nextQueueInsert = millis() + 1000;
}


//char *str = "Hello there.  This is a test.  I wonder long this will take to divide and conquer the small bits of robots.  Wow, this is kind of a long piece of text.  I hope this thing can keep up.";
//char *str = "abcdefghijklmnopqrstuvwxyz";
char *str = "Hello there.  This is a test.";
char* c = str;
bool isConnected = false;
int count = 0;
unsigned long lastQueueInsert = 0;
unsigned long lastBleAvailableCheck = 0;
bool keyDown = false;
bool sendBluetooth = true;
bool bleAvailable = true;
unsigned long lastReset = 0;

/**************************************************************************/
/*!
    @brief  Constantly poll for new command or response data
*/
/**************************************************************************/
void loop(void) {
    unsigned long now = millis();
    if (!isConnected) {
        if ((now - lastConnectCheck) > 2000) {
            Serial.println ("Checking to see if we're connected");
            bool connected = ble.isConnected();
            if (connected) {
                isConnected = true;
            }
            lastConnectCheck = now;
        }
    }
    if (isConnected) {
        /*
        Serial.print ("nextQueueInsert: ");
        Serial.print(nextQueueInsert);
        Serial.print(", now: ");
        Serial.print(now, DEC);
        Serial.print(", nextQueueInsert - now: ");
        Serial.println (nextQueueInsert - now);
        */
        
        // put a character into the queue. the random() line at the bottom
        // of this section specifies how long until the next character is
        // put into the queue.
        if ((now - lastQueueInsert) > nextQueueInsert) {
            //Serial.print (nextQueueInsert);
            //Serial.println (" inserting");
            queue[insertIndex] = c[0];
            insertIndex++;
            if (insertIndex >= maxQueueSize) {
                insertIndex = 0;
            }
            *c++;
            if (*c == 0) {
                c = str;
                queue[insertIndex] = '\n';
                insertIndex++;
                if (insertIndex >= maxQueueSize) {
                    insertIndex = 0;
                }
            }
            lastQueueInsert = now;
            nextQueueInsert = random(30,250);
        }

        // wait 10ms after sending the "key down" sequence to send a "key up". 
        if ((keyDown) && ((now - lastStart) > 10)) {
            Serial.print(now);
            Serial.print(" / ");
            Serial.print (now - lastStart);
            Serial.println (": Sending keyUp");
            if (sendBluetooth) {
                ble.println("AT+BLEKEYBOARDCODE=00-00");
            }
            lastStart = now;
            keyDown = false;
        }

        // Try to see if BLE is available.
        if ((now - lastBleAvailableCheck) > 100) {
            Serial.print(now);
            Serial.print(": Checking BLE Availability");

            // Weird. set bleAvailable to true if time < 10s because ble.available always returns false if you haven't sent anything through it first.
            if (ble.available() || (now < 10000)) {
                bleAvailable = true;
                Serial.print(". BLE Available");
            } else {
                bleAvailable = false;
                Serial.print(". BLE UNAvailable");
                if ((now - lastReset) > 5000) {
                    ble.info();
                    lastReset = now;
                    bleAvailable = true;
                }
            }
            Serial.println();
            lastBleAvailableCheck = now;
        }

/*
        Serial.println("Checking available");
        bool avail = ble.available();
        Serial.print("Available is: ");
        Serial.print(avail);
        */

        // if there's a character in the queue, and if it's time, and there's not a key currently down, and ble is
        // available, pop a character off the queue and send it via
        // bluetooth.
        if ((!keyDown) && (bleAvailable) && (removeIndex != insertIndex) && ((now - lastStart) > 150)) {
            Serial.print("removeIndex: ");
            Serial.print(removeIndex);
            Serial.print(", insertIndex: ");
            Serial.print(insertIndex);
            Serial.print(", now: ");
            Serial.print(now);
            Serial.print(", lastStart: ");
            Serial.print(lastStart);
            Serial.print(" / ");
            Serial.print (now - lastStart);
            lastStart = now;
            if (removeIndex == insertIndex) {
                Serial.println(": timeout reached, but not dirty.  Skipping");
            } else {
                //uint8_t modifier = (queue[removeIndex] >> 8) & 0xFF;
                //uint8_t charCode = queue[removeIndex] & 0xFF;
                char charToSend = queue[removeIndex];
                uint8_t modifier = 0;
                uint8_t keyCode = 0;
                if (isupper(charToSend)) {
                    modifier |= 0x02;
                }

                Serial.print (": sending ");
                Serial.print (removeIndex);
                Serial.print (" characters: '");
                Serial.print(queue[removeIndex]);
                Serial.print("', modifier: 0x");
                Serial.print(modifier, HEX);
                Serial.print(", hid code: " );
                Serial.print(keyCode, HEX);
                Serial.println();

                removeIndex++;
                if (removeIndex >= maxQueueSize) {
                    removeIndex = 0;
                }

                if (sendBluetooth) {
                    ble.print("AT+BLEKEYBOARDCODE=");
                    if (modifier < 16) {
                        ble.print("0");
                    }
                    ble.print(modifier);
                    ble.print("-00-");
                    if (charToSend == ' ') {
                        ble.print("2C");
                    } else if (charToSend  == '.') {
                        ble.print("37");
                    } else if (charToSend  == '\n') {
                        ble.print("28");
                    } else if (isupper(charToSend)) {
                        int hidCode = (int)charToSend - 61;
                        if (hidCode < 16) {
                            ble.print("0");
                        }
                        ble.print(hidCode, HEX);
                    } else {
                        int hidCode = (int)charToSend - 93;
                        if (hidCode < 16) {
                            ble.print("0");
                        }
                        ble.print(hidCode, HEX);
                    }
                    ble.println("");
                }
                keyDown = true;
            }
            //ble.println("00-00");
        }
    }
}
