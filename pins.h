#pragma once

// Pin map for the ESP32-C3 SuperMini board (QFN32, GPIO0-10/18-21 broken
// out, GPIO11-17 reserved for internal flash). GPIO8 is the onboard LED and
// GPIO9 is the BOOT button on this board — both left free of other duties.
// The rest stays within GPIO0-10 to avoid the C3's dedicated
// USB-Serial/JTAG pins (GPIO18/19).

// I2S DAC (PCM5102 breakout)
#define PIN_I2S_BCK  6
#define PIN_I2S_LRCK 7
#define PIN_I2S_DOUT 10

// OLED (SSD1306 128x64, I2C) — moved off the board's default 8/9 since
// those are the onboard LED and BOOT button here, not general-purpose.
#define PIN_OLED_SDA 20
#define PIN_OLED_SCL 21
#define OLED_WIDTH   128
#define OLED_HEIGHT  64
#define OLED_I2C_ADDR 0x3C
const int OLED_ROW_HEIGHT_PX = 16; // per-oscillator row spacing, below the title row at y=0

// Potentiometers: 3 volume + 3 pitch. The C3 only has 6 ADC-capable pins
// total (ADC1: GPIO0-4, ADC2: GPIO5), so all 6 are used here; ADC2 is safe
// only because this project never enables WiFi.
#define PIN_POT_VOL1 0
#define PIN_POT_VOL2 1
#define PIN_POT_VOL3 2  // strapping pin; a pot wiper doesn't affect boot mode
#define PIN_POT_PITCH1 3
#define PIN_POT_PITCH2 4
#define PIN_POT_PITCH3 5 // ADC2 channel 0 — the C3's only ADC2 pin

// Liveness LED — the SuperMini's onboard LED. Active-low: LOW turns it on.
#define PIN_STATUS_LED 8

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

// Liveness-blink half-period — PIN_STATUS_LED toggles every this many ms,
// giving a ~1Hz blink independent of DISPLAY_REFRESH_MS.
#define BLINK_HALF_PERIOD_MS 500

// USB serial debug output baud rate
#define SERIAL_BAUD_RATE 115200
