#ifndef EARTHQUAKE_MODEL_H
#define EARTHQUAKE_MODEL_H

// StandardScaler parameters
const float scaler_mean[] = { 111.680169, 6446.870624, -1169.948819, 3837.688486, -14688.472690, 10030.340813, 21895.334035, 31770.403047, 5296.642645 };
const float scaler_scale[] = { 1882.046638, 6915.966171, 936.146138, 4149.974169, 3891.831744, 10270.193204, 5493.889994, 14508.275547, 5146.054386 };
const int scaler_n_features = 9;

// Logistic Regression weights
const float model_coef[] = { -0.010993, 1.646546, 0.456876, 1.793364, 1.846339, 1.828136, -0.232046, 0.834225, 2.502970 };
const float model_intercept = 3.335228;
const float THRESHOLD = 0.5f;

#endif // EARTHQUAKE_MODEL_H
