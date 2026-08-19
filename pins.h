#pragma once

// I2S DAC (PCM5102 breakout)
#define PIN_I2S_BCK  26
#define PIN_I2S_LRCK 25
#define PIN_I2S_DOUT 27

// OLED (SSD1306 128x64, I2C) — uses default Wire pins on ESP32
#define PIN_OLED_SDA 21
#define PIN_OLED_SCL 22
#define OLED_WIDTH   128
#define OLED_HEIGHT  64
#define OLED_I2C_ADDR 0x3C
const int OLED_ROW_HEIGHT_PX = 16; // per-oscillator row spacing, below the title row at y=0

// Potentiometers: 3 volume + 3 pitch, all ADC1-only pins
#define PIN_POT_VOL1 32
#define PIN_POT_VOL2 33
#define PIN_POT_VOL3 34
#define PIN_POT_PITCH1 35
#define PIN_POT_PITCH2 36 // silkscreen VP
#define PIN_POT_PITCH3 39 // silkscreen VN

// Audio constants
#define NUM_OSCILLATORS   3
#define SAMPLE_RATE_HZ    44100
#define SINE_TABLE_SIZE   256
#define SINE_TABLE_AMPLITUDE 9000 // keeps 3-osc full-volume sum inside int16_t range

// Pitch mapping range (Hz), exponential across the pot sweep
#define FREQ_MIN_HZ 80.0f
#define FREQ_MAX_HZ 1000.0f

// ADC resolution — 12 is the ESP32 hardware ADC's native/max bit depth
const int ADC_RESOLUTION_BITS = 12;
const int ADC_MAX_COUNT = (1 << ADC_RESOLUTION_BITS) - 1;

// ADC smoothing
#define ADC_OVERSAMPLE_COUNT 8    // samples averaged per read, cuts per-sample ADC noise
#define ADC_EMA_ALPHA        0.15f // filtered += alpha * (raw - filtered); lower = smoother but slower to respond
#define ADC_HYSTERESIS_COUNTS 4 // above typical ADC noise floor, well below perceptible pot movement

// Display refresh cadence
#define DISPLAY_REFRESH_MS 100

// USB serial debug output baud rate
#define SERIAL_BAUD_RATE 115200
