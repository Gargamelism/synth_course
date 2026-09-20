#pragma once

// Build with -DDISABLE_DISPLAY (or uncomment below) for the final product,
// where the OLED isn't visible in the enclosure: this compiles the screen
// out entirely (Adafruit_GFX/SSD1306 included) rather than just skipping
// draws at runtime.
// #define DISABLE_DISPLAY

#ifdef DISABLE_DISPLAY

inline bool displayBegin() { return true; }
inline void displayUpdate() {}

#else

// Initializes the SSD1306 OLED over I2C. Call once from setup().
// Returns false if the display was not found at OLED_I2C_ADDR.
bool displayBegin();

// Redraws the status screen (root note + master volume, voice count and
// pitch mode, build switches) from the current g_oscParams.
// Call periodically from loop() (see DISPLAY_REFRESH_MS).
void displayUpdate();

#endif
