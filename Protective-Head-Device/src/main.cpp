#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>


// ── Display ───────────────────────────────────────────────────────────────
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ── Pins ──────────────────────────────────────────────────────────────────
#define BTN_EMERGENCY 2
#define BTN_BACKUP 3
#define LED_PIN       23
#define TRIG_PIN 32
#define ECHO_PIN 33
#define VIBRATION_PIN_1 12

// ── PWM ───────────────────────────────────────────────────────────────────
#define VIBRATION_CHANNEL_1 0
#define VIBRATION_FREQUENCY 1000 // Frequency for PWM (Hz)
#define VIBRATION_RESOLUTION 8


// ── WiFi / Server ─────────────────────────────────────────────────────────
const char* WIFI_SSID     = "zaptop";
const char* WIFI_PASSWORD = "12345678";
const char* SERVER_IP     = "192.168.137.1";
const int   SERVER_PORT   = 11111;
WiFiClient client;

#define MAX_ECHO_TIME 30000 // 30 ms timeout for ~5 meters

// ── State ─────────────────────────────────────────────────────────────────
String oledMessage      = "";   // message received from server
bool   showingMessage   = false;
unsigned long msgTimer  = 0;
#define MSG_DISPLAY_MS  5000    // show message for 5 seconds

// ── WiFi ──────────────────────────────────────────────────────────────────
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
  Serial.println("Connected!");
}

// ── Vibration ─────────────────────────────────────────────────────────────
void setupVibrationMotors() {
  ledcSetup(VIBRATION_CHANNEL_1, VIBRATION_FREQUENCY, VIBRATION_RESOLUTION);
  ledcAttachPin(VIBRATION_PIN_1, VIBRATION_CHANNEL_1);
  ledcWrite(VIBRATION_CHANNEL_1, 0);
}

void vibrateMotor(int intensity) {
  ledcWrite(VIBRATION_CHANNEL_1, intensity);
}

void vibrateCase(float objectDistance) {
  if (objectDistance < 0)        vibrateMotor(0);
  else if (objectDistance < 20)  vibrateMotor(255);
  else if (objectDistance < 30)  vibrateMotor(200);
  else if (objectDistance < 40)  vibrateMotor(128);
  else if (objectDistance < 50)  vibrateMotor(100);
  else if (objectDistance < 70)  vibrateMotor(64);
  else                           vibrateMotor(0);
}

// ── Ultrasonic ────────────────────────────────────────────────────────────
float measureDistanceCM() {
  digitalWrite(TRIG_PIN, LOW);  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  unsigned long duration = pulseIn(ECHO_PIN, HIGH, MAX_ECHO_TIME);
  if (duration == 0) return -1.0;
  return (duration * 0.0343) / 2.0;
}

// ── Incoming TCP messages ─────────────────────────────────────────────────
void handle_incoming() {
  if (!client.available()) return;

  String msg = client.readStringUntil('\n');
  msg.trim();
  if (msg.length() == 0) return;

  Serial.print("Received from server: ");
  Serial.println(msg);

  // LED commands
  if (msg == "LED_ON") {
    digitalWrite(LED_PIN, HIGH);
    return;
  }
  if (msg == "LED_OFF") {
    digitalWrite(LED_PIN, LOW);
    return;
  }

  // Everything else → display on OLED for 5 seconds
  oledMessage    = msg;
  showingMessage = true;
  msgTimer       = millis();
}

// ── OLED draw ─────────────────────────────────────────────────────────────
void drawSensorData(float tempF, float distCM) {
  display.clearDisplay();

  // Temperature (big)
  display.setTextSize(3);
  display.setTextColor(WHITE);
  display.setCursor(0, 4);
  display.print(tempF, 1);
  display.setTextSize(1);
  display.print(" F");

  // Distance
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
  display.setTextSize(1);
  display.setTextColor(WHITE);

  // Header
  display.fillRect(0, 0, SCREEN_WIDTH, 12, WHITE);
  display.setTextColor(BLACK);
  display.setCursor(2, 2);
  display.print(">> MSG FROM BASE");

  // Message body — word wrap
  display.setTextColor(WHITE);
  display.setCursor(0, 16);
  display.setTextSize(1);

  // Simple word wrap at ~21 chars per line
  int lineLen = 21;
  for (int i = 0; i < (int)msg.length(); i += lineLen) {
    display.println(msg.substring(i, min(i + lineLen, (int)msg.length())));
  }

  display.display();
}

// ── Setup ─────────────────────────────────────────────────────────────────
void setup() {
  setupVibrationMotors();

  Wire.setPins(4, 5);
  Wire.begin();

  pinMode(BTN_EMERGENCY, INPUT_PULLUP);
  pinMode(BTN_BACKUP,    INPUT_PULLUP);
  pinMode(LED_PIN,       OUTPUT);
  pinMode(TRIG_PIN,      OUTPUT);
  pinMode(ECHO_PIN,      INPUT);
  digitalWrite(LED_PIN,  LOW);
  digitalWrite(TRIG_PIN, LOW);

  Serial.begin(115200);
  analogReadResolution(10);

  // OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 not found!");
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

// ── Loop ──────────────────────────────────────────────────────────────────
void loop() {

  // Reconnect if dropped
  if (!client.connected()) {
    Serial.println("Lost connection, reconnecting...");
    connect_server();
  }

  // ── Button: EMERGENCY ──
  if (digitalRead(BTN_EMERGENCY) == LOW) {
    Serial.println("Sending: EMERGENCY");
    client.println("EMERGENCY");
    delay(300);
  }

  // ── Button: BACKUP ──
  if (digitalRead(BTN_BACKUP) == LOW) {
    Serial.println("Sending: BACKUP");
    client.println("BACKUP");
    delay(300);
  }

  // ── Sensors ──
  float distCM = measureDistanceCM();
  vibrateCase(distCM);

  int raw       = analogRead(2);
  float voltage = raw * (3.3f / 1024.0f);
  float tempC   = (voltage - 0.5f) * 100.0f + 2.0f;
  float tempF   = (tempC * 9.0f / 5.0f) + 32.0f;

  // ── Incoming messages ──
  handle_incoming();

  // ── OLED: show message or sensor data ──
  if (showingMessage) {
    drawMessage(oledMessage);
    if (millis() - msgTimer > MSG_DISPLAY_MS) {
      showingMessage = false;   // revert to sensor data after 5s
    }
  } else {
    drawSensorData(tempF, distCM);
  }

  delay(100);
}
