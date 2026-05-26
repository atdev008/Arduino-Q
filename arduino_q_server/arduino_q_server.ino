/*
 * Badminton AI Coach - Arduino Q (ESP32-S3) Web Server
 * 
 * ทำหน้าที่:
 * 1. เปิด WiFi Access Point
 * 2. เสิร์ฟ HTML5 Dashboard จาก LittleFS
 * 3. ส่งข้อมูล IMU ผ่าน WebSocket แบบ real-time
 * 4. คำนวณมุมข้อศอกจาก Gyro + Accel (Complementary Filter)
 * 
 * Libraries ที่ต้องติดตั้ง:
 * - ESPAsyncWebServer (https://github.com/me-no-dev/ESPAsyncWebServer)
 * - AsyncTCP (https://github.com/me-no-dev/AsyncTCP)
 * - LittleFS (มากับ ESP32 core)
 */

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <Wire.h>
#include <math.h>

// ===== WiFi AP Settings =====
const char* AP_SSID = "BadmintonCoach";
const char* AP_PASS = "coach1234";  // อย่างน้อย 8 ตัวอักษร

// ===== IMU Settings (MPU6050 / LSM6DS3) =====
#define IMU_ADDR 0x68  // MPU6050 default address
#define GYRO_SENSITIVITY 131.0  // LSB/(°/s) for ±250°/s
#define ACCEL_SENSITIVITY 16384.0  // LSB/g for ±2g
#define COMPLEMENTARY_ALPHA 0.96  // Gyro weight (0.9-0.98)
#define RAD_TO_DEG 57.2957795

// ===== Server =====
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ===== IMU Variables =====
float elbowAngle = 0;
float peakAngle = 0;
unsigned long lastIMURead = 0;
const int IMU_INTERVAL_MS = 20;  // 50Hz update rate

// ===== Shot Detection =====
float accelMagnitude = 0;
float prevAccelMag = 0;
const float SHOT_THRESHOLD = 2.5;  // g threshold for shot detection
bool shotDetected = false;
unsigned long lastShotTime = 0;
const int SHOT_COOLDOWN_MS = 500;

// ===== Function Prototypes =====
void initIMU();
void readIMU(float* ax, float* ay, float* az, float* gx, float* gy, float* gz);
float calculateElbowAngle(float ax, float ay, float az, float gx, float gy, float gz, float dt);
String classifyShot(float peakAccel, float angle);
int calculatePowerScore(float accelPeak);
int calculateTimingScore(float angle);
int calculateSweetSpotScore(float vibration);
String assessInjuryRisk(float angle);

void setup() {
    Serial.begin(115200);
    Serial.println("Badminton AI Coach - Starting...");

    // Init LittleFS
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS Mount Failed!");
        return;
    }
    Serial.println("LittleFS mounted");

    // Init I2C & IMU
    Wire.begin();
    initIMU();
    Serial.println("IMU initialized");

    // Start WiFi AP
    WiFi.softAP(AP_SSID, AP_PASS);
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());  // จะได้ 192.168.4.1

    // WebSocket handler
    ws.onEvent(onWebSocketEvent);
    server.addHandler(&ws);

    // Serve static files from LittleFS
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    // Start server
    server.begin();
    Serial.println("Server started! Connect to WiFi: " + String(AP_SSID));
    Serial.println("Open browser: http://192.168.4.1");
}

void loop() {
    ws.cleanupClients();

    unsigned long now = millis();

    // Read IMU at 50Hz
    if (now - lastIMURead >= IMU_INTERVAL_MS) {
        float dt = (now - lastIMURead) / 1000.0;
        lastIMURead = now;

        float ax, ay, az, gx, gy, gz;
        readIMU(&ax, &ay, &az, &gx, &gy, &gz);

        // Calculate elbow angle
        elbowAngle = calculateElbowAngle(ax, ay, az, gx, gy, gz, dt);
        if (elbowAngle > peakAngle) peakAngle = elbowAngle;

        // Send real-time elbow angle via WebSocket (every 100ms to save bandwidth)
        static unsigned long lastWsSend = 0;
        if (now - lastWsSend >= 100 && ws.count() > 0) {
            lastWsSend = now;
            String json = "{\"type\":\"imu\",\"elbowAngle\":" + String((int)elbowAngle) + "}";
            ws.textAll(json);
        }

        // Shot detection
        accelMagnitude = sqrt(ax * ax + ay * ay + az * az);
        if (accelMagnitude > SHOT_THRESHOLD && !shotDetected && (now - lastShotTime > SHOT_COOLDOWN_MS)) {
            shotDetected = true;
            lastShotTime = now;

            // Classify and score the shot
            String shotType = classifyShot(accelMagnitude, elbowAngle);
            int power = calculatePowerScore(accelMagnitude);
            int timing = calculateTimingScore(elbowAngle);
            int sweetspot = calculateSweetSpotScore(accelMagnitude);  // simplified
            String injury = assessInjuryRisk(peakAngle);

            // Send shot data
            if (ws.count() > 0) {
                String json = "{\"type\":\"shot\""
                    ",\"shotType\":\"" + shotType + "\""
                    ",\"power\":" + String(power) +
                    ",\"timing\":" + String(timing) +
                    ",\"sweetspot\":" + String(sweetspot) +
                    ",\"elbowAngle\":" + String((int)peakAngle) +
                    ",\"injuryRisk\":\"" + injury + "\"}";
                ws.textAll(json);
            }

            // Reset peak for next shot
            peakAngle = 0;

            Serial.println("Shot: " + shotType + " P:" + String(power) + " T:" + String(timing));
        }

        // Reset shot detection
        if (accelMagnitude < 1.5) {
            shotDetected = false;
        }
    }
}

