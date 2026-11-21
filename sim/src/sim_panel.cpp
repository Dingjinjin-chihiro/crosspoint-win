#include "sim_panel.h"

#include "sim_input.h"

#include <SDL.h>

#include <cstdint>

namespace {
constexpr int kMargin = 6;
constexpr int kGap = 4;
constexpr int kButtonW = 52;
constexpr int kButtonH = 44;
constexpr int kPanelW = 2 * kMargin + 7 * kButtonW + 6 * kGap;  // 400
// Window height includes the title bar on Windows, so leave headroom so the
// client area is at least 2*kMargin + kButtonH (56) tall.
constexpr int kPanelH = 2 * kMargin + kButtonH + 40;           // 96

SDL_Window* g_win = nullptr;
SDL_Renderer* g_renderer = nullptr;
uint32_t g_windowId = 0;

// 5x7 glyphs, one byte per row, bit 4 = leftmost column. Only the characters
// the labels use, so no font dependency.
struct Glyph {
  char ch;
  uint8_t rows[7];
};
const Glyph kGlyphs[] = {
    {'A', {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
    {'B', {0x1F, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1F}},
    {'D', {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}},
    {'H', {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
    {'L', {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}},
    {'O', {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    {'P', {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}},
    {'T', {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}},
    {'<', {0x04, 0x08, 0x1F, 0x08, 0x04, 0x00, 0x00}},
    {'>', {0x04, 0x02, 0x1F, 0x02, 0x04, 0x00, 0x00}},
    {'^', {0x04, 0x0E, 0x15, 0x04, 0x04, 0x04, 0x00}},
    {'v', {0x04, 0x04, 0x04, 0x15, 0x0E, 0x04, 0x00}},
};

const uint8_t* glyphRows(char ch) {
  for (const Glyph& g : kGlyphs) {
    if (g.ch == ch) return g.rows;
  }
  return nullptr;
}

struct Button {
  const char* label;
  SimGesture gesture;
  SDL_Rect rect;
};

Button g_buttons[7];

void drawText(SDL_Renderer* r, int x, int y, const char* s, int scale) {
  for (const char* p = s; *p; ++p) {
    const uint8_t* rows = glyphRows(*p);
    if (!rows) {
      x += 6 * scale;
      continue;
    }
    for (int row = 0; row < 7; ++row) {
      for (int col = 0; col < 5; ++col) {
        if (rows[row] & (0x10 >> col)) {
          SDL_Rect px = {x + col * scale, y + row * scale, scale, scale};
          SDL_RenderFillRect(r, &px);
        }
      }
    }
    x += 6 * scale;
  }
}

int textWidth(const char* s, int scale) {
  int n = 0;
  for (const char* p = s; *p; ++p) ++n;
  return n * 6 * scale - 1 * scale;
}

int buttonAt(int x, int y) {
  for (int i = 0; i < 7; ++i) {
    const SDL_Rect& r = g_buttons[i].rect;
    if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h) return i;
  }
  return -1;
}
}  // namespace

bool sim_panel_init(void) {
  if (g_win) return true;
  // Fixed top-left corner: the main window is centred, so centring the panel too
  // would stack it on top of the panel's own content.
  g_win = SDL_CreateWindow("Gesture Panel - tap/dbl/hold/swipe (stable timing)", 40, 40, kPanelW, kPanelH,
                           SDL_WINDOW_SHOWN);
  if (!g_win) return false;
  g_renderer = SDL_CreateRenderer(g_win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!g_renderer) return false;
  g_windowId = SDL_GetWindowID(g_win);

  const char* labels[7] = {"TAP", "DBL", "HOLD", "<", ">", "^", "v"};
  const SimGesture gestures[7] = {SIM_GESTURE_TAP,          SIM_GESTURE_DOUBLE_TAP, SIM_GESTURE_LONG_PRESS,
                                  SIM_GESTURE_SWIPE_LEFT,   SIM_GESTURE_SWIPE_RIGHT, SIM_GESTURE_SWIPE_UP,
                                  SIM_GESTURE_SWIPE_DOWN};
  for (int i = 0; i < 7; ++i) {
    g_buttons[i].label = labels[i];
    g_buttons[i].gesture = gestures[i];
    g_buttons[i].rect = {kMargin + i * (kButtonW + kGap), kMargin, kButtonW, kButtonH};
  }

  printf("[PANEL] init ok, window id=%u renderer=%s\n", g_windowId, g_renderer ? "yes" : "no");
  sim_panel_render();
  return true;
}

void sim_panel_shutdown(void) {
  if (g_renderer) {
    SDL_DestroyRenderer(g_renderer);
    g_renderer = nullptr;
  }
  if (g_win) {
    SDL_DestroyWindow(g_win);
    g_win = nullptr;
  }
  g_windowId = 0;
}

void sim_panel_render(void) {
  if (!g_renderer) return;
  SDL_SetRenderDrawColor(g_renderer, 60, 60, 64, 255);
  SDL_RenderClear(g_renderer);
  for (int i = 0; i < 7; ++i) {
    const SDL_Rect& r = g_buttons[i].rect;
    SDL_SetRenderDrawColor(g_renderer, 255, 255, 255, 255);
    SDL_RenderFillRect(g_renderer, &r);
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
    SDL_RenderDrawRect(g_renderer, &r);
    const int scale = 2;
    const int tw = textWidth(g_buttons[i].label, scale);
    drawText(g_renderer, r.x + (r.w - tw) / 2, r.y + (r.h - 7 * scale) / 2, g_buttons[i].label, scale);
  }
  SDL_RenderPresent(g_renderer);
}

void sim_panel_handle_event(const SDL_Event* e) {
  if (!e || !g_win) return;
  uint32_t wid = 0;
  int mx = 0;
  int my = 0;
  if (e->type == SDL_MOUSEMOTION) {
    wid = e->motion.windowID;
    mx = e->motion.x;
    my = e->motion.y;
  } else if (e->type == SDL_MOUSEBUTTONDOWN || e->type == SDL_MOUSEBUTTONUP) {
    wid = e->button.windowID;
    mx = e->button.x;
    my = e->button.y;
  } else if (e->type == SDL_WINDOWEVENT) {
    wid = e->window.windowID;
    if (e->window.event == SDL_WINDOWEVENT_EXPOSED) sim_panel_render();
    return;
  } else {
    return;
  }
  if (wid != g_windowId) return;

  if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
    const int idx = buttonAt(mx, my);
    if (idx >= 0) {
      // Fire on press: the gesture fires at the main panel's current pointer
      // position with the emulator's own timing (see sim_gpio.cpp). A button-up
      // is not used because SDL captures the mouse on press and the OS then
      // reports the release at the real cursor position, outside this window.
      printf("[PANEL] button %d '%s' fired\n", idx, g_buttons[idx].label);
      fflush(stdout);
      sim_gpio_on_gesture_key(g_buttons[idx].gesture);
    }
  }
}
