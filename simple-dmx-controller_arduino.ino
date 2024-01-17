/*
    Created by Jeffrey Sloan
    sloanjeffreyj@gmail.com
    sloanjeffreyj.com
    Programmed specifically for the "SparkFun ESP32 Thing Plus"
    with the "SparkFun ESP32 Thing Plus DMX to LED Shield".

    MIT license:
    Permission is hereby granted, free of charge, to any person obtaining a copy of this software 
    and associated documentation files (the "Software"), to deal in the Software without restriction, 
    including without limitation the rights to use, copy, modify, merge, publish, distribute, 
    sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is 
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all copies or 
    substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING 
    BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, 
    DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <ArduinoJson.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <FS.h>
#include <FFat.h>
#include "GroupOperations.h"
#include "helpers.h"
#include <HardwareSerial.h>
#include <SparkFunDMX.h>
#include <SPI.h>

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/
#define SERVICE_UUID                  "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_OPERATION_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHARACTERISTIC_INFO_UUID      "d8b9c1a4-66ac-4d64-92d8-11b244a3cd90"

// Possible operations that remote device can perform. (RESTART_DEVICE not yet implemented and possibly can't be.)
#define RESTART_DEVICE_CODE   0
#define SET_INTENSITY_CODE    10
#define SET_CONFIG_CODE       11

// Expected positions in incoming csv's.
#define OPERATION_CODE_POSITION 0
#define GROUP_POSITION          1
#define NICKNAME_POSITION       2
#define INTENSITY_POSITION      3
#define CHANNEL_START_POSITION  4

// Built in LED pin.
#define BUILTIN_LED_PIN 13

// Max number of group structs. If you'd like to change this, alter the size of Json config at your own peril.
// The 'stringAlloc' is how many bytes are allocated for the rest of the each groups json doc.
// I do not recommend increasing "MAX_CHANNELS", as I found that changing the intensity of many more channels
// than 32 causes significant slowdown.
#define MAX_GROUPS   10
#define MAX_CHANNELS 32
#define MAX_CIRCUIT  512

SparkFunDMX     dmx;
GroupOperations groupOp;
helpers         helpers;

// Name of config file and config struct array used in ArduinoJson
const char *filename     = "/groupConfig";
GroupConfig *groupConfig = new GroupConfig;

// Used for converting std::string to const char * for some functions
char bufferCharArr[256];

// Used to provide notifications to client.
bool deviceConnected  = false;
bool configSent       = false;
// Used to space out notifications to avoid congestion.
int stackWaitInterval = 10;

// BLE Server variables.
BLEServer *pServer                  = NULL;
BLECharacteristic *groupInfo        = NULL;
BLECharacteristic *deviceOperations = NULL;
BLEAdvertising *pAdvertising;

// Variables used to keep a timer and refresh DMX signal.
long ledBlinkInterval  = 1000;
bool isLedBlinkOn      = false;
long previousMillis    = 0;
// DMX signal will time out on most systems if not refreshed here.
int dmxRefreshInterval = 30000;
bool shouldDmxUpdate   = false;


// Server callbacks used to send notifications to client.
class MyServerCallbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer *pServer)
    {
      Serial.println("Device connected.");
      deviceConnected = true;
      configSent = false;
      // Begin advertising again so that multiple devices can connect.
      pAdvertising->start();
    }
    void onDisconnect(BLEServer *pServer)
    {
      Serial.println("Device has disconnected.");
      deviceConnected = false;
      configSent = false;
      pAdvertising->start();
    };
};

// Provides information about group configuration to client.
class InfoCallbacks : public BLECharacteristicCallbacks
{
    void transmitGroupConfig(BLECharacteristic *groupInfo, int groupId)
    {
      String value = "";
      value = value + groupOp.stringifyGroup(groupConfig->group[groupId]);
      Serial.print("transmitValue= ");
      Serial.println(value);
      std::string transmitValue = value.c_str();
      groupInfo->setValue(transmitValue);
      groupInfo->notify();
    }

    void onRead(BLECharacteristic *groupInfo)
    {
      // Give the app a generous amount of time to properly subscribe.
      delay(200);
      for (int groupIterator = 0; groupIterator < MAX_GROUPS; groupIterator++)
      {
        delay(10);
        transmitGroupConfig(groupInfo, groupIterator);
      }
    }
};

// BLE Callbacks used for operations.
class OperationCallbacks : public BLECharacteristicCallbacks
{
    // Alter intensity of given group.
  public:
    bool intensityUpdate(int groupId, int newIntensity)
    {
      // Intensity cannot be greater than 255 and channel cannot be greater than 512 (the size of a DMX universe).
      if (0 > newIntensity > 255)
      {
        Serial.println("Illegal intensity value detected");
        return false;
      }

      for (int i = 0; i < MAX_CHANNELS; i++)
      {
        if (groupConfig->group[groupId].channel[i] <= 0)
        {
          Serial.print("Channel is <= 0: ");
          Serial.println(groupConfig->group[groupId].channel[i]);
          break;
        }
        dmx.write(groupConfig->group[groupId].channel[i], newIntensity);
      }

      if (newIntensity == 0)
      {
        shouldDmxUpdate = false;
      }
      else
      {
        shouldDmxUpdate = true;
      }
      dmx.update();
      Serial.println("Successfully updated intensity");
      
      // Save intensity for updating other clients if multiple are connected.
      groupConfig->group[groupId].intensity = newIntensity;
    }

    // Take incoming data, determine what the user is trying to do, and do it.
    void onWrite(BLECharacteristic *deviceOperations)
    {
      std::string str = deviceOperations->getValue();

      // DEBUG
      Serial.println("Incoming value:");
      for (int i = 0; i < str.length(); i++)
      {
        Serial.print(str[i]);
      }
      Serial.println("");
      Serial.println("*******");
      // DEBUG

      String bufferStr = helpers.extractCsv(str, OPERATION_CODE_POSITION);
      int operation = bufferStr.toInt();
      switch (operation)
      {
        case RESTART_DEVICE_CODE:
          Serial.println("Restarting device..");
          break;
        // Extract data to alter GroupConfig struct.
        case SET_CONFIG_CODE:
          {
            Serial.println("Altering group config..");
            // Extract groupId
            bufferStr = helpers.extractCsv(str, GROUP_POSITION);
            int groupId = bufferStr.toInt();
            Serial.print("Altering group: ");
            Serial.println(groupId);

            // Extract and set new nickname to config struct.
            String newNickname = helpers.extractCsv(str, NICKNAME_POSITION);
            groupConfig->group[groupId].nickname = newNickname;

            // Extract and set channels (non-channels are set as '0')
            helpers.csvChansToArr(str, groupId, groupConfig);
            groupOp.writeConfig(FFat, filename);
            groupOp.printConfig(FFat, filename);
            break;
          }
        case SET_INTENSITY_CODE:
          {
            bufferStr = helpers.extractCsv(str, GROUP_POSITION);
            int groupId = bufferStr.toInt();
            bufferStr = helpers.extractCsv(str, INTENSITY_POSITION);
            int newIntensity = bufferStr.toInt();
            intensityUpdate(groupId, newIntensity);
            break;
          }
        default:
          Serial.println("Default case activated");
      }
    }
};

//// Only here for Walt lights.
//bool intensityUpdate(int groupId, int newIntensity)
//{
//  // Intensity cannot be greater than 255 and channel cannot be greater than 512 (the size of a DMX universe).
//  if (0 > newIntensity > 255)
//  {
//    Serial.println("Illegal intensity value detected");
//    return false;
//  }
//
//  for (int i = 0; i < MAX_CHANNELS; i++)
//  {
//    if (groupConfig->group[groupId].channel[i] <= 0)
//    {
//      Serial.print("Channel is <= 0: ");
//      Serial.println(groupConfig->group[groupId].channel[i]);
//      break;
//    }
//    dmx.write(groupConfig->group[groupId].channel[i], newIntensity);
//  }
//
//  if (newIntensity == 0)
//  {
//    shouldDmxUpdate = false;
//  }
//  else
//  {
//    shouldDmxUpdate = true;
//  }
//  dmx.update();
//  Serial.println("Successfully updated intensity");
//};

void setup()
{
  Serial.begin(115200);

  // Setup FFat storage.
  if (!FFat.begin(true))
  {
    Serial.println("FFat Mount Failed");
    return;
  }
  Serial.println("File system mounted");
  groupConfig = groupOp.readConfig(FFat, filename);

  // Initiate BLE functionality.
  BLEDevice::init("MVHS DMX 2");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Client can use to alter group configuration and set channel intensity.
  deviceOperations = pService->createCharacteristic(
                       CHARACTERISTIC_OPERATION_UUID,
                       BLECharacteristic::PROPERTY_READ |
                       BLECharacteristic::PROPERTY_WRITE |
                       BLECharacteristic::PROPERTY_NOTIFY);
  deviceOperations->setCallbacks(new OperationCallbacks());
  BLEDescriptor *pClientCharacteristicConfigDescriptor = new BLEDescriptor((uint16_t)0x2902);
  deviceOperations->addDescriptor(pClientCharacteristicConfigDescriptor);

  groupInfo = pService->createCharacteristic(
                CHARACTERISTIC_INFO_UUID,
                BLECharacteristic::PROPERTY_READ |
                BLECharacteristic::PROPERTY_WRITE |
                BLECharacteristic::PROPERTY_NOTIFY
              );
  groupInfo->setCallbacks(new InfoCallbacks());
  groupInfo->addDescriptor(new BLE2902());

  pService->start();
  pAdvertising = pServer->getAdvertising();
  pAdvertising->setScanResponse(false);
  // Functions that supposedly help with iPhone connections issue.
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);

  pAdvertising->start();

  dmx.initWrite(MAX_CHANNELS);
  BLEDevice::setMTU(500);

  // Setup built in LED for blinking.
  pinMode(BUILTIN_LED_PIN, OUTPUT);

  Serial.println("Successfully booted");

  // Temp group intensity for Walt.
//  intensityUpdate(0, 125);
//  intensityUpdate(1, 125);
//  intensityUpdate(2, 125);
//  delay(500);
//  intensityUpdate(0, 255);
//  intensityUpdate(1, 255);
//  intensityUpdate(2, 255);
//  delay(500);
}

void loop()
{
  // Keeps time to blink running LED and refresh DMX signal.
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis > ledBlinkInterval)
  {
    previousMillis = currentMillis;
    if (isLedBlinkOn)
    {
      digitalWrite(BUILTIN_LED_PIN, LOW);
      isLedBlinkOn = false;
    }
    else
    {
      digitalWrite(BUILTIN_LED_PIN, HIGH);
      isLedBlinkOn = true;
    }
  };


  // Refresh DMX signal based on set interval (default 30 seconds).
  if (currentMillis % dmxRefreshInterval == 0 && shouldDmxUpdate)
  {
    previousMillis = currentMillis;
    dmx.update();
    Serial.println("Refreshed DMX signal.");
  }
  else if (currentMillis % dmxRefreshInterval == 0 && !shouldDmxUpdate)
  {
    Serial.println("Didn't refresh DMX signal.");
  }
}
