#pragma once

// Call before setup() so HalDisplay::begin() can use the window.
bool sim_display_init(void);
void sim_display_shutdown(void);
// Process SDL events (keyboard, etc.). Returns false if user requested quit.
bool sim_display_pump_events(void);
// Write the current frame to screenshots/ (same output as Ctrl+S). Used by the
// scripted touch runner so a capture lines up exactly with a gesture.
bool sim_display_screenshot(void);
