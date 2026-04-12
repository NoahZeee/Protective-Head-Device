#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>


#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1

#define BTN_EMERGENCY 2
#define LED_PIN       23

// WiFi Credentials and Server Info
const char* WIFI_SSID     = "zaptop";
const char* WIFI_PASSWORD = "12345678";
const char* SERVER_IP     = "192.168.137.1";
const int   SERVER_PORT   = 11111;

// Vibration Motor Defines:
#define VIBRATION_PIN_1 12
#define VIBRATION_CHANNEL_1 0
#define VIBRATION_FREQUENCY 1000 // Frequency for PWM (Hz)
#define VIBRATION_RESOLUTION 8

#define TRIG_PIN 32
#define ECHO_PIN 33
#define MAX_ECHO_TIME 30000 // 30 ms timeout for ~5 meters

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

WiFiClient client;

void connect_wifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected! ESP32 IP: ");
  Serial.println(WiFi.localIP());
}

void connect_server() {
  Serial.print("Connecting to server...");
  while (!client.connect(SERVER_IP, SERVER_PORT)) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("Connected to server!");
}

void handle_incoming() {
  if (client.available()) {
    String msg = client.readStringUntil('\n');
    msg.trim();
    Serial.print("Received: ");
    Serial.println(msg);

    if (msg == "LED_ON") {
      digitalWrite(LED_PIN, HIGH);
      Serial.println("LED ON");
    } 
    else if (msg == "LED_OFF") {
      digitalWrite(LED_PIN, LOW);
      Serial.println("LED OFF");
    }
  }
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
    if (objectDistance < 0) {
        vibrateMotor(0); // Off
    } else if (objectDistance < 20) {
        vibrateMotor(255); // Max intensity
    } else if (objectDistance < 30) {
        vibrateMotor(200);
    } else if (objectDistance < 40) {
        vibrateMotor(128); // Medium intensity
    } else if (objectDistance < 50) {
        vibrateMotor(100);
    }else if (objectDistance < 70) {
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

  Serial.begin(115200);
  pinMode(BTN_EMERGENCY, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  connect_wifi();
  connect_server();
  Serial.println("Helmet ready!");
  delay(200);
}

void loop() {

    if (!client.connected()) {
    Serial.println("Lost connection, reconnecting...");
    connect_server();
  }


    if (digitalRead(BTN_EMERGENCY) == LOW) {
    Serial.println("Sending: EMERGENCY");
    client.println("EMERGENCY");
    delay(300);
  }


    float objectDistance = measureDistanceCM();
    vibrateCase(objectDistance);
    Serial.println(analogRead(2));
    float voltage = (analogRead(2))*(3.3/1024.0);

 float temperatureC = (voltage - 0.5) * 100 + 2;
  Serial.print(temperatureC); Serial.println(" degrees C");
   float temperatureF = (temperatureC * 9.0 / 5.0) + 32.0;
 Serial.print(temperatureF); Serial.println(" degrees F");
 if (objectDistance >= 0) {
        Serial.print("Distance: ");
        Serial.print(objectDistance);
        Serial.println(" cm");
    } else {
        Serial.println("Distance: timeout");
    }
 
   display.clearDisplay();

   
  display.setTextSize(3);
  display.setTextColor(WHITE);
  display.setCursor(0, 10);
  // Display static text
  display.println(temperatureF);
  display.setTextSize(2);
display.setCursor(0, 36);
  if (objectDistance >= 0) {
        display.print("Dist: ");
        display.print(objectDistance);
        display.println(" cm");
    } else {
        display.println("Dist: timeout");
    }
  display.display(); 


  
 handle_incoming();

    delay(100);
}

// put function definitions here: