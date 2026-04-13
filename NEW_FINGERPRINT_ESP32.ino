#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Fingerprint.h>

/* =========================
   OLED (SSD1306 I2C)
   ========================= */
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

/* =========================
   Fingerprint (UART)
   =========================
   ESP32 Serial2 default pins can be assigned:
   RX2 = 16, TX2 = 17 (you can change)
*/
static const int FP_RX = 16;   // ESP32 RX2  <- Sensor TX
static const int FP_TX = 17;   // ESP32 TX2  -> Sensor RX

HardwareSerial FPSerial(2);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&FPSerial);

/* =========================
   Mapping ID -> "Jenis Jari"
   =========================
   Silakan ubah sesuai kebutuhanmu.
*/
String fingerName(uint16_t id) {
  switch (id) {
    case 1:  return "Jempol Kanan";
    case 2:  return "Telunjuk Kanan";
    case 3:  return "Tengah Kanan";
    case 4:  return "Manis Kanan";
    case 5:  return "Kelingking Kanan";
    case 6:  return "Jempol Kiri";
    case 7:  return "Telunjuk Kiri";
    case 8:  return "Tengah Kiri";
    case 9:  return "Manis Kiri";
    case 10: return "Kelingking Kiri";
    default: return "Tidak dipetakan";
  }
}

/* =========================
   OLED helper
   ========================= */
void oledPrintCenter(const String &line1, const String &line2 = "", const String &line3 = "") {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  int y = 10;

  auto printLine = [&](const String &s) {
    if (s.length() == 0) return;
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
    int x = (SCREEN_WIDTH - (int)w) / 2;
    display.setCursor(max(0, x), y);
    display.print(s);
    y += 14;
  };

  printLine(line1);
  printLine(line2);
  printLine(line3);

  display.display();
}

void oledStatusSmall(const String &top, const String &mid, const String &bot) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.print(top);

  display.setCursor(0, 22);
  display.print(mid);

  display.setCursor(0, 44);
  display.print(bot);

  display.display();
}

/* =========================
   Enroll function
   =========================
   Based on Adafruit_Fingerprint examples.
*/
uint8_t enrollFingerprint(uint16_t id) {
  int p = -1;

  oledPrintCenter("ENROLL MODE", "ID: " + String(id), "Letakkan jari...");
  Serial.println("ENROLL ID " + String(id) + ": Letakkan jari...");

  // Wait for finger
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        break;
      case FINGERPRINT_NOFINGER:
        delay(50);
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        Serial.println("Error komunikasi (packet receive).");
        oledPrintCenter("ERROR", "Komunikasi", "Cek wiring");
        delay(800);
        break;
      case FINGERPRINT_IMAGEFAIL:
        Serial.println("Gagal ambil gambar.");
        break;
      default:
        Serial.println("Unknown error getImage: " + String(p));
        break;
    }
  }

  // Convert image to template (buffer 1)
  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK) {
    Serial.println("image2Tz(1) gagal: " + String(p));
    oledPrintCenter("ENROLL FAIL", "image2Tz(1)", "kode: " + String(p));
    delay(1000);
    return p;
  }

  oledPrintCenter("OK", "Angkat jari", "");
  Serial.println("Angkat jari...");
  delay(1200);

  // Wait until finger removed
  while (finger.getImage() != FINGERPRINT_NOFINGER) delay(50);

  oledPrintCenter("Letakkan lagi", "ID: " + String(id), "untuk konfirmasi");
  Serial.println("Letakkan jari yang sama lagi...");

  p = -1;
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        break;
      case FINGERPRINT_NOFINGER:
        delay(50);
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        Serial.println("Error komunikasi (packet receive).");
        oledPrintCenter("ERROR", "Komunikasi", "Cek wiring");
        delay(800);
        break;
      case FINGERPRINT_IMAGEFAIL:
        Serial.println("Gagal ambil gambar.");
        break;
      default:
        Serial.println("Unknown error getImage: " + String(p));
        break;
    }
  }

  // Convert image to template (buffer 2)
  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK) {
    Serial.println("image2Tz(2) gagal: " + String(p));
    oledPrintCenter("ENROLL FAIL", "image2Tz(2)", "kode: " + String(p));
    delay(1000);
    return p;
  }

  // Create model
  p = finger.createModel();
  if (p != FINGERPRINT_OK) {
    Serial.println("createModel gagal: " + String(p));
    oledPrintCenter("ENROLL FAIL", "createModel", "kode: " + String(p));
    delay(1000);
    return p;
  }

  // Store model
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.println("Enroll sukses! ID tersimpan: " + String(id));
    oledPrintCenter("ENROLL OK", "Tersimpan ID", String(id));
    delay(1200);
  } else {
    Serial.println("storeModel gagal: " + String(p));
    oledPrintCenter("ENROLL FAIL", "storeModel", "kode: " + String(p));
    delay(1200);
  }

  return p;
}

