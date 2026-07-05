#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <float.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_err.h"

#include "neural_network.h"

#define UART_PORT_NUM UART_NUM_1
#define UART_BAUD_RATE 115200

#define RXD_PIN GPIO_NUM_16
#define TXD_PIN GPIO_NUM_17

#define RX_BUF_SIZE        2048
#define LINE_BUF_SIZE      64
#define WINDOW_SAMPLES     (INPUT_SIZE / 3)

// stm32 sends centi-g: accel_g * 100
#define VALUE_SCALE_CG_TO_G 100.0f

// prevents division by zero if std is too low during relative normalization
#define AXIS_STD_FLOOR_G 0.05f

// motion trigger settings. adjust between 18-35 if necessary
#define MOTION_DELTA_THRESHOLD_CG 18.0f
#define AFTER_PREDICT_COOLDOWN_MS 1200

static uint8_t rx_data[RX_BUF_SIZE];
static char line_buf[LINE_BUF_SIZE];
static int line_len = 0;

// stores raw window in g first; then normalizes the same array for the model
static float sensor_data[INPUT_SIZE];
static int sample_count = 0;
static bool collecting = false;
static int64_t cooldown_until_us = 0;

static bool have_last_sample = false;
static float last_x_cg = 0.0f;
static float last_y_cg = 0.0f;
static float last_z_cg = 0.0f;

// globally available in neural_network.c. used to print scores
extern float output_layer[OUTPUT_SIZE];

static const char *class_name(int class_id)
{
    switch (class_id) {
    case 0: return "O";
    case 1: return "Z";
    case 2: return "I";
    default: return "?";
    }
}

static bool parse_sensor_line(const char *line, float *x_cg, float *y_cg, float *z_cg)
{
    return sscanf(line, "%f,%f,%f", x_cg, y_cg, z_cg) == 3;
}

static void reset_window(void)
{
    sample_count = 0;
    collecting = false;
}

static bool motion_started(float x_cg, float y_cg, float z_cg)
{
    if (!have_last_sample) {
        last_x_cg = x_cg;
        last_y_cg = y_cg;
        last_z_cg = z_cg;
        have_last_sample = true;
        return false;
    }

    const float dx = x_cg - last_x_cg;
    const float dy = y_cg - last_y_cg;
    const float dz = z_cg - last_z_cg;

    last_x_cg = x_cg;
    last_y_cg = y_cg;
    last_z_cg = z_cg;

    const float delta = sqrtf((dx * dx) + (dy * dy) + (dz * dz));
    return delta >= MOTION_DELTA_THRESHOLD_CG;
}

static void print_raw_window_stats_cg(void)
{
    float min_v[3] = {FLT_MAX, FLT_MAX, FLT_MAX};
    float max_v[3] = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
    float sum_v[3] = {0.0f, 0.0f, 0.0f};

    for (int s = 0; s < WINDOW_SAMPLES; s++) {
        for (int a = 0; a < 3; a++) {
            const float cg = sensor_data[(s * 3) + a] * VALUE_SCALE_CG_TO_G;
            sum_v[a] += cg;
            if (cg < min_v[a]) {
                min_v[a] = cg;
            }
            if (cg > max_v[a]) {
                max_v[a] = cg;
            }
        }
    }

    printf("raw window stats in centi-g:\n");
    printf("  X mean=%7.2f min=%7.2f max=%7.2f\n", sum_v[0] / WINDOW_SAMPLES, min_v[0], max_v[0]);
    printf("  Y mean=%7.2f min=%7.2f max=%7.2f\n", sum_v[1] / WINDOW_SAMPLES, min_v[1], max_v[1]);
    printf("  Z mean=%7.2f min=%7.2f max=%7.2f\n", sum_v[2] / WINDOW_SAMPLES, min_v[2], max_v[2]);
}

static void normalize_window_relative(void)
{
    float mean[3] = {0.0f, 0.0f, 0.0f};
    float var[3] = {0.0f, 0.0f, 0.0f};
    float std[3] = {AXIS_STD_FLOOR_G, AXIS_STD_FLOOR_G, AXIS_STD_FLOOR_G};

    for (int s = 0; s < WINDOW_SAMPLES; s++) {
        for (int a = 0; a < 3; a++) {
            mean[a] += sensor_data[(s * 3) + a];
        }
    }

    for (int a = 0; a < 3; a++) {
        mean[a] /= (float)WINDOW_SAMPLES;
    }

    for (int s = 0; s < WINDOW_SAMPLES; s++) {
        for (int a = 0; a < 3; a++) {
            const float d = sensor_data[(s * 3) + a] - mean[a];
            var[a] += d * d;
        }
    }

    for (int a = 0; a < 3; a++) {
        std[a] = sqrtf(var[a] / (float)WINDOW_SAMPLES);
        if (std[a] < AXIS_STD_FLOOR_G) {
            std[a] = AXIS_STD_FLOOR_G;
        }
    }

    printf("relative normalization: mean_g=[%.3f %.3f %.3f], std_g=[%.3f %.3f %.3f]\n",
           mean[0], mean[1], mean[2], std[0], std[1], std[2]);

    for (int s = 0; s < WINDOW_SAMPLES; s++) {
        for (int a = 0; a < 3; a++) {
            const int idx = (s * 3) + a;
            sensor_data[idx] = (sensor_data[idx] - mean[a]) / std[a];
        }
    }
}
//calculate possibiliteis
static void softmax3(const float logits[OUTPUT_SIZE], float probs[OUTPUT_SIZE])
{
    float max_logit = logits[0];
    for (int i = 1; i < OUTPUT_SIZE; i++) {
        if (logits[i] > max_logit) {
            max_logit = logits[i];
        }
    }

    float sum = 0.0f;
    for (int i = 0; i < OUTPUT_SIZE; i++) {
        probs[i] = expf(logits[i] - max_logit);
        sum += probs[i];
    }

    if (sum <= 0.0f) {
        probs[0] = probs[1] = probs[2] = 1.0f / 3.0f;
        return;
    }

    for (int i = 0; i < OUTPUT_SIZE; i++) {
        probs[i] /= sum;
    }
}

