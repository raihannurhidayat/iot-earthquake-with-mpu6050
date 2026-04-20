#include <Arduino.h>
#include <math.h>

#define BLYNK_TEMPLATE_ID   "TMPL5t3kPxkji"
#define BLYNK_TEMPLATE_NAME "Earthquake Detection"
#define BLYNK_AUTH_TOKEN    "8qCT2VWoMROkVQcx-Ei7RtCHzJ_h2OOn"

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <LiquidCrystal_I2C.h>
#include "earthquake_model.h"

// FIX: Tambahkan STEP agar tidak error saat compile
#ifndef STEP
#define STEP 25
#endif

char ssid[] = "Wokwi-GUEST";
char pass[] = "";

Adafruit_MPU6050  mpu;
LiquidCrystal_I2C lcd(0x27, 16, 2);
BlynkTimer        timer;

#define BUZZER_PIN 13
#define LED_HIJAU  25
#define LED_KUNING 26
#define LED_MERAH  27

// Ring buffer — nilai m/s² mentah
float axBuffer[WINDOW_SIZE];
float ayBuffer[WINDOW_SIZE];
float azBuffer[WINDOW_SIZE];
int   bufferIdx  = 0;
bool  bufferFull = false;

// Baseline untuk display & Richter
float baseX = 0, baseY = 0, baseZ = 0;

// Cooldown configuration
#define COOLDOWN_WINDOWS 3
int cooldownCount = 0;

// =========================================================
// STATE MANAGEMENT — Hysteresis & Confirmation (FIXED)
// =========================================================
bool  lastStableState = false;  // false=AMAN, true=GEMPA
int   confirmCounter  = 0;      // counter konfirmasi transisi state
#define CONFIRM_COUNT     1     // Langsung responsif (1 window cukup)
#define PROB_GEMPA_THRESH 0.65f // Prob >= 0.65 → target GEMPA
#define PROB_AMAN_THRESH  0.35f // Prob <= 0.35 → target AMAN
// Zona 0.35 - 0.65 = Deadband (pertahankan state terakhir)

// Smoothing probabilitas
float smoothedProb = 0.0f;      // Start dari AMAN
#define ALPHA 0.5f              // Respons lebih cepat

// =========================================================
// UTILITAS
// =========================================================
void  buzzerOn(int f) { tone(BUZZER_PIN, f); }
void  buzzerOff()     { noTone(BUZZER_PIN); }

float calcMean(const float* a, int n) {
    float s = 0;
    for (int i = 0; i < n; i++) s += a[i];
    return s / n;
}

float calcStd(const float* a, int n) {
    float m = calcMean(a, n), s = 0;
    for (int i = 0; i < n; i++) s += (a[i]-m)*(a[i]-m);
    return sqrtf(s / n);
}

float calcMax(const float* a, int n) {
    float mx = a[0];
    for (int i = 1; i < n; i++) if (a[i] > mx) mx = a[i];
    return mx;
}

float sigmoid(float z) {
    return (z >= 0) ? 1.0f/(1.0f+expf(-z)) : expf(z)/(1.0f+expf(z));
}

float hitungRichter(float ms2) {
    float g = ms2 / 9.81f;
    if (g < 0.01f) return 0.0f;
    return constrain(1.0f + log10f(g + 1.0f) * 3.5f, 0.0f, 9.0f);
}

