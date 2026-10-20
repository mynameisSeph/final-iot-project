
#define BLYNK_TEMPLATE_ID "TMPL6LwRWfHCv"
#define BLYNK_TEMPLATE_NAME "ESP32 Ultrasonic"
#define BLYNK_AUTH_TOKEN "7XYJfw3Nilr5b5iG1oECWv-C7P_Sxsgc"

#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>

// ==================================================
// WiFi
// ==================================================

char ssid[] = "Net";
char pass[] = "621601431";

BlynkTimer timer;

// ==================================================
// LCD I2C
// ==================================================

#define SDA_PIN 22
#define SCL_PIN 21

LiquidCrystal_I2C lcd(0x27, 16, 2);

// ==================================================
// Ultrasonic
// ==================================================

#define TRIG_PIN 17
#define ECHO_PIN 18
#define DISTANCE_THRESHOLD 18

// ==================================================
// LDR
// ==================================================

#define LDR_AO_PIN 34
#define LIGHT_THRESHOLD 600

// ==================================================
// Relay (ยังไม่ได้เปิดใช้งานในโค้ดนี้)
// ==================================================

#define RELAY_IN_PIN 33
#define RELAY_ON LOW
#define RELAY_OFF HIGH

// ==================================================
// Servo
// ==================================================

#define SERVO_PIN 12
#define SERVO_HOME_ANGLE 0
#define SERVO_ACTIVE_ANGLE 90
#define SERVO_ACTION_TIME 500

Servo myServo;

// ==================================================
// Blynk Virtual Pins
// ==================================================

#define SERVO_MODE_PIN V2
#define MANUAL_SERVO_PIN V3

long currentDistance = -1;
int currentLightRaw = 0;

bool autoMode = true;
bool servoBusy = false;
bool previousObjectDetected = false;

unsigned long servoStartTime = 0;

// ==================================================
// Function Declarations
// ==================================================

void setupLCD();
void setupServo();
void setupUltrasonic();
void setupLightSensor();

long readUltrasonic();
long microsecondsToCentimeters(long microseconds);
int readLightSensor();

void showOnLCD(long distance, int lightRaw);
void sendSensorData();
void updateServo();

// ==================================================
// Blynk Callbacks
// ==================================================

// รับสถานะ Switch: ON = Auto, OFF = Manual
BLYNK_WRITE(V2) {
  autoMode = (param.asInt() == 1);

  // เปลี่ยนเป็น Manual ให้กลับตำแหน่งเริ่มต้น
  if (!autoMode) {
    myServo.write(SERVO_HOME_ANGLE);
    servoBusy = false;
    previousObjectDetected = false;
  }

  Serial.println(autoMode ? "Mode: AUTO" : "Mode: MANUAL");
}

// เมื่อเชื่อมต่อ Blynk ให้รับสถานะ Switch ที่บันทึกไว้
BLYNK_CONNECTED() {
  Blynk.syncVirtual(V2);
}

// ปุ่ม Push สำหรับสั่ง Servo ใน Manual
BLYNK_WRITE(V3) {
  if (param.asInt() == 1 && !autoMode && !servoBusy) {
    myServo.write(SERVO_ACTIVE_ANGLE);
    servoStartTime = millis();
    servoBusy = true;

    Serial.println("Manual: Servo activated");
  }
}

// ==================================================
// Setup
// ==================================================

void setup() {
  Serial.begin(115200);

  setupLCD();
  setupServo();
  setupUltrasonic();
  setupLightSensor();

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  timer.setInterval(1000L, sendSensorData);

  Serial.println("Blynk connected");
}

// ==================================================
// Main Loop
// ==================================================

void loop() {
  Blynk.run();
  timer.run();

  long distance = readUltrasonic();
  int lightRaw = readLightSensor();

  currentDistance = distance;
  currentLightRaw = lightRaw;

  showOnLCD(distance, lightRaw);

  bool objectDetected =
      distance >= 0 && distance < DISTANCE_THRESHOLD;

  if (autoMode) {
    // Auto: ทำงานเมื่อวัตถุเพิ่งเข้ามาในระยะ
    if (objectDetected &&
        !previousObjectDetected &&
        !servoBusy) {

      myServo.write(SERVO_ACTIVE_ANGLE);
      servoStartTime = millis();
      servoBusy = true;

      Serial.println("AUTO: Object detected");
    }

    previousObjectDetected = objectDetected;
  } else {
    // Manual: Ultrasonic ไม่ควบคุม Servo
    previousObjectDetected = false;
  }

  updateServo();
}

// ==================================================
// Servo Action
// ==================================================

void setupServo() {
  myServo.setPeriodHertz(50);
  myServo.attach(SERVO_PIN, 500, 2400);
  myServo.write(SERVO_HOME_ANGLE);
}

void updateServo() {
  if (servoBusy &&
      millis() - servoStartTime >= SERVO_ACTION_TIME) {

    myServo.write(SERVO_HOME_ANGLE);
    servoBusy = false;

    Serial.println("Servo returned to home");
  }
}

// ==================================================
// LCD
// ==================================================

void setupLCD() {
  Wire.begin(SDA_PIN, SCL_PIN);

  lcd.init();
  lcd.backlight();
  lcd.clear();
}

void showOnLCD(long distance, int lightRaw) {
  lcd.setCursor(0, 0);
  lcd.print("Dist: ");

  if (distance < 0) {
    lcd.print("Error");
  } else {
    lcd.print(distance);
    lcd.print(" cm");
  }

  lcd.print("       ");

  lcd.setCursor(0, 1);
  lcd.print("Light: ");
  lcd.print(lightRaw);
  lcd.print("       ");
}

// ==================================================
// Ultrasonic
// ==================================================

void setupUltrasonic() {
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  digitalWrite(TRIG_PIN, LOW);
}

long readUltrasonic() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);

  if (duration == 0) {
    return -1;
  }

  return microsecondsToCentimeters(duration);
}

long microsecondsToCentimeters(long microseconds) {
  return microseconds / 29 / 2;
}

// ==================================================
// LDR
// ==================================================

void setupLightSensor() {
  pinMode(LDR_AO_PIN, INPUT);
  analogReadResolution(12);
}

int readLightSensor() {
  return analogRead(LDR_AO_PIN);
}

// ==================================================
// Send Sensor Data to Blynk
// ==================================================

void sendSensorData() {
  if (currentDistance >= 0) {
    Blynk.virtualWrite(V0, currentDistance);
  } else {
    Blynk.virtualWrite(V0, -1);
  }

  Blynk.virtualWrite(V1, currentLightRaw);

  Serial.print("Distance: ");
  Serial.print(currentDistance);
  Serial.print(" cm | Light: ");
  Serial.println(currentLightRaw);
}
