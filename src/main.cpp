#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <SPI.h>
#include <MFRC522.h>
#include <time.h>

// ==========================================
// 1. ตั้งค่า Wi-Fi
// ==========================================
const char* WIFI_SSID = "Forever_2.4G";
const char* WIFI_PASS = "0917790252B";

// ==========================================
// 2. ข้อมูลโปรเจกต์ Firebase (FitLab)
// ==========================================
const char* PROJECT_ID = "fitlab-3ad95";
const char* API_KEY    = "AIzaSyCWrjjjAED_ZII9-lFvEY9iwEqqsP_owE8";

// ==========================================
// 3. ขาต่อ RFID-RC522 และไฟ RGB บน ESP32-S3
// ==========================================
#define SS_PIN    10
#define RST_PIN   9
#define SCK_PIN   12
#define MISO_PIN  13
#define MOSI_PIN  11
#define RGB_PIN   48

MFRC522 mfrc522(SS_PIN, RST_PIN);

// ฟังก์ชันควบคุมสีไฟ RGB บนบอร์ด
void setColorRed()   { neopixelWrite(RGB_PIN, 50, 0, 0); }  // แดง: รอแตะบัตร
void setColorGreen() { neopixelWrite(RGB_PIN, 0, 50, 0); }  // เขียว: ส่งสำเร็จ
void setColorBlue()  { neopixelWrite(RGB_PIN, 0, 0, 50); }  // น้ำเงิน: กำลังส่งข้อมูล
void setColorOff()   { neopixelWrite(RGB_PIN, 0, 0, 0); }

void blinkRedError() {
  for (int i = 0; i < 3; i++) {
    setColorOff();
    delay(150);
    setColorRed();
    delay(150);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  setColorRed();

  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
  mfrc522.PCD_Init();

  // เชื่อมต่อ Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("กำลังเชื่อมต่อ WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nเชื่อมต่อ WiFi สำเร็จ! IP: " + WiFi.localIP().toString());

  // ซิงค์เวลา NTP เพื่อสร้าง Timestamp (ms)
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("กำลังซิงค์เวลา NTP");
  while (time(nullptr) < 1700000000) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n====================================");
  Serial.println("พร้อมใช้งาน: กรุณาแตะบัตร RFID...");
  Serial.println("====================================");
}

bool sendRfidToFirestore(String uid) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[Error] WiFi หลุดการเชื่อมต่อ");
    return false;
  }

  long long nowMs = (long long)time(nullptr) * 1000LL;

  String url = "https://firestore.googleapis.com/v1/projects/" + String(PROJECT_ID) +
               "/databases/(default)/documents/rfidReader/register_station?key=" + String(API_KEY);

  String payload = "{\"fields\":{"
                   "\"rfidUid\":{\"stringValue\":\"" + uid + "\"},"
                   "\"scannedAt\":{\"integerValue\":\"" + String(nowMs) + "\"}"
                   "}}";

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  if (!http.begin(client, url)) {
    Serial.println("[Error] ไม่สามารถเริ่มการเชื่อมต่อ HTTPS ได้");
    return false;
  }

  http.addHeader("Content-Type", "application/json");
  int httpCode = http.PATCH(payload);
  String response = http.getString();
  http.end();

  if (httpCode == 200) {
    Serial.println("[Firebase] ส่งรหัสบัตร " + uid + " สำเร็จ!");
    return true;
  } else {
    Serial.printf("[Firebase Error] HTTP Code: %d\n", httpCode);
    Serial.println("รายละเอียด Error: " + response);
    return false;
  }
}

void loop() {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  // อ่านค่า UID แบบมีเว้นวรรค เช่น "11 D8 14 07"
  String currentUID = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) {
      currentUID += "0";
    }
    currentUID += String(mfrc522.uid.uidByte[i], HEX);
    if (i < mfrc522.uid.size - 1) {
      currentUID += " ";
    }
  }
  currentUID.toUpperCase();

  Serial.println("\nแตะบัตร RFID: " + currentUID);
  setColorBlue(); // สีน้ำเงิน: กำลังส่งข้อมูลเข้า Cloud

  if (sendRfidToFirestore(currentUID)) {
    setColorGreen(); // สีเขียว: ส่งเข้า Firestore สำเร็จ
    delay(2000);
    setColorRed();
  } else {
    blinkRedError(); // กระพริบแดง 3 ครั้ง: ส่งไม่สำเร็จ
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}