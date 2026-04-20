#ifndef EARTHQUAKE_MODEL_H
#define EARTHQUAKE_MODEL_H

// ============================================================
// Model: Logistic Regression untuk deteksi gempa
// Fitur : 6 fitur berbasis STD dalam satuan m/s²
//         [ax_std, ay_std, az_std, total_std, total_max, total_mean]
// Data  : 59276 sampel raw → 2370 window (size=50, step=25)
// Akurasi test: 100.0%
// Satuan: m/s² (sama dengan output Adafruit MPU6050 library)
// Catatan: STD tidak bergantung offset/gravitasi → works di
//          Wokwi simulator & sensor fisik
// ============================================================

// StandardScaler — fit pada training set
// Urutan fitur: ax_std, ay_std, az_std, total_std, total_max, total_mean
const float scaler_mean[]  = { 15.440381, 9.191339, 24.022862, 12.685563, 76.090736, 52.439753 };
const float scaler_scale[] = { 16.563874, 9.939269, 24.597313, 12.324901, 34.747603, 13.157974 };
const int   scaler_n_features = 6;

// Logistic Regression weights
const float model_coef[]    = { 1.676148, 1.978176, 2.301304, 2.913510, 0.469453, -2.099658 };
const float model_intercept = 1.543892f;

// Konfigurasi inferensi
const float PREDICTION_THRESHOLD = 0.5f;
const float THRESHOLD   = 0.5f;
const int   WINDOW_SIZE = 50;  // harus sama dengan STEP di Python

#endif // EARTHQUAKE_MODEL_H
