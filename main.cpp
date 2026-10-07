/*
 * ============================================================================
 * Project: SAM2695 Emulator / Hardware Test for XIAO RP2350
 * File: main.cpp
 * Version: v1.1.2 (Fully Verified)
 * Description: MIDI Note Synthesizer (D0/GPIO 0) with Corrected Low-Active RGB LED
 * ============================================================================
 */

#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"

// --- ピン定義 (XIAO RP2350 公式仕様) ---
#define AUDIO_PWM_PIN 0    // D0 (GPIO 0) 音声出力

// 右上「RGB」表記のオンボードLED (Low-Active: 0=ON, 1=OFF)
#define LED_R_PIN 17       // Red: GPIO 17
#define LED_G_PIN 16       // Green: GPIO 16
#define LED_B_PIN 25       // Blue: GPIO 25

#define SAMPLE_RATE 44100

// MIDI Note番号: C4(60), D4(62), E4(64), F4(65), G4(67), A4(69), B4(71), C5(72)
const uint8_t midi_scale[] = {60, 62, 64, 65, 67, 69, 71, 72};
const int scale_length = sizeof(midi_scale) / sizeof(midi_scale[0]);

// RGB LED 色パターン (Low-Active のため 0 が点灯, 1 が消灯)
const bool rgb_colors[][3] = {
    {0, 1, 1}, // ド: 赤
    {1, 0, 1}, // レ: 緑
    {1, 1, 0}, // ミ: 青
    {0, 0, 1}, // ファ: 黄 (赤+緑)
    {1, 0, 0}, // ソ: シアン (緑+青)
    {0, 1, 0}, // ラ: マゼンタ (赤+青)
    {0, 0, 0}, // シ: 白 (全点灯)
    {0, 0, 1}  // ド: 黄
};

// MIDIノート番号から周波数(Hz)への変換関数
float midiNoteToFreq(uint8_t note) {
    return 440.0f * powf(2.0f, (float)(note - 69) / 12.0f);
}

// LED制御用関数 (Low-Active)
void set_rgb_led(bool r, bool g, bool b) {
    gpio_put(LED_R_PIN, r);
    gpio_put(LED_G_PIN, g);
    gpio_put(LED_B_PIN, b);
}

// 音声出力タイマー変数
struct repeating_timer timer;
uint pwm_slice_num;
uint pwm_chan_num;

static float current_phase = 0.0f;
static float phase_inc = 0.0f;
static bool note_playing = false;

// 44.1kHz タイマー割り込み（PWMサンプル出力）
bool audio_timer_callback(struct repeating_timer *t) {
    if (!note_playing) {
        pwm_set_chan_level(pwm_slice_num, pwm_chan_num, 128);
        return true;
    }

    // サイン波音源生成
    float sample = sinf(current_phase);
    current_phase += phase_inc;
    if (current_phase >= 2.0f * M_PI) {
        current_phase -= 2.0f * M_PI;
    }

    // PWMデューティ比変換 (0〜255)
    int pwm_val = (int)((sample + 1.0f) * 127.5f);
    if (pwm_val < 0) pwm_val = 0;
    if (pwm_val > 255) pwm_val = 255;

    pwm_set_chan_level(pwm_slice_num, pwm_chan_num, pwm_val);
    return true;
}

int main() {
    stdio_init_all();

    // 1. RGB LED ピンの初期化 (Low-Activeのため初期値 1=消灯)
    gpio_init(LED_R_PIN); gpio_set_dir(LED_R_PIN, GPIO_OUT); gpio_put(LED_R_PIN, 1);
    gpio_init(LED_G_PIN); gpio_set_dir(LED_G_PIN, GPIO_OUT); gpio_put(LED_G_PIN, 1);
    gpio_init(LED_B_PIN); gpio_set_dir(LED_B_PIN, GPIO_OUT); gpio_put(LED_B_PIN, 1);

    // 2. D0 ピン (GPIO 0) PWM オーディオ設定
    gpio_set_function(AUDIO_PWM_PIN, GPIO_FUNC_PWM);
    pwm_slice_num = pwm_gpio_to_slice_num(AUDIO_PWM_PIN);
    pwm_chan_num = pwm_gpio_to_channel(AUDIO_PWM_PIN);

    pwm_config config = pwm_get_default_config();
    pwm_config_set_wrap(&config, 255);
    pwm_init(pwm_slice_num, &config, true);

    // 44.1kHz 周期タイマー開始 (約22.67μs周期)
    add_repeating_timer_us(-23, audio_timer_callback, NULL, &timer);

    int note_idx = 0;

    // メインループ: ドレミファソラシドを順に鳴らし、RGB LEDを変化させる
    while (1) {
        // LED色変更
        set_rgb_led(rgb_colors[note_idx][0], 
                    rgb_colors[note_idx][1], 
                    rgb_colors[note_idx][2]);

        // MIDI ノート発音
        float freq = midiNoteToFreq(midi_scale[note_idx]);
        phase_inc = (freq * 2.0f * M_PI) / SAMPLE_RATE;
        note_playing = true;

        sleep_ms(600); // 0.6秒発音

        note_playing = false; // 消音
        sleep_ms(100);

        note_idx = (note_idx + 1) % scale_length;
    }

    return 0;
}
