#include <Arduino.h>

/* --- KONFIGURASI BLYNK --- */
#define BLYNK_TEMPLATE_ID "TMPL5t3kPxkji"
#define BLYNK_TEMPLATE_NAME "Earthquake Detection"
#define BLYNK_AUTH_TOKEN "8qCT2VWoMROkVQcx-Ei7RtCHzJ_h2OOn"

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <LiquidCrystal_I2C.h>

// --- KREDENSIAL WIFI ---
char ssid[] = "Wokwi-GUEST";
char pass[] = "";

// --- INISIALISASI OBJEK ---
Adafruit_MPU6050 mpu;
LiquidCrystal_I2C lcd(0x27, 16, 2);
BlynkTimer timer;

// --- DEFINISI PIN ---
#define BUZZER_PIN 13
#define LED_HIJAU 25
#define LED_KUNING 26
#define LED_MERAH 27

// --- AMBANG BATAS (UNIT: m/s²) ---
#define THRESH_AMAN 2.5
#define THRESH_WASPADA 6.0

// --- BASELINE KALIBRASI ---
float baseX = 0, baseY = 0, baseZ = 0;

// =========================================================
// FUNGSI BUZZER — pakai tone()/noTone() agar kompatibel
// di semua versi ESP32 Arduino Core (v2.x maupun v3.x)
// tanpa perlu ledcSetup / ledcAttachPin sama sekali
// =========================================================
void buzzerOn(int frekuensi)
{
  tone(BUZZER_PIN, frekuensi);
}

void buzzerOff()
{
  noTone(BUZZER_PIN);
}

// --- FUNGSI HITUNG SKALA RICHTER ---
float hitungRichter(float ms2)
{
  float g = ms2 / 9.81;
  if (g < 0.01)
    return 0.0;
  float r = 1.0 + log10(g + 1.0) * 3.5;
  return constrain(r, 0.0, 9.0);
}

// --- FUNGSI KALIBRASI ---
void kalibrasi()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("  Kalibrasi...  ");
  lcd.setCursor(0, 1);
  lcd.print("  Jangan gerak! ");
  Serial.println("[INFO] Memulai Kalibrasi...");

  float sx = 0, sy = 0, sz = 0;
  sensors_event_t a, g, tmp;
  for (int i = 0; i < 100; i++)
  {
    mpu.getEvent(&a, &g, &tmp);
    sx += a.acceleration.x;
    sy += a.acceleration.y;
    sz += a.acceleration.z;
    delay(10);
  }
  baseX = sx / 100.0;
  baseY = sy / 100.0;
  baseZ = sz / 100.0;

  Serial.printf("[INFO] Baseline -> X:%.3f Y:%.3f Z:%.3f\n", baseX, baseY, baseZ);
  Serial.println("");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("   Selesai!     ");
  lcd.setCursor(0, 1);
  lcd.print(" Sistem Aktif!  ");
  Serial.println("[INFO] Kalibrasi Selesai.");
  delay(1500);
}

// --- FUNGSI CEK GEMPA (dipanggil timer tiap 500ms) ---
void checkEarthquake()
{
  sensors_event_t a, g, tmp;
  mpu.getEvent(&a, &g, &tmp);

  // 1. Hitung akselerasi relatif terhadap baseline
  float accX_ms2 = a.acceleration.x - baseX;
  float accY_ms2 = a.acceleration.y - baseY;
  float accZ_ms2 = a.acceleration.z - baseZ;
  float totalMs2 = sqrt(accX_ms2 * accX_ms2 + accY_ms2 * accY_ms2 + accZ_ms2 * accZ_ms2);

  // 2. Konversi ke satuan g
  float accX = accX_ms2 / 9.81;
  float accY = accY_ms2 / 9.81;
  float accZ = accZ_ms2 / 9.81;
  float totalG = totalMs2 / 9.81;

  float richter = hitungRichter(totalMs2);

  // 3. Matikan semua LED dulu
  digitalWrite(LED_HIJAU, LOW);
  digitalWrite(LED_KUNING, LOW);
  digitalWrite(LED_MERAH, LOW);

  String status;

  if (totalMs2 < THRESH_AMAN)
  {
    // ---- STATUS: AMAN ----
    status = "AMAN";
    digitalWrite(LED_HIJAU, HIGH);
    buzzerOff();
  }
  else if (totalMs2 < THRESH_WASPADA)
  {
    // ---- STATUS: WASPADA ----
    status = "WASPADA";
    digitalWrite(LED_KUNING, HIGH);
    buzzerOn(800); // 800 Hz
  }
  else
  {
    // ---- STATUS: BAHAYA ----
    status = "BAHAYA!";
    digitalWrite(LED_MERAH, HIGH);
    buzzerOn(2000); // 2000 Hz

    // Kirim notifikasi Blynk
    Blynk.logEvent("earthquake_alert",
                   String("Gempa! Skala: ") + String(richter, 1) + " SR");
  }

  // 4. Tampilan LCD
  // Baris 0: status + skala richter
  lcd.setCursor(0, 0);
  lcd.print(status);
  lcd.print(" R:");
  lcd.print(richter, 1);
  lcd.print("     "); // bersihkan sisa char

  // Baris 1: total akselerasi dalam g
  lcd.setCursor(0, 1);
  lcd.print("Acc:");
  lcd.print(totalG, 3);
  lcd.print(" g   ");

  // 5. Kirim ke Blynk
  Blynk.virtualWrite(V1, richter); // Skala Richter
  Blynk.virtualWrite(V2, totalG);  // Total G
  Blynk.virtualWrite(V3, status);  // Status teks
  Blynk.virtualWrite(V4, accX);    // Sumbu X
  Blynk.virtualWrite(V5, accY);    // Sumbu Y
  Blynk.virtualWrite(V6, accZ);    // Sumbu Z

  Serial.print("Status: "); Serial.print(status);
  Serial.print(" | Richter: "); Serial.print(richter);
  Serial.print(" | Total: "); Serial.print(totalMs2); Serial.println(" m/s²");
}

// =============================================================
void setup()
{
  Serial.begin(115200);
  Serial.println("--- Earthquake Detection Booting ---");

  // Pin output
  pinMode(LED_HIJAU, OUTPUT);
  pinMode(LED_KUNING, OUTPUT);
  pinMode(LED_MERAH, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT); // ✅ Wajib untuk tone() di ESP32

  // Test buzzer singkat saat boot (opsional, untuk verifikasi)
  buzzerOn(1000);
  delay(200);
  buzzerOff();

  // Inisialisasi Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  // Inisialisasi I2C & LCD
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Init MPU6050...");

  // Inisialisasi MPU6050
  if (!mpu.begin())
  {
    Serial.println("[ERROR] MPU6050 Tidak Terdeteksi!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("  MPU6050 ERROR ");
    lcd.setCursor(0, 1);
    lcd.print("  Cek Kabel I2C ");
    while (1)
      delay(1000);
  }

  Serial.println("[INFO] MPU6050 OK");

  // Konfigurasi range sensor
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  // Kalibrasi
  kalibrasi();

  // Timer
  timer.setInterval(500L, checkEarthquake);

  Serial.println("[INFO] Sistem Siap!");
}

void loop()
{
  Blynk.run();
  timer.run();
}
