# Earthquake Detection with MPU6050

Dokumentasi ini menjelaskan cara menjalankan proyek IoT deteksi gempa berbasis ESP32, MPU6050, LCD I2C, dan platform Blynk. Termasuk instruksi pasang ekstensi Wokwi dan PlatformIO serta ketentuan penggunaan Wokwi.

## Persyaratan

1. **Visual Studio Code**
   - Buka proyek di VS Code.

2. **PlatformIO**
   - Install extension `PlatformIO IDE` di VS Code.
   - PlatformIO digunakan untuk build, upload, dan dependency management.

3. **Wokwi Extension**
   - Install extension `Wokwi` atau `Wokwi Simulator` di VS Code.
   - Pastikan Anda sudah menerima lisensi atau Terms of Service dari Wokwi.
   - Jika diminta login atau aktivasi, lakukan sesuai petunjuk Wokwi.

4. **Akses Internet**
   - Diperlukan untuk mengunduh library PlatformIO dan menghubungkan Blynk.

## Struktur Proyek

- `platformio.ini` : konfigurasi board ESP32 dan library dependency.
- `src/main.cpp` : kode utama untuk deteksi gempa, Blynk, dan tampilan LCD.
- `wokwi.toml` : konfigurasi untuk integrasi dengan Wokwi simulator.
- `doc/` : contoh tampilan dashboard dan screenshot.

## Komponen yang Dibutuhkan

- ESP32 DevKit V1
- Modul sensor MPU6050
- LCD I2C 16x2
- Buzzer
- LED hijau, kuning, merah
- Kabel jumper dan breadboard

## Menjalankan di PlatformIO

1. Buka folder proyek di VS Code.
2. Pastikan PlatformIO extension sudah terpasang.
3. Buka panel PlatformIO atau gunakan terminal:
   - `PlatformIO: Build`
   - `PlatformIO: Upload`
4. Jika belum ada library yang terpasang, PlatformIO akan mengunduh otomatis berdasarkan `lib_deps` di `platformio.ini`.

### Konfigurasi Koneksi WiFi dan Blynk

Dalam `src/main.cpp`, ubah:

- `ssid` dan `pass` jika menggunakan WiFi sendiri.
- `BLYNK_AUTH_TOKEN` dengan token dari aplikasi Blynk Anda.
- `BLYNK_TEMPLATE_ID` jika menggunakan template Blynk berbeda.

Contoh:

```cpp
char ssid[] = "Nama_WiFi";
char pass[] = "Password_WiFi";
```

> Catatan: Kode default menggunakan `Wokwi-GUEST` untuk simulasi Wokwi. Jika menjalankan di perangkat nyata, ganti dengan kredensial WiFi Anda.

## Menjalankan di Wokwi

1. Pastikan extension Wokwi terpasang dan aktif.
2. Gunakan `wokwi.toml` untuk menunjuk firmware yang dibangun oleh PlatformIO.
3. Setelah build selesai, jalankan simulator Wokwi dari VS Code.
4. Jika Wokwi meminta lisensi atau aktivasi, ikuti instruksi untuk mendapatkan izin.

> Jika Anda belum memiliki akun Wokwi, daftar di https://wokwi.com dan selesaikan proses login/aktivasi.

## Konfigurasi Blynk

Proyek ini menggunakan Blynk untuk menampilkan data gempa secara realtime:

- `V1` = nilai skala Richter
- `V2` = total akselerasi (g)
- `V3` = status teks (`AMAN`, `WASPADA`, `BAHAYA!`)
- `V4` = akselerasi X
- `V5` = akselerasi Y
- `V6` = akselerasi Z

### Contoh Dashboard Blynk

Gunakan tampilan contoh di folder `doc/` sebagai referensi:

- `doc/dashboard-view.png`
- `doc/data-stream.png`

Tambahkan widget Blynk sesuai nilai virtual pin di atas.

## Cara Kerja Program

1. ESP32 terhubung ke Blynk dan WiFi.
2. MPU6050 melakukan pembacaan akselerasi setiap 500 ms.
3. Kode menghitung perbedaan terhadap baseline kalibrasi.
4. Status ditentukan berdasarkan ambang batas:
   - `AMAN` jika getaran kecil.
   - `WASPADA` jika getaran menengah.
   - `BAHAYA!` jika getaran kuat.
5. LED dan buzzer aktif sesuai level bahaya.
6. Data dikirim ke Blynk untuk ditampilkan.

## Troubleshooting

- `MPU6050 Tidak Terdeteksi!`
  - Periksa sambungan I2C (SDA ke GPIO 21, SCL ke GPIO 22).
  - Pastikan sensor diberi daya dan alamat I2C benar.

- Build gagal karena library:
  - Pastikan koneksi internet aktif.
  - Jalankan ulang build di PlatformIO.

- Wokwi tidak berjalan:
  - Pastikan extension Wokwi terpasang dan sudah login.
  - Periksa `wokwi.toml` bahwa jalur firmware benar.

## Catatan Tambahan

- Jika ingin menggunakan board ESP32 lain, sesuaikan `board` di `platformio.ini`.
- Jika ingin make Blynk dengan template atau token baru, sesuaikan nilai di `src/main.cpp`.
- Sertakan dokumentasi `doc/` sebagai referensi tampilan UI agar konfigurasi lebih mudah.
