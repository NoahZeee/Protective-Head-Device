#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Wire.h>
#include "bluetooth.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1

// Vibration Motor Defines:
#define VIBRATION_PIN_1 12
#define VIBRATION_CHANNEL_1 0
#define VIBRATION_FREQUENCY 1000 // Frequency for PWM (Hz)
#define VIBRATION_RESOLUTION 8

#define TRIG_PIN 32
#define ECHO_PIN 33
#define MAX_ECHO_TIME 30000 // 30 ms timeout for ~5 meters

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

BLEServer *pServer = NULL;
BLECharacteristic *pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint8_t txValue = 0;
int objectDistance = 60;

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

// TODO: change UUIDs
#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

class MyServerCallbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer *pServer)
    {
        deviceConnected = true;
    };
    void onDisconnect(BLEServer *pServer)
    {
        deviceConnected = false;
    }
};

class MyCallbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *pCharacteristic)
    {
        std::string rxValue = pCharacteristic->getValue().c_str();
        if (rxValue.length() > 0)
        {
            messageHandler(rxValue.c_str());
        }
    }
};

boolean setupBluetooth()
{
    // Create the BLE Device
    BLEDevice::init("UART Service");

    // Create the BLE Server
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    // Create the BLE Service
    BLEService *pService = pServer->createService(SERVICE_UUID);

    // Create a BLE Characteristic
    pTxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_TX,
        BLECharacteristic::PROPERTY_NOTIFY);

    pTxCharacteristic->addDescriptor(new BLE2902());

    BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_RX,
        BLECharacteristic::PROPERTY_WRITE);

    pRxCharacteristic->setCallbacks(new MyCallbacks());

    // Start the service
    pService->start();

    // Start advertising
    pServer->getAdvertising()->start();
    Serial.println("Waiting a client connection to notify...");

    return true;
}

void loopBluetooth()
{
    if (deviceConnected)
    {
        // case: device connected
        std::string value = std::to_string(txValue);
        //Serial.println(value.c_str());
        //pTxCharacteristic->setValue(value.c_str());
        //pTxCharacteristic->notify(); 
       // txValue++;
        delay(10); // bluetooth stack will go into congestion, if too many packets are sent
    }
    // disconnecting
    if (!deviceConnected && oldDeviceConnected)
    {
        // case: device disconnected
        delay(500);                  // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising(); // restart advertising
        Serial.println("start advertising");
        oldDeviceConnected = deviceConnected;
    }
    // connecting
    if (deviceConnected && !oldDeviceConnected)
    {
        // do stuff here on first connect
        oldDeviceConnected = deviceConnected;
    }
    delay(1000);
}

void setupVibrationMotors() {
    // Configure PWM for vibration motors
    ledcSetup(VIBRATION_CHANNEL_1, VIBRATION_FREQUENCY, VIBRATION_RESOLUTION);
    ledcAttachPin(VIBRATION_PIN_1, VIBRATION_CHANNEL_1);
    
    // Start with motors off
    ledcWrite(VIBRATION_CHANNEL_1, 0);
}

void vibrateMotor(int intensity) {
    // intensity: 0-255 (0 = off, 255 = max)
    // duration: milliseconds
    ledcWrite(VIBRATION_CHANNEL_1, intensity);
}

void vibrateCase(int objectDistance) {
    if (objectDistance < 20) {
        vibrateMotor(255); // Max intensity
    } else if (objectDistance < 40) {
        vibrateMotor(128); // Medium intensity
    } else if (objectDistance < 70) {
        vibrateMotor(64); // Low intensity
    } else {
        vibrateMotor(0); // Off
    }
}

float measureDistanceCM()
{
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    unsigned long duration = pulseIn(ECHO_PIN, HIGH, MAX_ECHO_TIME);
    if (duration == 0)
    {
        return -1.0; // timeout / no echo
    }
    return (duration*.0343)/2;
}

void sendMessage(String message)
{
    pTxCharacteristic->setValue(message.c_str());
    pTxCharacteristic->notify();
}

void messageHandler(String message)
{
    // ADD YOUR CODE HERE
    Serial.println(message);
}

void setup() {
  // put your setup code here, to run once:
  setupVibrationMotors();
  Wire.setPins(4, 5);
  Wire.begin();
  pinMode(2, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);
  Serial.begin(9600);
  analogReadResolution(10); 
  setupBluetooth();
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  delay(200);
}

void loop() {
    float distanceCm = measureDistanceCM();
    objectDistance = (distanceCm >= 0) ? (int)distanceCm : objectDistance;
    vibrateCase(10);
    Serial.println(analogRead(2));
    float voltage = (analogRead(2))*(3.0/1024.0);

 float temperatureC = (voltage - 0.5) * 100 ;
  Serial.print(temperatureC); Serial.println(" degrees C");
   float temperatureF = (temperatureC * 9.0 / 5.0) + 32.0;
 Serial.print(temperatureF); Serial.println(" degrees F");
 if (distanceCm >= 0) {
        Serial.print("Distance: ");
        Serial.print(distanceCm);
        Serial.println(" cm");
    } else {
        Serial.println("Distance: timeout");
    }
 
   loopBluetooth();
  
   sendMessage(String(temperatureF));
   display.clearDisplay();

   
  display.setTextSize(3);
  display.setTextColor(WHITE);
  display.setCursor(0, 10);
  // Display static text
  display.println(temperatureF);
display.setCursor(0, 36);
  if (distanceCm >= 0) {
        display.print("Dist: ");
        display.print(distanceCm);
        display.println(" cm");
    } else {
        display.println("Dist: timeout");
    }
  display.display(); 


  


    delay(1000);
}

// put function definitions here: