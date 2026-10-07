#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

// ピン配置設定 (XIAO RP2350 / M5Stack Unit MIDI互換)
#define MIDI_UART_ID uart0
#define MIDI_BAUD_RATE 31250
#define UART_RX_PIN 7      // XIAO RP2350 D7 (M5Stack Unit-MIDI RX)
#define AUDIO_PWM_PIN 0    // XIAO RP2350 D0 (オーディオ出力)

#define SAMPLE_RATE 44100
#define MAX_VOICES 32

struct Voice {
    bool active = false;
    uint8_t channel = 0;
    uint8_t note = 0;
    uint8_t velocity = 0;
    float phase = 0.0f;
    float phase_increment = 0.0f;
    float envelope = 0.0f;
    float release_rate = 0.9995f;
};

Voice voices[MAX_VOICES];
uint8_t current_program[16] = {0};

float noteToFreq(uint8_t note) {
    return 440.0f * powf(2.0f, (note - 69) / 12.0f);
}

void noteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
    if (velocity == 0) {
        for (int i = 0; i < MAX_VOICES; i++) {
            if (voices[i].active && voices[i].channel == channel && voices[i].note == note) {
                voices[i].active = false;
            }
        }
        return;
    }

    for (int i = 0; i < MAX_VOICES; i++) {
        if (!voices[i].active) {
            voices[i].active = true;
            voices[i].channel = channel;
            voices[i].note = note;
            voices[i].velocity = velocity;
            voices[i].phase = 0.0f;
            
            float freq = noteToFreq(note);
            voices[i].phase_increment = (freq * 2.0f * M_PI) / SAMPLE_RATE;
            voices[i].envelope = (float)velocity / 127.0f;
            break;
        }
    }
}

void noteOff(uint8_t channel, uint8_t note) {
    for (int i = 0; i < MAX_VOICES; i++) {
        if (voices[i].active && voices[i].channel == channel && voices[i].note == note) {
            voices[i].active = false;
        }
    }
}

void processMidiByte(uint8_t b) {
    static uint8_t status = 0;
    static uint8_t data1 = 0;
    static uint8_t state = 0;

    if (b >= 0x80) {
        status = b;
        state = 1;
        return;
    }

    uint8_t cmd = status & 0xF0;
    uint8_t ch = status & 0x0F;

    if (state == 1) {
        data1 = b;
        if (cmd == 0xC0 || cmd == 0xD0) {
            if (cmd == 0xC0) current_program[ch] = data1;
            state = 1;
        } else {
            state = 2;
        }
    } else if (state == 2) {
        uint8_t data2 = b;
        if (cmd == 0x90) {
            noteOn(ch, data1, data2);
        } else if (cmd == 0x80) {
            noteOff(ch, data1);
        }
        state = 1;
    }
}

float renderSample() {
    float mix = 0.0f;

    for (int i = 0; i < MAX_VOICES; i++) {
        if (!voices[i].active) continue;

        uint8_t prog = current_program[voices[i].channel];
        float sample = 0.0f;

        if (voices[i].channel == 9) { // Drum Track
            sample = ((float)rand() / RAND_MAX - 0.5f) * voices[i].envelope;
        } else if (prog < 8) { // Piano
            sample = (sinf(voices[i].phase) + 0.5f * sinf(2.0f * voices[i].phase)) * 0.5f;
        } else if (prog >= 16 && prog < 24) { // Organ
            sample = sinf(voices[i].phase) + 0.5f * sinf(3.0f * voices[i].phase);
        } else { // Synth/Others
            sample = (voices[i].phase < M_PI) ? 0.5f : -0.5f;
        }

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

    return mix * 0.1f;
}

struct repeating_timer timer;
uint pwm_slice_num;

bool audio_timer_callback(struct repeating_timer *t) {
    float sample = renderSample();
    int pwm_val = (int)((sample + 1.0f) * 127.5f);
    if (pwm_val < 0) pwm_val = 0;
    if (pwm_val > 255) pwm_val = 255;

    pwm_set_chan_level(pwm_slice_num, PWM_CHAN_A, pwm_val);
    return true;
}

int main() {
    stdio_init_all();

    uart_init(MIDI_UART_ID, MIDI_BAUD_RATE);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    gpio_set_function(AUDIO_PWM_PIN, GPIO_FUNC_PWM);
    pwm_slice_num = pwm_gpio_to_slice_num(AUDIO_PWM_PIN);

    pwm_config config = pwm_get_default_config();
    pwm_config_set_wrap(&config, 255);
    pwm_init(pwm_slice_num, &config, true);

    add_repeating_timer_us(-23, audio_timer_callback, NULL, &timer);

    while (1) {
        while (uart_is_readable(MIDI_UART_ID)) {
            uint8_t ch = uart_getc(MIDI_UART_ID);
            processMidiByte(ch);
        }
        tight_loop_contents();
    }

    return 0;
}
