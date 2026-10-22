
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
#define DISTANCE_THRESHOLD 10

// ==================================================
// LDR
// ==================================================

#define LDR_AO_PIN 34
#define LIGHT_THRESHOLD 700

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
#define SERVO_ACTION_TIME 1500

Servo myServo;

unsigned long servoStartTime = 0;
unsigned long servoCooldownStart = 0;

const unsigned long SERVO_COOLDOWN_TIME = 500;

bool servoCoolingDown = false;
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

void showOnLCD(bool isAutoMode);
void sendSensorData();
void updateServo();
bool canStartServo();
void startServo();

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
  if (param.asInt() == 1 && !autoMode) {
    if (canStartServo()) {
      startServo();
      Serial.println("Manual: Servo activated");
    } else {
      Serial.println("Manual: Servo busy or cooling down");
    }
  }
}


// ==================================================
// Setup
// ==================================================

void setup() {
  Serial.begin(115200);

  // Relay: ON
  pinMode(RELAY_IN_PIN, OUTPUT);
  digitalWrite(RELAY_IN_PIN, RELAY_ON);

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

  showOnLCD(autoMode);

  
bool objectDetected =
    distance >= 0 && distance < DISTANCE_THRESHOLD;

  if (autoMode) {
    // ทำงานเฉพาะตอนวัตถุเพิ่งเข้ามาในระยะ
    if (objectDetected &&
        !previousObjectDetected &&
        canStartServo()) {

      startServo();
      Serial.println("AUTO: Object detected");
    }

    // บันทึกสถานะทุกครั้ง เพื่อป้องกันการสั่งซ้ำ
    previousObjectDetected = objectDetected;

  } else {
    // Manual mode ไม่ให้ Ultrasonic ควบคุม Servo
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


bool canStartServo() {
  if (servoBusy) {
    return false;
  }

  if (servoCoolingDown) {
    if (millis() - servoCooldownStart < SERVO_COOLDOWN_TIME) {
      return false;
    }

    servoCoolingDown = false;
  }

  return true;
}

void startServo() {
  if (!canStartServo()) {
    return;
  }

  myServo.write(SERVO_ACTIVE_ANGLE);
  servoStartTime = millis();
  servoBusy = true;
}

void updateServo() {
  if (servoBusy &&
      millis() - servoStartTime >= SERVO_ACTION_TIME) {

    myServo.write(SERVO_HOME_ANGLE);
    servoBusy = false;

    // เริ่มนับเวลาพักหลังกลับตำแหน่งเริ่มต้น
    servoCooldownStart = millis();
    servoCoolingDown = true;

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


unsigned long lastLCDUpdate = 0;
const unsigned long LCD_INTERVAL = 250;

void showOnLCD(bool isAutoMode) {
  if (millis() - lastLCDUpdate < LCD_INTERVAL) {
    return;
  }

  lastLCDUpdate = millis();

  lcd.setCursor(0, 0);
  lcd.print("ESP32 SYSTEM    ");

  lcd.setCursor(0, 1);

  if (isAutoMode) {
    lcd.print("MODE: AUTO      ");
  } else {
    lcd.print("MODE: MANUAL    ");
  }
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

   // เงื่อนไขตรวจสอบค่าแสง
  if (currentLightRaw > LIGHT_THRESHOLD) {
    digitalWrite(RELAY_IN_PIN, RELAY_ON);
    Blynk.virtualWrite(V4, "Ball Detected");
  } else {
    digitalWrite(RELAY_IN_PIN, RELAY_OFF);
    Blynk.virtualWrite(V4, "No Ball Detected");
  }

  Serial.print("Distance: ");
  Serial.print(currentDistance);
  Serial.print(" cm | Light: ");
  Serial.println(currentLightRaw);
}
