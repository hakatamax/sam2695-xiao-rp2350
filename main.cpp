/*
 * ============================================================================
 * Project: SAM2695 Emulator for XIAO RP2350
 * File: main.cpp
 * Version: v1.0.1
 * Date: 2026-10-08
 * Description: MIDI / Auto-Play Synthesizer Emulator using PWM Audio Output
 * ============================================================================
 */

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

// --- ピン配置設定 (XIAO RP2350) ---
#define AUDIO_PWM_PIN 0    // サウンド出力 (D0 / GPIO 0)

#define SAMPLE_RATE 44100
#define MAX_VOICES 8

// ドレミファソラシド (C4 〜 C5) のMIDIノート番号
const uint8_t scale_notes[] = {60, 62, 64, 65, 67, 69, 71, 72};
const int num_notes = sizeof(scale_notes) / sizeof(scale_notes[0]);

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

// サンプル描画 (矩形波+サイン波で音量を確保)
float renderSample() {
    float mix = 0.0f;

    for (int i = 0; i < MAX_VOICES; i++) {
        if (!voices[i].active) continue;

        // 音量をしっかり出すため矩形波とサイン波を合成
        float wave_sq = (voices[i].phase < M_PI) ? 0.6f : -0.6f;
        float wave_sin = sinf(voices[i].phase);
        float sample = (wave_sq + wave_sin) * 0.5f;

        mix += sample * voices[i].envelope;

        // 位相更新
        voices[i].phase += voices[i].phase_increment;
        if (voices[i].phase >= 2.0f * M_PI) {
            voices[i].phase -= 2.0f * M_PI;
        }

        // エンベロープ減衰
        voices[i].envelope *= voices[i].release_rate;
        if (voices[i].envelope < 0.001f) {
            voices[i].active = false;
        }
    }

    return mix * 0.3f; // 音量ゲイン調整
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

int main() {
    stdio_init_all();

    // オーディオPWM設定 (GPIO 0)
    gpio_set_function(AUDIO_PWM_PIN, GPIO_FUNC_PWM);
    pwm_slice_num = pwm_gpio_to_slice_num(AUDIO_PWM_PIN);
    pwm_chan_num = pwm_gpio_to_channel(AUDIO_PWM_PIN);

    pwm_config config = pwm_get_default_config();
    pwm_config_set_wrap(&config, 255); // 8-bit 分解能
    pwm_init(pwm_slice_num, &config, true);

    // 44.1kHz オーディオタイマー開始
    add_repeating_timer_us(-23, audio_timer_callback, NULL, &timer);

    int note_index = 0;

    // 1秒ごとにドレミファソラシドを自動再生
    while (1) {
        noteOn(scale_notes[note_index], 127);
        note_index = (note_index + 1) % num_notes;
        sleep_ms(1000);
    }

    return 0;
}
