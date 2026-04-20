import numpy as np
import pandas as pd
from sklearn.model_selection import train_test_split
from sklearn.linear_model import LogisticRegression
from sklearn.preprocessing import StandardScaler
from sklearn.metrics import classification_report, confusion_matrix
import matplotlib.pyplot as plt
import matplotlib
matplotlib.use('Agg')
import os, warnings
warnings.filterwarnings('ignore')

# ================================================================
# KONFIGURASI
# ================================================================
CSV_PATH    = 'data/earthquake_data.csv'
OUTPUT_H    = 'src/earthquake_model.h'
WINDOW_SIZE = 50
STEP        = 25
TEST_SIZE   = 0.2
RANDOM_SEED = 42
C_VALUE     = 1.0   # regularisasi logistic regression

# ================================================================
# 1. LOAD & VALIDASI DATA
# ================================================================
print("=" * 55)
print("  EARTHQUAKE ML MODEL TRAINER")
print("=" * 55)

if not os.path.exists(CSV_PATH):
    raise FileNotFoundError(f"File tidak ditemukan: {CSV_PATH}")

df = pd.read_csv(CSV_PATH)
print(f"\n[1] Data dimuat: {len(df)} baris, kolom: {list(df.columns)}")

# Pastikan kolom yang dibutuhkan ada
required = ['aX', 'aY', 'aZ', 'Result']
for col in required:
    if col not in df.columns:
        raise ValueError(f"Kolom '{col}' tidak ada di CSV!")

df = df[required].dropna()
print(f"    Setelah dropna: {len(df)} baris")
print(f"\n    Distribusi label:")
print(f"      Kelas 0 (AMAN) : {(df['Result']==0).sum()}")
print(f"      Kelas 1 (GEMPA): {(df['Result']==1).sum()}")

# ================================================================
# 2. KONVERSI SATUAN: raw ADC → m/s²
#    Data training menggunakan raw 16-bit ADC (±8g, 4096 LSB/g)
#    Arduino (Adafruit library) mengeluarkan m/s² secara langsung
#    → harus disamakan ke m/s² agar scaler cocok
# ================================================================
print("\n[2] Konversi satuan raw ADC → m/s²")
print(f"    Sebelum: aX mean={df['aX'].mean():.1f} (raw ADC)")

df = df.copy()
df['aX'] = df['aX'] / 4096.0 * 9.81
df['aY'] = df['aY'] / 4096.0 * 9.81
df['aZ'] = df['aZ'] / 4096.0 * 9.81

print(f"    Sesudah: aX mean={df['aX'].mean():.3f} m/s²")

# Hitung total acceleration
df['total'] = np.sqrt(df['aX']**2 + df['aY']**2 + df['aZ']**2)

print("\n    Statistik per kelas (m/s²):")
print(df.groupby('Result')[['aX','aY','aZ','total']].mean().round(3).to_string())

# ================================================================
# 3. FEATURE EXTRACTION (window-based)
#    Hanya menggunakan fitur STD — tidak bergantung pada
#    nilai absolut/offset gravitasi, sehingga bekerja di
#    Wokwi simulator maupun sensor fisik
# ================================================================
print(f"\n[3] Ekstraksi fitur (window={WINDOW_SIZE}, step={STEP})")

def prepare_for_ml(data, window_size=50, step=25):
    """
    Ekstrak 6 fitur berbasis STD dari setiap window:
      ax_std, ay_std, az_std  → variabilitas per sumbu
      total_std               → variabilitas total
      total_max               → puncak akselerasi
      total_mean              → rata-rata akselerasi

    Fitur STD dipilih karena:
    - Tidak bergantung pada offset gravitasi
    - Membedakan sinyal diam (std≈0) vs gempa (std besar)
    - Bekerja baik di Wokwi simulator & sensor fisik
    """
    features, labels = [], []
    for i in range(0, len(data) - window_size, step):
        win = data.iloc[i:i + window_size]
        feat = {
            'ax_std':     win['aX'].std(ddof=0),
            'ay_std':     win['aY'].std(ddof=0),
            'az_std':     win['aZ'].std(ddof=0),
            'total_std':  win['total'].std(ddof=0),
            'total_max':  win['total'].max(),
            'total_mean': win['total'].mean(),
        }
        features.append(feat)
        labels.append(win['Result'].mode()[0])
    return pd.DataFrame(features), np.array(labels)

