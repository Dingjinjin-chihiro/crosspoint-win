#pragma once

// A small "gesture panel" SDL window with buttons for tap / double-tap /
// long-press / four swipe directions. Clicking a button fires that gesture at
// the main panel's current pointer position, with timing the emulator owns
// (the injection engine in sim_gpio.cpp), so a tap is always 80 ms and a swipe
// always its fixed duration -- the host mouse cannot perturb the gesture.

union SDL_Event;

bool sim_panel_init(void);
void sim_panel_shutdown(void);
void sim_panel_handle_event(const SDL_Event* e);
void sim_panel_render(void);