static void add_sample_and_maybe_predict(float x_cg, float y_cg, float z_cg)
{
    if (sample_count < 0 || sample_count >= WINDOW_SAMPLES) {
        reset_window();
    }

    const int base = sample_count * 3;
    sensor_data[base + 0] = x_cg / VALUE_SCALE_CG_TO_G;
    sensor_data[base + 1] = y_cg / VALUE_SCALE_CG_TO_G;
    sensor_data[base + 2] = z_cg / VALUE_SCALE_CG_TO_G;
    sample_count++;

    if (sample_count == 1 || sample_count == 60 || sample_count == 120 || sample_count == 179) {
        printf("window: %d/%d samples\n", sample_count, WINDOW_SAMPLES);
    }

    if (sample_count < WINDOW_SAMPLES) {
        return;
    }

    print_raw_window_stats_cg();
    normalize_window_relative();

    const int64_t start_us = esp_timer_get_time();
    const int predicted_class = predict_class(sensor_data);
    const int64_t end_us = esp_timer_get_time();

    float probs[OUTPUT_SIZE];
    softmax3(output_layer, probs);

    printf("scores: O=% .4f Z=% .4f I=% .4f\n", output_layer[0], output_layer[1], output_layer[2]);
    printf("probabilities: O=%5.1f%% Z=%5.1f%% I=%5.1f%%\n", probs[0] * 100.0f, probs[1] * 100.0f, probs[2] * 100.0f);
    printf("prediction: %s (class=%d) | time: %lld us\n\n",
           class_name(predicted_class), predicted_class, (long long)(end_us - start_us));

    reset_window();
    cooldown_until_us = esp_timer_get_time() + ((int64_t)AFTER_PREDICT_COOLDOWN_MS * 1000LL);
}

static void handle_sensor_sample(float x_cg, float y_cg, float z_cg)
{
    const int64_t now_us = esp_timer_get_time();

    if (!collecting) {
        if (now_us < cooldown_until_us) {
            last_x_cg = x_cg;
            last_y_cg = y_cg;
            last_z_cg = z_cg;
            have_last_sample = true;
            return;
        }

        if (!motion_started(x_cg, y_cg, z_cg)) {
            return;
        }

        collecting = true;
        sample_count = 0;
        printf("motion detected, collecting 180 samples...\n");
    }

    add_sample_and_maybe_predict(x_cg, y_cg, z_cg);
}

static void process_rx_bytes(const uint8_t *buf, int len)
{
    for (int i = 0; i < len; i++) {
        const char c = (char)buf[i];

        if (c == '\r') {
            continue;
        }

        if (c == '\n') {
            if (line_len > 0) {
                line_buf[line_len] = '\0';

                float x_cg = 0.0f;
                float y_cg = 0.0f;
                float z_cg = 0.0f;

                if (parse_sensor_line(line_buf, &x_cg, &y_cg, &z_cg)) {
                    handle_sensor_sample(x_cg, y_cg, z_cg);
                }

                line_len = 0;
            }
            continue;
        }

        if (line_len < LINE_BUF_SIZE - 1) {
            line_buf[line_len++] = c;
        } else {
            // discard if line is broken until new line
            line_len = 0;
        }
    }
}

void app_main(void)
{
    const uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, RX_BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    printf("=========================================\n");
    printf("esp32-s3 inference mode ready\n");
    printf("stm32 format: accel_x*100,accel_y*100,accel_z*100\n");
    printf("preprocessing: per-window/per-axis relative normalization\n");
    printf("uart1 rx=gpio16, baud=115200\n");
    printf("=========================================\n");

    while (1) {
        const int rx_bytes = uart_read_bytes(UART_PORT_NUM, rx_data, RX_BUF_SIZE, pdMS_TO_TICKS(100));
        if (rx_bytes > 0) {
            process_rx_bytes(rx_data, rx_bytes);
        }
    }
}