X, y = prepare_for_ml(df, WINDOW_SIZE, STEP)
print(f"    Total window: {len(X)}")
print(f"    Distribusi window:")
print(f"      Kelas 0 (AMAN) : {(y==0).sum()}")
print(f"      Kelas 1 (GEMPA): {(y==1).sum()}")

print("\n    Rata-rata fitur per kelas:")
feat_df = X.copy(); feat_df['label'] = y
print(feat_df.groupby('label').mean().round(4).to_string())

# ================================================================
# 4. TRAIN / TEST SPLIT
# ================================================================
print(f"\n[4] Split data (test={TEST_SIZE*100:.0f}%, stratified)")
X_train, X_test, y_train, y_test = train_test_split(
    X, y,
    test_size=TEST_SIZE,
    random_state=RANDOM_SEED,
    stratify=y
)
print(f"    Train: {len(X_train)} | Test: {len(X_test)}")

# ================================================================
# 5. STANDARDSCALER
# ================================================================
print("\n[5] StandardScaler (fit pada train saja)")
scaler = StandardScaler()
X_train_s = scaler.fit_transform(X_train)
X_test_s  = scaler.transform(X_test)

print(f"    scaler_mean  (6 nilai): {[f'{v:.4f}' for v in scaler.mean_]}")
print(f"    scaler_scale (6 nilai): {[f'{v:.4f}' for v in scaler.scale_]}")

# ================================================================
# 6. TRAINING
# ================================================================
print(f"\n[6] Training Logistic Regression (C={C_VALUE})")
model = LogisticRegression(max_iter=1000, C=C_VALUE, random_state=RANDOM_SEED)
model.fit(X_train_s, y_train)

print(f"    Intercept  : {model.intercept_[0]:.6f}")
print(f"    Koefisien  : {[f'{v:.4f}' for v in model.coef_[0]]}")

# ================================================================
# 7. EVALUASI
# ================================================================
print("\n[7] Evaluasi pada test set")
y_pred = model.predict(X_test_s)
print(classification_report(y_test, y_pred,
                             target_names=['AMAN (0)', 'GEMPA (1)']))

cm = confusion_matrix(y_test, y_pred)
print(f"    Confusion Matrix:")
print(f"      TN={cm[0,0]}  FP={cm[0,1]}")
print(f"      FN={cm[1,0]}  TP={cm[1,1]}")

# ================================================================
# 8. VALIDASI MODEL — simulasi kondisi Arduino
# ================================================================
print("\n[8] Validasi kondisi Arduino / Wokwi")

# Kasus 1: Sensor DIAM (semua std = 0, total_mean = 9.81 m/s²)
diam = np.array([[0, 0, 0, 0, 9.81, 9.81]])
p_diam = model.predict_proba(scaler.transform(diam))[0][1]
ok_diam = p_diam < 0.5
print(f"\n    Diam (std=0, total=9.81):   prob={p_diam:.4f} "
      f"→ {'✅ AMAN' if ok_diam else '❌ BIAS - MASALAH!'}")

# Kasus 2: Gempa kecil (std sedang)
kecil = np.array([[2.0, 1.5, 1.0, 1.8, 20.0, 14.0]])
p_kecil = model.predict_proba(scaler.transform(kecil))[0][1]
print(f"    Gempa kecil (std~2 m/s²):   prob={p_kecil:.4f} "
      f"→ {'GEMPA' if p_kecil>=0.5 else 'AMAN'}")

# Kasus 3: Gempa besar (std besar)
besar = np.array([[15.0, 10.0, 20.0, 18.0, 80.0, 50.0]])
p_besar = model.predict_proba(scaler.transform(besar))[0][1]
ok_gempa = p_besar >= 0.5
print(f"    Gempa besar (std~15 m/s²):  prob={p_besar:.4f} "
      f"→ {'✅ GEMPA' if ok_gempa else '❌ TIDAK TERDETEKSI!'}")

# Kasus 4: Wokwi diam (nilai konstan dari simulator)
# X=0.1g=0.981 m/s², Y=0, Z=1g=9.81 m/s² → semua std ≈ 0
wokwi_diam = np.array([[0, 0, 0, 0, 9.86, 9.86]])
p_wokwi = model.predict_proba(scaler.transform(wokwi_diam))[0][1]
print(f"    Wokwi diam (std=0, total≈9.86): prob={p_wokwi:.4f} "
      f"→ {'✅ AMAN' if p_wokwi<0.5 else '❌ MASALAH!'}")

