#ifndef NEURAL_NETWORK_H
#define NEURAL_NETWORK_H

// Ağın sabit boyutları
#define INPUT_SIZE 540
#define HIDDEN_SIZE 16
#define OUTPUT_SIZE 3

// Tahmin fonksiyonunun imzası
int predict_class(float* input_data);

#endif