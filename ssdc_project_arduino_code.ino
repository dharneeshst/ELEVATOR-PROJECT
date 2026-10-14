#include <WiFi.h>
#include <HTTPClient.h>
#include <ESP32Servo.h>

// ==================================================
// WI-FI & LOCAL DASHBOARD SERVER CONFIGURATION
// ==================================================
const char* ssid = "CIT-CAMPUS";         // Wi-Fi Name (SSID)
const char* password = "CIT@1956";       // Wi-Fi Password

// Direct Local HTTP API Endpoint on laptop (Updated to Active IP: 192.168.181.98)
const char* local_server_api = "http://192.168.181.98:3000/api/telemetry";

// ==================================================
// HARDWARE & SERVO DEFINITIONS
// ==================================================
Servo liftServo;

const int RED_LED = 12;         // UP Floor LED (GPIO 12)
const int TOP_BUTTON = 14;      // UP Push Button (GPIO 14)

const int GREEN_LED = 27;       // DOWN Floor LED (GPIO 27)
const int BOTTOM_BUTTON = 26;   // DOWN Push Button (GPIO 26)

const int SERVO_PIN = 13;       // Servo Signal (GPIO 13)
const int BUZZER_PIN = 15;      // Buzzer Signal (GPIO 15)

const int GROUND_ANGLE = 100;   // Floor 0 position (100 degrees)
const int TOP_ANGLE = 0;        // Floor 1 position (0 degrees)

const int SERVO_DELAY = 50;     // Speed delay per degree
const int BUZZER_TIME = 1000;   // Buzzer beep time (1 second)

int currentAngle = GROUND_ANGLE;
unsigned long lastHeartbeat = 0;

// Forward Declaration
void sendTelemetry(String status, String operation, float torque);

// ==================================================
// NON-BLOCKING TELEMETRY SENDER
// ==================================================
void sendTelemetry(String status, String operation, float torque) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.setTimeout(250); // 250ms timeout - zero lag
    http.begin(local_server_api);
    http.addHeader("Content-Type", "application/json");
    
    String jsonPayload = "{\"status\":\"" + status + "\",\"operation\":\"" + operation + "\",\"torque\":" + String(torque, 2) + "}";
    int httpResponseCode = http.POST(jsonPayload);
    
    if (httpResponseCode > 0) {
      Serial.println("📡 Telemetry Sent: " + jsonPayload);
    } else {
      Serial.println("⚠️ HTTP Telemetry Error: " + String(httpResponseCode));
    }
    http.end();
  }
}

// ==================================================
// SETUP
// ==================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\n=================================================");
  Serial.println("🚀 BOOTING CIT LIFT HARDWARE SYSTEM...");
  Serial.println("=================================================");

  // Output Pins
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  // Input Pins (Pullup)
  pinMode(TOP_BUTTON, INPUT_PULLUP);
  pinMode(BOTTOM_BUTTON, INPUT_PULLUP);

  // Servo Setup
  liftServo.setPeriodHertz(50);
  liftServo.attach(SERVO_PIN, 500, 2400);
  liftServo.write(GROUND_ANGLE);
  currentAngle = GROUND_ANGLE;

  Serial.println("✅ Servo Initialized at GROUND ANGLE (100°)");

  // Wi-Fi Connection
  Serial.print("📡 Connecting to Wi-Fi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  int wifiWait = 0;
  while (WiFi.status() != WL_CONNECTED && wifiWait < 12) {
    delay(250);
    Serial.print(".");
    wifiWait++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Wi-Fi Connected!");
    Serial.print("📍 ESP32 IP: ");
    Serial.println(WiFi.localIP());
    sendTelemetry("ON", "LIFT IDLE AT FLOOR 0", 0.00);
  } else {
    Serial.println("\n⚠️ Wi-Fi Connecting in background...");
  }
}

// ==================================================
// MAIN LOOP
// ==================================================
void loop() {

  // Read Push Button States
  int topBtnState = digitalRead(TOP_BUTTON);
  int bottomBtnState = digitalRead(BOTTOM_BUTTON);

  // Status monitor every 3 seconds
  unsigned long now = millis();
  if (now - lastHeartbeat > 3000) {
    lastHeartbeat = now;
    if (WiFi.status() == WL_CONNECTED) {
      sendTelemetry("ON", currentAngle == TOP_ANGLE ? "LIFT AT FLOOR 1" : "LIFT IDLE AT FLOOR 0", 0.00);
    }
  }

  // ==================================================
  // 1. UP MOVEMENT (GROUND -> TOP FLOOR)
  // ==================================================
  if (topBtnState == LOW && currentAngle > TOP_ANGLE) {
    Serial.println("\n🔘 TOP BUTTON PRESSED -> MOVING UP!");

    // Turn ON RED LED while moving UP
    digitalWrite(RED_LED, HIGH);
    digitalWrite(GREEN_LED, LOW);

    // Send Telemetry: UP Movement Started
    sendTelemetry("ON", "UP FROM FLOOR '0'", 50.85);

    // Smooth Servo Movement (100° -> 0°)
    for (int angle = currentAngle; angle >= TOP_ANGLE; angle--) {
      liftServo.write(angle);
      delay(SERVO_DELAY);
    }
    currentAngle = TOP_ANGLE;

    // Arrival Buzzer (1 Second)
    digitalWrite(BUZZER_PIN, HIGH);
    delay(BUZZER_TIME);
    digitalWrite(BUZZER_PIN, LOW);

    // Turn OFF LEDs on arrival
    digitalWrite(RED_LED, LOW);
    digitalWrite(GREEN_LED, LOW);

    // Send Telemetry: Arrived at Floor 1
    sendTelemetry("ON", "REACHED FLOOR '1'", 0.00);
    Serial.println("✅ Arrived at FLOOR 1!");
  }

  // ==================================================
  // 2. DOWN MOVEMENT (TOP -> GROUND FLOOR)
  // ==================================================
  if (bottomBtnState == LOW && currentAngle < GROUND_ANGLE) {
    Serial.println("\n🔘 BOTTOM BUTTON PRESSED -> MOVING DOWN!");

    // Turn ON GREEN LED while moving DOWN
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(RED_LED, LOW);

    // Send Telemetry: DOWN Movement Started
    sendTelemetry("ON", "DOWN FROM FLOOR '1'", -50.85);

    // Smooth Servo Movement (0° -> 100°)
    for (int angle = currentAngle; angle <= GROUND_ANGLE; angle++) {
      liftServo.write(angle);
      delay(SERVO_DELAY);
    }
    currentAngle = GROUND_ANGLE;

    // Arrival Buzzer (1 Second)
    digitalWrite(BUZZER_PIN, HIGH);
    delay(BUZZER_TIME);
    digitalWrite(BUZZER_PIN, LOW);

    // Turn OFF LEDs on arrival
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, LOW);

    // Send Telemetry: Arrived at Floor 0
    sendTelemetry("ON", "REACHED FLOOR '0'", 0.00);
    Serial.println("✅ Arrived at FLOOR 0!");
  }

  delay(20);
}