// =========================================================
// ML INFERENCE — Hysteresis Logic (CORRECTED)
// =========================================================
int predictEarthquake() {
    if (!bufferFull) return -1;

    // Hitung magnitude total per sampel
    float total[WINDOW_SIZE];
    for (int i = 0; i < WINDOW_SIZE; i++)
        total[i] = sqrtf(axBuffer[i]*axBuffer[i] +
                         ayBuffer[i]*ayBuffer[i] +
                         azBuffer[i]*azBuffer[i]);

    // 6 fitur STD & Max/Mean
    float features[6];
    features[0] = calcStd (axBuffer, WINDOW_SIZE);
    features[1] = calcStd (ayBuffer, WINDOW_SIZE);
    features[2] = calcStd (azBuffer, WINDOW_SIZE);
    features[3] = calcStd (total,    WINDOW_SIZE);
    features[4] = calcMax (total,    WINDOW_SIZE);
    features[5] = calcMean(total,    WINDOW_SIZE);

    // StandardScaler
    for (int i = 0; i < 6; i++)
        features[i] = (features[i] - scaler_mean[i]) / scaler_scale[i];

    // Logistic Regression
    float z = model_intercept;
    for (int i = 0; i < 6; i++) z += model_coef[i] * features[i];

    float prob = sigmoid(z);
    
    // Exponential Moving Average
    smoothedProb = ALPHA * prob + (1.0f - ALPHA) * smoothedProb;

    Serial.printf("[FEAT] total_std=%.3f total_max=%.3f | raw=%.3f smooth=%.3f\n",
                  features[3], features[4], prob, smoothedProb);

    // === HYSTERESIS & CONFIRMATION (FIXED LOGIC) ===
    bool targetState;
    if (smoothedProb >= PROB_GEMPA_THRESH) {
        targetState = true;   // Ingin berubah ke GEMPA
    } else if (smoothedProb <= PROB_AMAN_THRESH) {
        targetState = false;  // Ingin berubah ke AMAN
    } else {
        targetState = lastStableState; // Deadband: tetap di state sekarang
    }

    // Hitung konsistensi menuju state TARGET
    if (targetState != lastStableState) {
        confirmCounter++;
    } else {
        confirmCounter = 0; // Reset jika probabilitas kembali ke zona state aktif
    }

    // Transisi state hanya jika konsisten
    if (confirmCounter >= CONFIRM_COUNT) {
        lastStableState = targetState;
        confirmCounter = 0;
        Serial.printf("[STATE] >>> Berubah ke: %s (prob=%.3f)\n", 
                      lastStableState ? "GEMPA" : "AMAN", smoothedProb);
    }

    Serial.printf("[ML] z=%.3f prob=%.3f stable=%d confirm=%d/%d\n", 
                  z, smoothedProb, lastStableState ? 1 : 0, confirmCounter, CONFIRM_COUNT);

    return lastStableState ? 1 : 0;
}

// =========================================================
// KALIBRASI
// =========================================================
void kalibrasi() {
    lcd.clear();
    lcd.setCursor(0,0); lcd.print("  Kalibrasi...  ");
    lcd.setCursor(0,1); lcd.print("  Jangan gerak! ");
    Serial.println("[INFO] Memulai Kalibrasi...");

    float sx = 0, sy = 0, sz = 0;
    sensors_event_t a, g, tmp;
    for (int i = 0; i < 100; i++) {
        mpu.getEvent(&a, &g, &tmp);
        sx += a.acceleration.x;
        sy += a.acceleration.y;
        sz += a.acceleration.z;
        delay(10);
    }
    baseX = sx / 100.0f;
    baseY = sy / 100.0f;
    baseZ = sz / 100.0f;

    Serial.printf("[INFO] Baseline -> X:%.3f Y:%.3f Z:%.3f m/s²\n", baseX, baseY, baseZ);
    lcd.clear();
    lcd.setCursor(0,0); lcd.print("   Selesai!     ");
    lcd.setCursor(0,1); lcd.print(" Sistem Aktif!  ");
    Serial.println("[INFO] Kalibrasi Selesai.");
    delay(1500);
}

