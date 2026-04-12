#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Defines
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
#define BTN_EMERGENCY 21
#define BTN_BACKUP    22
#define VIBRATION_PIN_1      12
#define VIBRATION_CHANNEL_1  0
#define VIBRATION_FREQUENCY  1000
#define VIBRATION_RESOLUTION 8
#define TRIG_PIN      33
#define ECHO_PIN      32
#define MAX_ECHO_TIME 30000
const char* WIFI_SSID     = "zaptop";
const char* WIFI_PASSWORD = "12345678";
const char* SERVER_IP     = "192.168.137.1";
const int   SERVER_PORT   = 11111;
WiFiClient client;
String        oledMessage    = "";
bool          showingMessage = false;
unsigned long msgTimer       = 0;
#define MSG_DISPLAY_MS 5000

// Wifi
void connect_wifi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
    Serial.println();
    Serial.print("Connected! IP: ");
    Serial.println(WiFi.localIP());
}

void connect_server() {
    Serial.print("Connecting to server...");
    while (!client.connect(SERVER_IP, SERVER_PORT)) { delay(1000); Serial.print("."); }
    Serial.println("Connected to server!");
}

void handle_incoming() {
    if (!client.available()) return;
    String msg = client.readStringUntil('\n');
    msg.trim();
    if (msg.length() == 0) return;
    Serial.print("Server says: "); Serial.println(msg);
    oledMessage    = msg;
    showingMessage = true;
    msgTimer       = millis();
}

// Vibration Motor
void setupVibrationMotors() {
    ledcSetup(VIBRATION_CHANNEL_1, VIBRATION_FREQUENCY, VIBRATION_RESOLUTION);
    ledcAttachPin(VIBRATION_PIN_1, VIBRATION_CHANNEL_1);
    ledcWrite(VIBRATION_CHANNEL_1, 0);
}

void vibrateMotor(int intensity) {
    ledcWrite(VIBRATION_CHANNEL_1, intensity);
}

void vibrateCase(float objectDistance) {
    if      (objectDistance < 0)   vibrateMotor(0);
    else if (objectDistance < 20)  vibrateMotor(255);
    else if (objectDistance < 30)  vibrateMotor(200);
    else if (objectDistance < 40)  vibrateMotor(128);
    else if (objectDistance < 50)  vibrateMotor(100);
    else if (objectDistance < 70)  vibrateMotor(64);
    else                           vibrateMotor(0);
}

// Ultrasonic Sensor
float measureDistanceCM() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(4);  
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(10);       

    unsigned long duration = pulseIn(ECHO_PIN, HIGH, MAX_ECHO_TIME);
    if (duration == 0) return -1.0f;

    float distance = (duration * 0.0343f) / 2.0f;
    if (distance > 400.0f) return -1.0f;
    return distance;
}

// OLED
void drawSensorData(float tempF, float distCM) {
    display.clearDisplay();

    display.setTextSize(3);
    display.setTextColor(WHITE);
    display.setCursor(0, 4);
    display.print(tempF, 1);
    display.setTextSize(1);
    display.print(" F");

    display.setTextSize(2);
    display.setCursor(0, 42);
    if (distCM >= 0) {
        display.print(distCM, 1);
        display.print("cm");
    } else {
        display.print("No echo");
    }

    display.display();
}

void drawMessage(String msg) {
    display.clearDisplay();

    display.fillRect(0, 0, SCREEN_WIDTH, 12, WHITE);
    display.setTextColor(BLACK);
    display.setCursor(2, 2);
    display.setTextSize(1);
    display.print(">> MSG FROM BASE");

    display.setTextColor(WHITE);
    display.setCursor(0, 16);
    int lineLen = 21;
    for (int i = 0; i < (int)msg.length(); i += lineLen) {
        display.println(msg.substring(i, min(i + lineLen, (int)msg.length())));
    }

    display.display();
}


// Setup
void setup() {
    Serial.begin(9600);

    setupVibrationMotors();

    Wire.setPins(4, 5);
    Wire.begin();

    pinMode(BTN_EMERGENCY, INPUT);
    pinMode(BTN_BACKUP,    INPUT);
    pinMode(TRIG_PIN,      OUTPUT);
    pinMode(ECHO_PIN,      INPUT);
    digitalWrite(TRIG_PIN, LOW);
    pinMode(2, INPUT);

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("SSD1306 failed!");
        for(;;);
    }
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.println("Booting...");
    display.display();

    connect_wifi();
    connect_server();

    Serial.println("Helmet ready!");
}

// Loop
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

    if (digitalRead(BTN_BACKUP) == LOW) {
        Serial.println("Sending: BACKUP");
        client.println("BACKUP");
        delay(300);
    }

    Serial.println(analogRead(2));
 float voltage = (analogRead(2))*(3.3/1024.0);

 float tempC = (voltage - 0.5) * 100 ;
  Serial.print(tempC); Serial.println(" degrees C");
   float tempF = (tempC * 9.0 / 5.0) + 32.0;
 Serial.print(tempF); Serial.println(" degrees F");

    float distCM = measureDistanceCM();
    vibrateCase(distCM);

    if (distCM >= 0) {
        Serial.print("Distance: "); Serial.print(distCM); Serial.println(" cm");
    } else {
        Serial.println("Distance: timeout");
    }

    handle_incoming();

    if (showingMessage) {
        drawMessage(oledMessage);
        if (millis() - msgTimer > MSG_DISPLAY_MS) {
            showingMessage = false;
        }
    } else {
        drawSensorData(tempF, distCM);
    }

    delay(100);
}