// ===== IMU Functions =====

void initIMU() {
    // Wake up MPU6050
    Wire.beginTransmission(IMU_ADDR);
    Wire.write(0x6B);  // PWR_MGMT_1
    Wire.write(0x00);  // Wake up
    Wire.endTransmission();

    // Set Gyro range ±250°/s
    Wire.beginTransmission(IMU_ADDR);
    Wire.write(0x1B);
    Wire.write(0x00);
    Wire.endTransmission();

    // Set Accel range ±2g
    Wire.beginTransmission(IMU_ADDR);
    Wire.write(0x1C);
    Wire.write(0x00);
    Wire.endTransmission();
}

void readIMU(float* ax, float* ay, float* az, float* gx, float* gy, float* gz) {
    Wire.beginTransmission(IMU_ADDR);
    Wire.write(0x3B);  // Starting register
    Wire.endTransmission(false);
    Wire.requestFrom(IMU_ADDR, 14);

    int16_t raw_ax = Wire.read() << 8 | Wire.read();
    int16_t raw_ay = Wire.read() << 8 | Wire.read();
    int16_t raw_az = Wire.read() << 8 | Wire.read();
    Wire.read(); Wire.read();  // Temperature (skip)
    int16_t raw_gx = Wire.read() << 8 | Wire.read();
    int16_t raw_gy = Wire.read() << 8 | Wire.read();
    int16_t raw_gz = Wire.read() << 8 | Wire.read();

    *ax = raw_ax / ACCEL_SENSITIVITY;
    *ay = raw_ay / ACCEL_SENSITIVITY;
    *az = raw_az / ACCEL_SENSITIVITY;
    *gx = raw_gx / GYRO_SENSITIVITY;
    *gy = raw_gy / GYRO_SENSITIVITY;
    *gz = raw_gz / GYRO_SENSITIVITY;
}

float calculateElbowAngle(float ax, float ay, float az, float gx, float gy, float gz, float dt) {
    // Accelerometer angle (pitch of forearm)
    float accelAngle = atan2(ay, sqrt(ax * ax + az * az)) * RAD_TO_DEG;

    // Map to 0-180 range (elbow angle approximation)
    // เมื่อแขนเหยียดตรง accelAngle ≈ 0° → elbow = 180°
    // เมื่อแขนงอ 90° accelAngle ≈ 90° → elbow = 90°
    float accelElbow = 180.0 - abs(accelAngle);

    // Complementary filter: combine gyro integration with accel
    static float filteredAngle = 90.0;
    float gyroRate = gx;  // ใช้แกน X (pitch) สำหรับการงอ-เหยียดข้อศอก
    filteredAngle = COMPLEMENTARY_ALPHA * (filteredAngle + gyroRate * dt) + (1.0 - COMPLEMENTARY_ALPHA) * accelElbow;

    // Clamp to valid range
    filteredAngle = constrain(filteredAngle, 0, 180);

    return filteredAngle;
}

// ===== Shot Classification =====

String classifyShot(float peakAccel, float angle) {
    // Simple classification based on acceleration and arm angle
    if (peakAccel > 5.0 && angle > 150) return "Smash";
    if (peakAccel > 3.5 && angle > 130) return "Clear";
    if (peakAccel < 3.0 && angle < 120) return "Drop";
    return "Drive";
}

int calculatePowerScore(float accelPeak) {
    // Map acceleration to 0-100 score
    int score = (int)map(constrain(accelPeak * 100, 150, 800), 150, 800, 30, 100);
    return constrain(score, 0, 100);
}

int calculateTimingScore(float angle) {
    // Optimal timing = arm extended (150-170°)
    float optimal = 160.0;
    float diff = abs(angle - optimal);
    int score = 100 - (int)(diff * 2);
    return constrain(score, 0, 100);
}

int calculateSweetSpotScore(float vibration) {
    // Higher vibration = off-center hit = lower score
    // This is simplified; real implementation would use piezo sensor
    int score = 100 - (int)((vibration - 2.0) * 15);
    return constrain(score, 0, 100);
}

String assessInjuryRisk(float angle) {
    if (angle > 165) return "High";
    if (angle > 145) return "Medium";
    return "Low";
}

// ===== WebSocket Event Handler =====

void onWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                      AwsEventType type, void* arg, uint8_t* data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("Client #%u connected\n", client->id());
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("Client #%u disconnected\n", client->id());
            break;
        case WS_EVT_DATA:
            // Handle incoming commands from dashboard if needed
            break;
        default:
            break;
    }
}
