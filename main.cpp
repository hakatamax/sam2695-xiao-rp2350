/*
 * ============================================================================
 * Project: SAM2695 Emulator for XIAO RP2350
 * File: main.cpp
 * Version: v1.0.3
 * Date: 2026-10-08
 * Description: MIDI / Auto-Play Synthesizer Emulator with Corrected RGB LED Logic
 * ============================================================================
 */

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

// --- ピン配置設定 (XIAO RP2350 Schematics v1.0 準拠) ---
#define AUDIO_PWM_PIN 0    // サウンド出力 (D0 / GPIO 0)

// オンボードRGB LEDピン (XIAO RP2350: Highアクティブ)
#define LED_R_PIN 17       // 赤: GPIO 17
#define LED_G_PIN 16       // 緑: GPIO 16
#define LED_B_PIN 25       // 青: GPIO 25

#define SAMPLE_RATE 44100
#define MAX_VOICES 8

// ドレミファソラシド (C4 〜 C5) のMIDIノート番号
const uint8_t scale_notes[] = {60, 62, 64, 65, 67, 69, 71, 72};
const int num_notes = sizeof(scale_notes) / sizeof(scale_notes[0]);

// LED色テーブル (R, G, B) ※ 1で点灯、0で消灯
const bool led_colors[][3] = {
    {1, 0, 0}, // ド: 赤
    {0, 1, 0}, // レ: 緑
    {0, 0, 1}, // ミ: 青
    {1, 1, 0}, // ファ: 黄
    {0, 1, 1}, // ソ: シアン
    {1, 0, 1}, // ラ: マゼンタ
    {1, 1, 1}, // シ: 白
    {1, 1, 0}  // ド: 黄
};

struct Voice {
    bool active = false;
    uint8_t note = 0;
    float phase = 0.0f;
    float phase_increment = 0.0f;
    float envelope = 0.0f;
    float release_rate = 0.9997f;
};

Voice voices[MAX_VOICES];

// MIDIノート番号から周波数 (Hz) を計算
float noteToFreq(uint8_t note) {
    return 440.0f * powf(2.0f, (note - 69) / 12.0f);
}

// ノート発音
void noteOn(uint8_t note, uint8_t velocity) {
    for (int i = 0; i < MAX_VOICES; i++) {
        if (!voices[i].active) {
            voices[i].active = true;
            voices[i].note = note;
            voices[i].phase = 0.0f;
            
            float freq = noteToFreq(note);
            voices[i].phase_increment = (freq * 2.0f * M_PI) / SAMPLE_RATE;
            voices[i].envelope = (float)velocity / 127.0f;
            break;
        }
    }
}

// サンプル描画 (PWM音響出力)
float renderSample() {
    float mix = 0.0f;

    for (int i = 0; i < MAX_VOICES; i++) {
        if (!voices[i].active) continue;

        float wave_sq = (voices[i].phase < M_PI) ? 0.7f : -0.7f;
        float wave_sin = sinf(voices[i].phase);
        float sample = (wave_sq + wave_sin) * 0.5f;

        mix += sample * voices[i].envelope;

        voices[i].phase += voices[i].phase_increment;
        if (voices[i].phase >= 2.0f * M_PI) {
            voices[i].phase -= 2.0f * M_PI;
        }

        voices[i].envelope *= voices[i].release_rate;
        if (voices[i].envelope < 0.001f) {
            voices[i].active = false;
        }
    }

    return mix * 0.4f;
}

// --- タイマー割り込み（PWM音声出力） ---
struct repeating_timer timer;
uint pwm_slice_num;
uint pwm_chan_num;

bool audio_timer_callback(struct repeating_timer *t) {
    float sample = renderSample();
    int pwm_val = (int)((sample + 1.0f) * 127.5f);
    if (pwm_val < 0) pwm_val = 0;
    if (pwm_val > 255) pwm_val = 255;

    pwm_set_chan_level(pwm_slice_num, pwm_chan_num, pwm_val);
    return true;
}

// LEDの色を設定する関数
void set_led_color(bool r, bool g, bool b) {
    gpio_put(LED_R_PIN, r);
    gpio_put(LED_G_PIN, g);
    gpio_put(LED_B_PIN, b);
}

int main() {
    stdio_init_all();

    // LEDピンの初期化
    gpio_init(LED_R_PIN); gpio_set_dir(LED_R_PIN, GPIO_OUT);
    gpio_init(LED_G_PIN); gpio_set_dir(LED_G_PIN, GPIO_OUT);
    gpio_init(LED_B_PIN); gpio_set_dir(LED_B_PIN, GPIO_OUT);
    set_led_color(0, 0, 0); // 初期状態は消灯 (0)

    // オーディオPWM設定 (GPIO 0)
    gpio_set_function(AUDIO_PWM_PIN, GPIO_FUNC_PWM);
    pwm_slice_num = pwm_gpio_to_slice_num(AUDIO_PWM_PIN);
    pwm_chan_num = pwm_gpio_to_channel(AUDIO_PWM_PIN);

    pwm_config config = pwm_get_default_config();
    pwm_config_set_wrap(&config, 255);
    pwm_init(pwm_slice_num, &config, true);

    // 44.1kHz オーディオタイマー開始
    add_repeating_timer_us(-23, audio_timer_callback, NULL, &timer);

    int note_index = 0;

    // メインループ: 1秒ごとにLEDを正しく色変更しながら発音
    while (1) {
        set_led_color(led_colors[note_index][0], 
                      led_colors[note_index][1], 
                      led_colors[note_index][2]);

        noteOn(scale_notes[note_index], 127);

        note_index = (note_index + 1) % num_notes;
        sleep_ms(1000);
    }

    return 0;
}