/* =========================
   Identify function
   ========================= */
int16_t identifyFingerprint(uint16_t &outId, uint16_t &outConfidence) {
  int p = finger.getImage();
  if (p != FINGERPRINT_OK) return p;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) return p;

  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK) return p;

  outId = finger.fingerID;
  outConfidence = finger.confidence;
  return FINGERPRINT_OK;
}

/* =========================
   Serial command parser
   =========================
   Commands:
   - e<ID>  => enroll, contoh: e1, e10, e25
*/
bool readEnrollCommand(uint16_t &id) {
  if (!Serial.available()) return false;
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  cmd.toLowerCase();

  if (cmd.length() >= 2 && cmd.charAt(0) == 'e') {
    String num = cmd.substring(1);
    int val = num.toInt();
    if (val > 0 && val <= 200) {  // kapasitas tergantung sensor, ini batas aman
      id = (uint16_t)val;
      return true;
    } else {
      Serial.println("ID tidak valid. Pakai 1..200 (atau sesuaikan).");
    }
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(200);

  // OLED init
  Wire.begin(21, 22);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    // kalau OLED kamu 0x3D, ganti jadi 0x3D
    Serial.println("OLED tidak terdeteksi. Cek alamat I2C (0x3C/0x3D) dan wiring.");
    while (true) delay(100);
  }
  display.clearDisplay();
  display.display();

  oledPrintCenter("CNC TEST", "HLK-ZW101", "ESP32 + OLED");
  delay(900);

  // Fingerprint init
  FPSerial.begin(57600, SERIAL_8N1, FP_RX, FP_TX); // banyak sensor default 57600
  finger.begin(57600);

  if (finger.verifyPassword()) {
    Serial.println("Fingerprint sensor terdeteksi dan siap.");
    oledPrintCenter("Sensor OK", "Siap scan", "Serial: e<ID> enroll");
    delay(900);
  } else {
    Serial.println("Fingerprint sensor tidak terdeteksi / password salah.");
    oledPrintCenter("Sensor ERROR", "Cek wiring", "Baud/power");
    while (true) delay(100);
  }

  // Optional: tampilkan info template
  finger.getTemplateCount();
  Serial.println("Template tersimpan: " + String(finger.templateCount));
}

void loop() {
  // 1) Cek command enroll dari Serial
  uint16_t enrollId;
  if (readEnrollCommand(enrollId)) {
    enrollFingerprint(enrollId);

    finger.getTemplateCount();
    Serial.println("Template tersimpan sekarang: " + String(finger.templateCount));
    oledPrintCenter("Kembali", "Scan mode", "");
    delay(700);
  }

  // 2) Scan finger terus-menerus
  uint16_t id = 0, conf = 0;
  int16_t res = identifyFingerprint(id, conf);

  if (res == FINGERPRINT_OK) {
    String name = fingerName(id);

    Serial.println("MATCH! ID=" + String(id) + " | " + name + " | conf=" + String(conf));

    oledStatusSmall(
      "MATCH",
      "ID: " + String(id) + " (" + name + ")",
      "Conf: " + String(conf)
    );

    // tunggu finger dilepas biar tidak spam
    while (finger.getImage() != FINGERPRINT_NOFINGER) delay(50);
    delay(200);

    oledPrintCenter("Siap scan", "Tempelkan jari", "Serial: e<ID> enroll");
  } 
  else if (res == FINGERPRINT_NOFINGER) {
    // idle, biar OLED tetap informatif
    // (ga usah di-refresh terus supaya ga flicker)
    delay(60);
  } 
  else if (res == FINGERPRINT_NOTFOUND) {
    Serial.println("Tidak cocok (NOTFOUND).");
    oledPrintCenter("Tidak cocok", "Daftarkan dulu", "Serial: e<ID> enroll");
    while (finger.getImage() != FINGERPRINT_NOFINGER) delay(50);
    delay(300);
    oledPrintCenter("Siap scan", "Tempelkan jari", "Serial: e<ID> enroll");
  } 
  else {
    // error lain (komunikasi, image2Tz fail, dll)
    Serial.println("Scan error code: " + String(res));
    // tetap lanjut, tapi kasih jeda biar ga panik
    delay(120);
  }
}