intercept_prob = 1 / (1 + np.exp(-model.intercept_[0]))
print(f"\n    Sigmoid(intercept saja): {intercept_prob:.4f} "
      f"{'⚠ intercept bias!' if intercept_prob > 0.8 else '✅ OK'}")

if not ok_diam:
    print("\n    ⚠ PERINGATAN: Model bias ke GEMPA saat diam!")
    print("    → Coba naikkan C atau gunakan class_weight='balanced'")

# ================================================================
# 9. VISUALISASI (simpan ke file)
# ================================================================
print("\n[9] Menyimpan visualisasi...")
fig, axes = plt.subplots(1, 3, figsize=(15, 4))

# Plot 1: Distribusi fitur total_std per kelas
feat_df.boxplot(column='total_std', by='label', ax=axes[0])
axes[0].set_title('total_std per kelas')
axes[0].set_xlabel('Kelas (0=AMAN, 1=GEMPA)')
axes[0].set_ylabel('m/s²')

# Plot 2: Distribusi total_max per kelas
feat_df.boxplot(column='total_max', by='label', ax=axes[1])
axes[1].set_title('total_max per kelas')
axes[1].set_xlabel('Kelas (0=AMAN, 1=GEMPA)')
axes[1].set_ylabel('m/s²')

# Plot 3: Confusion matrix
im = axes[2].imshow(cm, cmap='Blues')
axes[2].set_xticks([0,1]); axes[2].set_yticks([0,1])
axes[2].set_xticklabels(['Pred AMAN','Pred GEMPA'])
axes[2].set_yticklabels(['Aktual AMAN','Aktual GEMPA'])
for i in range(2):
    for j in range(2):
        axes[2].text(j, i, str(cm[i,j]),
                     ha='center', va='center', fontsize=14, fontweight='bold')
axes[2].set_title('Confusion Matrix')
plt.colorbar(im, ax=axes[2])

plt.tight_layout()
os.makedirs('doc', exist_ok=True)
plt.savefig('doc/model_evaluation.png', dpi=120, bbox_inches='tight')
print("    Disimpan ke: doc/model_evaluation.png")

# ================================================================
# 10. EXPORT KE earthquake_model.h
# ================================================================
print(f"\n[10] Export ke {OUTPUT_H}")
os.makedirs(os.path.dirname(OUTPUT_H), exist_ok=True)

def fmt_arr(arr):
    return ', '.join(f'{v:.6f}' for v in arr)

header_content = f"""#ifndef EARTHQUAKE_MODEL_H
#define EARTHQUAKE_MODEL_H

// ============================================================
// Model: Logistic Regression untuk deteksi gempa
// Fitur : 6 fitur berbasis STD dalam satuan m/s²
//         [ax_std, ay_std, az_std, total_std, total_max, total_mean]
// Data  : {len(df)} sampel raw → {len(X)} window (size={WINDOW_SIZE}, step={STEP})
// Akurasi test: {(y_pred==y_test).mean()*100:.1f}%
// Satuan: m/s² (sama dengan output Adafruit MPU6050 library)
// Catatan: STD tidak bergantung offset/gravitasi → works di
//          Wokwi simulator & sensor fisik
// ============================================================

// StandardScaler — fit pada training set
// Urutan fitur: ax_std, ay_std, az_std, total_std, total_max, total_mean
const float scaler_mean[]  = {{ {fmt_arr(scaler.mean_)} }};
const float scaler_scale[] = {{ {fmt_arr(scaler.scale_)} }};
const int   scaler_n_features = 6;

// Logistic Regression weights
const float model_coef[]    = {{ {fmt_arr(model.coef_[0])} }};
const float model_intercept = {model.intercept_[0]:.6f}f;

// Konfigurasi inferensi
const float PREDICTION_THRESHOLD = 0.5f;
const float THRESHOLD   = 0.5f;
const int   WINDOW_SIZE = {WINDOW_SIZE};  // harus sama dengan STEP di Python

#endif // EARTHQUAKE_MODEL_H
"""

with open(OUTPUT_H, 'w', encoding='utf-8') as f:
    f.write(header_content)

print(f"    ✅ File berhasil dibuat: {OUTPUT_H}")
print("\n" + "=" * 55)
print("  SELESAI — Salin src/earthquake_model.h ke project Arduino")
print("=" * 55)