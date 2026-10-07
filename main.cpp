/*
 * ============================================================================
 * Project: SAM2695 Emulator for XIAO RP2350
 * File: main.cpp
 * Version: v1.2.1 (Fully Fixed & Self-Contained)
 * Description: WS2812 RGB LED (GPIO20) & MIDI Pitch Generator (D0 / GPIO0)
 * ============================================================================
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

// --- ハードウェアピン定義 (Seeed Studio XIAO RP2350 公式仕様) ---
#define AUDIO_PWM_PIN    0    // D0 (GPIO 0) 音声をPWM波形出力
#define WS2812_POWER_PIN 23   // RGB LED 電源制御ピン (HIGHでVCC供給)
#define WS2812_DATA_PIN  20   // WS2812 データ信号ピン (GPIO 20)

// MIDI C4 〜 C5 周波数 (Hz)
const float midi_freqs[] = {
    261.63f, // C4 (ド)
    293.66f, // D4 (レ)
    329.63f, // E4 (ミ)
    349.23f, // F4 (ファ)
    392.00f, // G4 (ソ)
    440.00f, // A4 (ラ)
    493.88f, // B4 (シ)
    523.25f  // C5 (ド)
};
const int scale_length = sizeof(midi_freqs) / sizeof(midi_freqs[0]);

// WS2812用 カラーテーブル (R, G, B) 各0〜255
const uint8_t color_table[][3] = {
    {100,   0,   0}, // 赤
    {  0, 100,   0}, // 緑
    {  0,   0, 100}, // 青
    {100, 100,   0}, // 黄
    {  0, 100, 100}, // シアン
    {100,   0, 100}, // マゼンタ
    {100, 100, 100}, // 白
    { 80,  50,   0}  // オレンジ
};

// WS2812 タイミング制御 (標準GPIOビットバン制御)
void __no_inline_not_in_flash_func(ws2812_put_pixel)(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t grb = ((uint32_t)g << 16) | ((uint32_t)r << 8) | (uint32_t)b;

    for (int i = 23; i >= 0; i--) {
        if ((grb >> i) & 1) {
            gpio_put(WS2812_DATA_PIN, 1);
            sleep_us(1);
            gpio_put(WS2812_DATA_PIN, 0);
            sleep_us(1);
        } else {
            gpio_put(WS2812_DATA_PIN, 1);
            asm volatile("nop\nnop\nnop\nnop\n");
            gpio_put(WS2812_DATA_PIN, 0);
            sleep_us(1);
        }
    }
    sleep_us(60); // Reset パルス (>50us)
}

// ハードウェアPWM周波数制御 (D0ピン)
void play_hardware_tone(float freq_hz, uint slice_num, uint chan_num) {
    if (freq_hz <= 0.0f) {
        pwm_set_chan_level(slice_num, chan_num, 0);
        return;
    }

    uint32_t system_clock = clock_get_hz(clk_sys);
    float clock_div = 64.0f;
    uint32_t wrap = (uint32_t)((float)system_clock / (clock_div * freq_hz)) - 1;

    pwm_set_clkdiv(slice_num, clock_div);
    pwm_set_wrap(slice_num, wrap);
    pwm_set_chan_level(slice_num, chan_num, wrap / 2); // デューティ比50%
    pwm_set_enabled(slice_num, true);
}

int main() {
    stdio_init_all();

    // 1. WS2812 電源ピン (GPIO 23) を HIGH にセットしてLEDへ電力を供給
    gpio_init(WS2812_POWER_PIN);
    gpio_set_dir(WS2812_POWER_PIN, GPIO_OUT);
    gpio_put(WS2812_POWER_PIN, 1);

    // 2. WS2812 データピン (GPIO 20) の初期化
    gpio_init(WS2812_DATA_PIN);
    gpio_set_dir(WS2812_DATA_PIN, GPIO_OUT);
    gpio_put(WS2812_DATA_PIN, 0);

    // 3. D0 (GPIO 0) PWM 設定
    gpio_set_function(AUDIO_PWM_PIN, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(AUDIO_PWM_PIN);
    uint chan_num = pwm_gpio_to_channel(AUDIO_PWM_PIN);

    int note_idx = 0;

    // メインループ: ドレミファソラシドを鳴らしつつ、WS2812の色を変更
    while (1) {
        // WS2812 にカラー送信 (R, G, B)
        ws2812_put_pixel(color_table[note_idx][0],
                         color_table[note_idx][1],
                         color_table[note_idx][2]);

        // D0 から MIDI トーン出力 (0.5秒)
        play_hardware_tone(midi_freqs[note_idx], slice_num, chan_num);
        sleep_ms(500);

        // 消音 (0.1秒)
        play_hardware_tone(0.0f, slice_num, chan_num);
        sleep_ms(100);

        note_idx = (note_idx + 1) % scale_length;
    }

    return 0;
}
