# วิธีนำ Dashboard ไปรันบน Arduino Q (ESP32-S3)

## สิ่งที่ต้องเตรียม

1. **บอร์ด**: ESP32-S3 (Arduino Q) 
2. **เซ็นเซอร์**: MPU6050 หรือ LSM6DS3 (6-Axis IMU)
3. **ซอฟต์แวร์**: Arduino IDE 2.x + ESP32 Board Package

---

## ขั้นตอนการติดตั้ง

### Step 1: ติดตั้ง Arduino IDE & Board Package

1. ดาวน์โหลด [Arduino IDE 2.x](https://www.arduino.cc/en/software)
2. เพิ่ม ESP32 Board URL ใน Preferences → Additional Board Manager URLs:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. ไปที่ Board Manager → ค้นหา "esp32" → ติดตั้ง **esp32 by Espressif Systems**

### Step 2: ติดตั้ง Libraries

ไปที่ Sketch → Include Library → Manage Libraries แล้วติดตั้ง:
- ไม่มีใน Library Manager ต้องติดตั้งจาก GitHub:
  - [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer) → Download ZIP → Sketch → Include Library → Add .ZIP Library
  - [AsyncTCP](https://github.com/me-no-dev/AsyncTCP) → เช่นเดียวกัน

### Step 3: อัปโหลดไฟล์ Dashboard ไปยัง LittleFS

นี่คือขั้นตอนสำคัญ — ต้องอัปโหลดไฟล์ HTML ไปเก็บใน Flash ของ ESP32

#### วิธีที่ 1: ใช้ Arduino IDE Plugin

1. ดาวน์โหลด [arduino-esp32fs-plugin](https://github.com/lorol/arduino-esp32fs-plugin/releases)
2. วางไฟล์ `.jar` ไว้ใน `<Arduino>/tools/ESP32FS/tool/esp32fs.jar`
3. สร้างโฟลเดอร์ `data` ในโฟลเดอร์โปรเจกต์ Arduino:
   ```
   arduino_q_server/
   ├── arduino_q_server.ino
   └── data/
       └── index.html        ← คัดลอกจาก dashboard/index.html
   ```
4. ใน Arduino IDE: Tools → ESP32 Sketch Data Upload
5. เลือก "LittleFS" แล้วกด Upload

#### วิธีที่ 2: ใช้ PlatformIO (แนะนำ)

1. ติดตั้ง [PlatformIO](https://platformio.org/)
2. สร้างโปรเจกต์ ESP32-S3
3. วางไฟล์ใน:
   ```
   project/
   ├── src/
   │   └── main.cpp          ← เนื้อหาจาก arduino_q_server.ino
   ├── data/
   │   └── index.html        ← คัดลอกจาก dashboard/index.html
   └── platformio.ini
   ```
4. `platformio.ini`:
   ```ini
   [env:esp32s3]
   platform = espressif32
   board = esp32-s3-devkitc-1
   framework = arduino
   board_build.filesystem = littlefs
   lib_deps =
       me-no-dev/ESPAsyncWebServer @ ^1.2.3
       me-no-dev/AsyncTCP @ ^1.1.1
   ```
5. อัปโหลด filesystem: `pio run --target uploadfs`
6. อัปโหลดโค้ด: `pio run --target upload`

### Step 4: ต่อวงจร IMU

```
ESP32-S3          MPU6050/LSM6DS3
─────────         ───────────────
3.3V       →      VCC
GND        →      GND
GPIO 8 (SDA) →   SDA
GPIO 9 (SCL) →   SCL
```

> หมายเหตุ: ขา SDA/SCL อาจต่างกันตามบอร์ด ดูจาก pinout ของบอร์ดที่ใช้

### Step 5: อัปโหลดโค้ดและทดสอบ

1. เลือกบอร์ด: Tools → Board → ESP32S3 Dev Module
2. เลือก Port ที่ถูกต้อง
3. กด Upload
4. เปิด Serial Monitor (115200 baud) → จะเห็น:
   ```
   Badminton AI Coach - Starting...
   LittleFS mounted
   IMU initialized
   AP IP: 192.168.4.1
   Server started! Connect to WiFi: BadmintonCoach
   Open browser: http://192.168.4.1
   ```

---

## วิธีใช้งาน

1. **เปิดบอร์ด** → ESP32 จะสร้าง WiFi ชื่อ `BadmintonCoach`
2. **เชื่อมต่อ WiFi** จากมือถือ/คอม → รหัส: `coach1234`
3. **เปิด Browser** → พิมพ์ `http://192.168.4.1`
4. **Dashboard จะแสดงขึ้นมา** พร้อมรับข้อมูลแบบ real-time

---

## โครงสร้างไฟล์ในโปรเจกต์

```
Arduino-Q/
├── dashboard/
│   └── index.html              ← ไฟล์ Dashboard (พัฒนา/ทดสอบบน PC)
├── arduino_q_server/
│   ├── arduino_q_server.ino    ← โค้ด Arduino หลัก
│   └── data/
│       └── index.html          ← คัดลอกมาจาก dashboard/ (สำหรับ upload ไป LittleFS)
├── DEPLOY.md                   ← ไฟล์นี้
└── README.md
```

---

## ทดสอบ Dashboard บน PC ก่อน

เปิดไฟล์ `dashboard/index.html` ใน Browser ได้เลย — จะเข้า Demo Mode อัตโนมัติ
หลังจาก 5 วินาทีที่เชื่อมต่อ WebSocket ไม่ได้ จะแสดงข้อมูลจำลองให้ดู

---

## ปรับแต่งเพิ่มเติม

| ต้องการ | แก้ไขที่ |
|---------|----------|
| เปลี่ยนชื่อ WiFi/รหัส | `AP_SSID` / `AP_PASS` ใน .ino |
| ปรับ threshold ตรวจจับ shot | `SHOT_THRESHOLD` ใน .ino |
| ปรับ sensitivity มุมข้อศอก | `COMPLEMENTARY_ALPHA` ใน .ino |
| เปลี่ยนหน้าตา Dashboard | แก้ CSS ใน index.html |
| เพิ่มเซ็นเซอร์ (Piezo/Mic) | เพิ่ม analog read ใน loop() |
