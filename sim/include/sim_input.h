#pragma once

// Pointer input for the emulated capacitive touch panel.
//
// The emulator runs exactly one SDL event loop (sim_display_pump_events, called
// from main_sim's loop). It forwards mouse events here so touch input shares
// that loop: a second SDL_PollEvent() would find the queue already drained.
//
// Coordinates are SDL client pixels plus the client size they were measured
// against, so the mapping back to panel pixels survives any window scaling.
enum SimPointerKind {
  SIM_POINTER_MOVE = 1,
  SIM_POINTER_DOWN = 2,
  SIM_POINTER_UP = 3,
};

// Standard gestures the emulator can deliver on its own, at the current pointer
// position, with timing the host cannot perturb. Values match the Gesture enum
// in sim_gpio.cpp and are bound to F1..F7 there.
enum SimGesture {
  SIM_GESTURE_TAP = 0,
  SIM_GESTURE_DOUBLE_TAP = 1,
  SIM_GESTURE_LONG_PRESS = 2,
  SIM_GESTURE_SWIPE_LEFT = 3,
  SIM_GESTURE_SWIPE_RIGHT = 4,
  SIM_GESTURE_SWIPE_UP = 5,
  SIM_GESTURE_SWIPE_DOWN = 6,
};

void sim_gpio_on_pointer(int kind, int x, int y, int clientW, int clientH);
void sim_gpio_on_gesture_key(int action);