// =========================================================
// LOOP UTAMA (50 Hz)
// =========================================================
void checkEarthquake() {
    sensors_event_t a, g, tmp;
    mpu.getEvent(&a, &g, &tmp);

    // Isi buffer ML (RAW m/s²)
    axBuffer[bufferIdx] = a.acceleration.x;
    ayBuffer[bufferIdx] = a.acceleration.y;
    azBuffer[bufferIdx] = a.acceleration.z;

    bufferIdx++;
    if (bufferIdx >= WINDOW_SIZE) {
        bufferIdx  = 0;
        bufferFull = true;
    }
    if (!bufferFull) return;

    int prediction = predictEarthquake();

    // === COOLDOWN LOGIC ===
    // Aktifkan cooldown saat pertama kali terdeteksi GEMPA
    if (prediction == 1 && cooldownCount == 0) {
        cooldownCount = COOLDOWN_WINDOWS;
        Serial.println("[INFO] *** GEMPA terdeteksi! Cooldown aktif. ***");
    }
    
    // Selama cooldown, paksa status tetap GEMPA
    if (cooldownCount > 0) {
        prediction = 1;
        cooldownCount--;
        Serial.printf("[INFO] Cooldown: %d window tersisa\n", cooldownCount);
    }

    // Reset cooldown hanya jika benar-benar stabil di AMAN
    if (prediction == 0 && lastStableState == false) {
        cooldownCount = 0;
    }

    // Hitung nilai terkoreksi untuk display & Richter
    float ax     = a.acceleration.x - baseX;
    float ay     = a.acceleration.y - baseY;
    float az     = a.acceleration.z - baseZ;
    float totalMs2 = sqrtf(ax*ax + ay*ay + az*az);
    float totalG   = totalMs2 / 9.81f;
    float richter  = hitungRichter(totalMs2);

    // LED & Buzzer
    digitalWrite(LED_HIJAU,  LOW);
    digitalWrite(LED_KUNING, LOW);
    digitalWrite(LED_MERAH,  LOW);

    String status;
    if (prediction == 0) {
        status = "AMAN";
        digitalWrite(LED_HIJAU, HIGH);
        buzzerOff();
    } else {
        status = "GEMPA!";
        digitalWrite(LED_MERAH, HIGH);
        buzzerOn(2000);
        
        // Kirim alert Blynk hanya saat awal cooldown
        if (cooldownCount == COOLDOWN_WINDOWS - 1) {
            Blynk.logEvent("earthquake_alert",
                String("Gempa terdeteksi! Skala: ") + String(richter,1) + " SR");
            Serial.println("[BLYNK] Alert terkirim!");
        }
    }

    // LCD
    lcd.setCursor(0,0);
    lcd.print(status); lcd.print(" R:"); lcd.print(richter,1); lcd.print("     ");
    lcd.setCursor(0,1);
    lcd.print("Acc:"); lcd.print(totalG,3); lcd.print(" g   ");

    // Blynk
    Blynk.virtualWrite(V1, richter);
    Blynk.virtualWrite(V2, totalG);
    Blynk.virtualWrite(V3, status);
    Blynk.virtualWrite(V4, ax / 9.81f);
    Blynk.virtualWrite(V5, ay / 9.81f);
    Blynk.virtualWrite(V6, az / 9.81f);
    Blynk.virtualWrite(V7, prediction);
    Blynk.virtualWrite(V8, smoothedProb);

    Serial.printf(">>> ML:%d | %s | R:%.1f | Acc:%.3f g | Prob:%.3f | CD:%d\n",
                  prediction, status.c_str(), richter, totalG, smoothedProb, cooldownCount);
}

// =========================================================
// SETUP
// =========================================================
void setup() {
    Serial.begin(115200);
    Serial.println("\n=== Earthquake Detection (ML + Hysteresis) ===");

    pinMode(LED_HIJAU,  OUTPUT);
    pinMode(LED_KUNING, OUTPUT);
    pinMode(LED_MERAH,  OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    
    buzzerOn(1000); delay(200); buzzerOff();
    digitalWrite(LED_HIJAU, HIGH); delay(100); digitalWrite(LED_HIJAU, LOW);

    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
    
    Wire.begin(21, 22);
    lcd.init(); lcd.backlight(); lcd.clear();
    lcd.setCursor(0,0); lcd.print("Init MPU6050...");

    if (!mpu.begin()) {
        Serial.println("[ERROR] MPU6050 tidak terdeteksi!");
        lcd.print("  MPU6050 ERROR ");
        while (1) {
            digitalWrite(LED_MERAH, !digitalRead(LED_MERAH));
            delay(500);
        }
    }
    Serial.println("[INFO] MPU6050 OK");
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

    kalibrasi();
    timer.setInterval(20L, checkEarthquake);

    Serial.println("\n[INFO] === Sistem Siap! ===");
    Serial.printf("[INFO] Window:%d | Step:%d | Threshold: 0.35 / 0.65\n",
                  WINDOW_SIZE, STEP);
    Serial.printf("[INFO] Confirm:%d | Cooldown:%d | Alpha:%.2f\n",
                  CONFIRM_COUNT, COOLDOWN_WINDOWS, ALPHA);
    Serial.println("================================================\n");
}

void loop() {
    Blynk.run();
    timer.run();
}