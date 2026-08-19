#pragma once

// Initializes the SSD1306 OLED over I2C. Call once from setup().
// Returns false if the display was not found at OLED_I2C_ADDR.
bool displayBegin();

// Redraws the 3-oscillator status screen from the current g_oscParams.
// Call periodically from loop() on Core 1 (see DISPLAY_REFRESH_MS).
void displayUpdate();
