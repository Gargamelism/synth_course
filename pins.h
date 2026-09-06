#pragma once

#include <stdint.h>

// Pin map for the ABRobot ESP32-C3 0.42" OLED dev board (ESP32-C3, QFN32).
// This is NOT a bare SuperMini: it has a built-in SSD1306 OLED hardwired to
// GPIO5 (SDA) / GPIO6 (SCL), GPIO18/19 soldered straight to the USB socket,
// and GPIO20/21 wired as UART0. GPIO8 is the onboard LED, GPIO9 the BOOT
// button, GPIO11-17 the internal flash. That leaves GPIO0-4, 7, 10 general-
// purpose — plus GPIO20/21 once "USB CDC On Boot" is enabled in the Arduino
// IDE (which this project requires: it routes the serial console over native
// USB and frees GPIO20/21 for other use).

// I2S DAC (PCM5102 breakout). BCK sits on GPIO20 — a UART0 pin, free here
// only because USB CDC On Boot moves the console to native USB. Its usual
// home, GPIO6, is the OLED's SCL on this board.
#define PIN_I2S_BCK  20
#define PIN_I2S_LRCK 7
#define PIN_I2S_DOUT 10

// OLED — the board's built-in SSD1306, I2C, on fixed pins (not movable).
#define PIN_OLED_SDA 5
#define PIN_OLED_SCL 6
#define OLED_I2C_ADDR 0x3C

// The SSD1306 controller has a 128x64 GDDRAM buffer, but this board's 0.42"
// glass only shows a 72x40 window into it, offset to (30,12). Adafruit_SSD1306
// has no offset support, so display.cpp shifts every draw by (OLED_X_OFFSET,
// OLED_Y_OFFSET) by hand. A few panels of this type centre at (28,24)
// instead — nudge OLED_X_OFFSET / OLED_Y_OFFSET if the image is clipped.
#define OLED_WIDTH   128  // full SSD1306 buffer, not the visible area
#define OLED_HEIGHT  64
#define OLED_VISIBLE_WIDTH  72
#define OLED_VISIBLE_HEIGHT 40
#define OLED_X_OFFSET 30
#define OLED_Y_OFFSET 24

// Compact status layout inside the visible window (see display.cpp).
const int OLED_STATUS_ROW_TOP_PX     = 12; // first status row's y, from the window top
const int OLED_STATUS_ROW_SPACING_PX = 9;
const int OLED_TEXT_HEIGHT_PX     = 8;  // Adafruit GFX size-1 glyph height
const int OLED_VOL_BAR_X_PX       = 32; // volume bar's left edge, from the window's left

// Two pots for the whole instrument: one MASTER volume and one pitch. Every
// voice in kVoices (voices.h) is pitched relative to this single pitch pot
// and scaled by this single volume pot, so the pot count no longer grows
// with the voice count.
#define PIN_POT_VOL1 0
#define PIN_POT_PITCH1 3

// Liveness LED — the board's onboard LED. Active-low: LOW turns it on.
#define PIN_STATUS_LED 8

// Audio on/off toggle switch — SPST, wired between this pin and GND, read
// with the internal pull-up (INPUT_PULLUP): open = HIGH = off (muted),
// closed = LOW = on. GPIO21 is UART0 TX, free here for the same reason
// GPIO20 is free for I2S BCK — "USB CDC On Boot" moves the console off
// UART0.
#define PIN_AUDIO_SWITCH 21

// Audio constants. NUM_VOICES is not here — it comes from the kVoices table
// in voices.h, which is where an instrument is defined.
#define SAMPLE_RATE_HZ    44100
#define SINE_TABLE_SIZE   1024
// Peak amplitude of the sine table and of every composite wavetable built
// from it. Nearly full-scale: kVoices' levels are normalized to sum to
// VOLUME_Q15_ONE (see normalizeVoiceLevelsQ15), so the full-volume mix peaks
// here whatever the voice count — no headroom needs reserving per voice.
#define SINE_TABLE_AMPLITUDE 32000
#define AUDIO_BLOCK_FRAMES 256    // stereo frames generated + written per I2S block

#define NUM_HARMONICS 22  // fundamental (1x) + n overtones (2x..nx), at the
                          // bottom of the pitch range; the wavetable mip
                          // levels drop harmonics as pitch rises so the
                          // series never crosses Nyquist (see oscillator.h)

// Per-voice volume is carried into the mixer as a Q15 fixed-point fraction
// (0 .. VOLUME_Q15_ONE == 0.0 .. 1.0) so the per-sample mix stays integer-only
// — the ESP32-C3 core has no hardware FPU, so a per-sample float multiply is a
// software-emulated routine.
const int VOLUME_Q15_SHIFT = 15;
const int VOLUME_Q15_ONE   = (1 << VOLUME_Q15_SHIFT) - 1; // 32767

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
