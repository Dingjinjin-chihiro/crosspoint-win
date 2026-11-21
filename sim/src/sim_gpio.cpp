#include <BoardConfig.h>
#include <EInkDisplay.h>
#include <HalGPIO.h>
#include <SDL.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Logging.h"
#include "sim_display.h"
#include "sim_input.h"

namespace {
uint8_t g_lastState = 0;
uint8_t g_prevState = 0;
bool g_anyPressed = false;
bool g_anyReleased = false;
unsigned long g_pressStartMs = 0;
unsigned long g_powerPressStartMs = 0;

uint8_t readButtonState() {
  const Uint8* keys = SDL_GetKeyboardState(nullptr);
  uint8_t state = 0;
  if (keys[SDL_SCANCODE_LEFT]) state |= (1 << HalGPIO::BTN_LEFT);
  if (keys[SDL_SCANCODE_RIGHT]) state |= (1 << HalGPIO::BTN_RIGHT);
  if (keys[SDL_SCANCODE_UP]) state |= (1 << HalGPIO::BTN_UP);
  if (keys[SDL_SCANCODE_DOWN]) state |= (1 << HalGPIO::BTN_DOWN);
  if (keys[SDL_SCANCODE_RETURN] || keys[SDL_SCANCODE_KP_ENTER]) state |= (1 << HalGPIO::BTN_CONFIRM);
  if (keys[SDL_SCANCODE_BACKSPACE] || keys[SDL_SCANCODE_ESCAPE]) state |= (1 << HalGPIO::BTN_BACK);
  if (keys[SDL_SCANCODE_P]) state |= (1 << HalGPIO::BTN_POWER);
  return state;
}

// --- Capacitive touch surrogate (SDL pointer) -------------------------------
//
// The firmware's touch classifiers were written against the FreeInk SDK's
// InputManager, so the emulator reproduces that state machine here: the same
// slop/timeout constants, the same one-frame events, and the same suppression
// latch. HalGPIO is the seam because CROSSPOINT_EMULATED builds drop
// InputManager entirely (HalGPIO.h).
//
// Coordinates: the panel buffer is 800x480 and the SDL window shows it rotated
// into 480x800 (render_bw_to_texture in sim_display.cpp). Mapping a window pixel
// back onto the panel is the inverse of that rotation, and normalizing against
// the panel extent is what GfxRenderer::tapToLogical() expects: it multiplies by
// panelWidth/panelHeight and then applies the live orientation.
constexpr int kPanelW = EInkDisplay::DISPLAY_WIDTH;   // 800, panel X
constexpr int kPanelH = EInkDisplay::DISPLAY_HEIGHT;  // 480, panel Y

// Mirrors InputManager's touch thresholds (InputManager.h) so the same finger
// motions classify the same way they would on hardware.
constexpr float kTapSlopPx = 28.0f;
constexpr float kTapReleaseSlopPx = 59.0f;
constexpr float kSwipeMinPx = 60.0f;
constexpr unsigned long kSwipeMaxMs = 700;
constexpr unsigned long kLongPressMs = 500;

struct TouchState {
  // Raw pointer, refreshed by sim_gpio_on_pointer() from the SDL event pump.
  float rawX = 0.0f;
  float rawY = 0.0f;
  bool downEdge = false;
  bool upEdge = false;
  unsigned long downEdgeAt = 0;
  unsigned long upEdgeAt = 0;
  float downEdgeX = 0.0f;
  float downEdgeY = 0.0f;
  float upEdgeX = 0.0f;
  float upEdgeY = 0.0f;

  // Contact state machine, one frame per HalGPIO::update().
  bool pressed = false;
  bool pressedEvent = false;
  bool releasedEvent = false;
  bool longPressEvent = false;
  bool longPressFired = false;
  bool suppressed = false;
  bool movedBeyondTapSlop = false;
  bool movedBeyondReleaseSlop = false;
  float downX = 0.0f;
  float downY = 0.0f;
  float curX = 0.0f;
  float curY = 0.0f;
  float upX = 0.0f;
  float upY = 0.0f;
  unsigned long downAt = 0;
  unsigned long lastHeldMs = 0;
};

TouchState g_touch;

float clampUnit(const float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

void normalizePanel(const float px, const float py, float& nx, float& ny) {
  nx = clampUnit(px / static_cast<float>(kPanelW));
  ny = clampUnit(py / static_cast<float>(kPanelH));
}

// Slop is measured from the touch-down point, in panel pixels.
void updateTouchSlop() {
  const float dx = g_touch.curX - g_touch.downX;
  const float dy = g_touch.curY - g_touch.downY;
  if (dx > kTapSlopPx || dx < -kTapSlopPx || dy > kTapSlopPx || dy < -kTapSlopPx) g_touch.movedBeyondTapSlop = true;
  if (dx > kTapReleaseSlopPx || dx < -kTapReleaseSlopPx || dy > kTapReleaseSlopPx || dy < -kTapReleaseSlopPx) {
    g_touch.movedBeyondReleaseSlop = true;
  }
}

void runInjectSteps(unsigned long now);

void touchUpdate() {
  const unsigned long now = millis();
  g_touch.pressedEvent = false;
  g_touch.releasedEvent = false;
  g_touch.longPressEvent = false;

  // The suppression latch holds through the release-edge frame and self-clears
  // on the first frame with no contact left (InputManager::serviceTouch).
  if (!g_touch.pressed) {
    g_touch.suppressed = false;
    g_touch.longPressFired = false;
  }

  // Scripted/hotkey gestures are polled here, after the latch and before the
  // edges are consumed, exactly where the touch controller would be read.
  runInjectSteps(now);

  if (g_touch.downEdge) {
    g_touch.downEdge = false;
    g_touch.pressed = true;
    g_touch.pressedEvent = true;
    g_touch.downX = g_touch.curX = g_touch.upX = g_touch.downEdgeX;
    g_touch.downY = g_touch.curY = g_touch.upY = g_touch.downEdgeY;
    g_touch.downAt = g_touch.downEdgeAt;
    g_touch.movedBeyondTapSlop = false;
    g_touch.movedBeyondReleaseSlop = false;
    LOG_DBG("TOUCH", "down (%d,%d)", static_cast<int>(g_touch.downX), static_cast<int>(g_touch.downY));
  } else if (g_touch.pressed) {
    // Live contact tracking for drag interactions.
    g_touch.curX = g_touch.upX = g_touch.rawX;
    g_touch.curY = g_touch.upY = g_touch.rawY;
    updateTouchSlop();
  }

  if (g_touch.upEdge) {
    g_touch.upEdge = false;
    if (g_touch.pressed) {
      g_touch.pressed = false;
      g_touch.releasedEvent = true;
      g_touch.curX = g_touch.upX = g_touch.upEdgeX;
      g_touch.curY = g_touch.upY = g_touch.upEdgeY;
      g_touch.lastHeldMs = g_touch.upEdgeAt - g_touch.downAt;
      // A flick can end before an intermediate motion sample was polled, so the
      // release point is tested here as well.
      updateTouchSlop();
      LOG_DBG("TOUCH", "release (%d,%d) held=%lums moved=%d/%d", static_cast<int>(g_touch.upX),
              static_cast<int>(g_touch.upY), g_touch.lastHeldMs, g_touch.movedBeyondTapSlop ? 1 : 0,
              g_touch.movedBeyondReleaseSlop ? 1 : 0);
    }
  }

  if (g_touch.pressed && !g_touch.movedBeyondTapSlop && !g_touch.longPressFired && !g_touch.suppressed &&
      now - g_touch.downAt >= kLongPressMs) {
    g_touch.longPressFired = true;
    g_touch.longPressEvent = true;
  }
}

// --- Scripted touch injection -----------------------------------------------
//
// The emulator can drive the contact state machine itself, with exact
// coordinates and exact timings. This is what makes a gesture reproducible:
// OS mouse input is the natural way to click, but its event timing depends on
// the host, so a tap can land as a drag or a slow swipe falls outside the swipe
// window. Injected gestures do not.
//
// Two drivers use it:
//   * gesture hotkeys (F1-F7), which fire at the current pointer position --
//     move the mouse to aim, press a key to deliver a standard gesture;
//   * CROSSPOINT_TOUCH_SCRIPT, which names coordinates outright, for repeatable
//     runs:  "wait 1500; tap 232,421; swipe 400,400 100,400 300"
//
// Coordinates are logical portrait pixels of what the window shows (x 0..479,
// y 0..799), the same frame GfxRenderer::tapToLogical() produces, so a value
// read off a screenshot can be used directly.
enum class Gesture : uint8_t { Tap, DoubleTap, LongPress, SwipeLeft, SwipeRight, SwipeUp, SwipeDown };

// 1 = down, 2 = move, 3 = up. Times are absolute millis(), so a slow frame
// cannot stretch a gesture: overdue steps are simply applied back to back.
struct InjectStep {
  unsigned long at;
  uint8_t op;
  float px;
  float py;
};

constexpr int kMaxInjectSteps = 192;
constexpr unsigned long kInjectTapMs = 80;
constexpr unsigned long kInjectLongPressMs = 700;
constexpr unsigned long kInjectDoubleGapMs = 150;
constexpr unsigned long kInjectSwipeMs = 260;
constexpr int kInjectSwipeSegments = 8;
constexpr float kHotkeySwipePx = 220.0f;  // logical px: well past the 60 px swipe gate
constexpr unsigned long kScriptSettleMs = 400;

InjectStep g_steps[kMaxInjectSteps];
int g_stepCount = 0;
int g_stepNext = 0;
bool g_scriptParsed = false;
bool g_stepsDropped = false;

void logicalToPanel(const float lx, const float ly, float& px, float& py) {
  // Inverse of GfxRenderer::tapToLogical() in Portrait: outX = 479 - panelY,
  // outY = panelX.
  const float cx = lx < 0.0f ? 0.0f : (lx > static_cast<float>(kPanelH - 1) ? static_cast<float>(kPanelH - 1) : lx);
  const float cy = ly < 0.0f ? 0.0f : (ly > static_cast<float>(kPanelW - 1) ? static_cast<float>(kPanelW - 1) : ly);
  px = cy;
  py = static_cast<float>(kPanelH - 1) - cx;
}

void panelToLogical(const float px, const float py, float& lx, float& ly) {
  lx = static_cast<float>(kPanelH - 1) - py;
  ly = px;
}

// The single sink for pointer contacts: real mouse events and injected steps
// both end up here, so both feed identical state.
void pointerEvent(const uint8_t op, const float px, const float py) {
  g_touch.rawX = px;
  g_touch.rawY = py;
  if (op == 1) {
    g_touch.downEdge = true;
    g_touch.downEdgeAt = millis();
    g_touch.downEdgeX = px;
    g_touch.downEdgeY = py;
  } else if (op == 3) {
    g_touch.upEdge = true;
    g_touch.upEdgeAt = millis();
    g_touch.upEdgeX = px;
    g_touch.upEdgeY = py;
  }
}

bool pushStep(const unsigned long at, const uint8_t op, const float px, const float py) {
  if (g_stepCount >= kMaxInjectSteps) {
    if (!g_stepsDropped) {
      g_stepsDropped = true;
      LOG_DBG("TOUCH", "inject queue full, dropping the rest");
    }
    return false;
  }
  g_steps[g_stepCount].at = at;
  g_steps[g_stepCount].op = op;
  g_steps[g_stepCount].px = px;
  g_steps[g_stepCount].py = py;
  ++g_stepCount;
  return true;
}

void pushTap(const unsigned long at, const float px, const float py) {
  if (!pushStep(at, 1, px, py)) return;
  pushStep(at + kInjectTapMs, 3, px, py);
}

void pushLongPress(const unsigned long at, const float px, const float py, const unsigned long holdMs) {
  if (!pushStep(at, 1, px, py)) return;
  pushStep(at + holdMs, 3, px, py);
}

void pushSwipe(const unsigned long at, const float x1, const float y1, const float x2, const float y2,
               const unsigned long durationMs) {
  if (!pushStep(at, 1, x1, y1)) return;
  const unsigned long dur = durationMs < 40 ? 40 : durationMs;
  for (int i = 1; i <= kInjectSwipeSegments; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(kInjectSwipeSegments);
    pushStep(at + static_cast<unsigned long>(dur * t), 2, x1 + (x2 - x1) * t, y1 + (y2 - y1) * t);
  }
  pushStep(at + dur, 3, x2, y2);
}

// The duration is optional and may be written with a comma or a space
// ("swipe 1,2 3,4,300" or "swipe 1,2 3,4 300"). %*d skips the four required
// coordinates so only the trailing value is read.
int parseSwipeDuration(const char* cmd) {
  int dur = 0;
  if (std::sscanf(cmd, " swipe %*d , %*d %*d , %*d , %d", &dur) == 1) return dur;
  if (std::sscanf(cmd, " swipe %*d , %*d %*d , %*d %d", &dur) == 1) return dur;
  return 0;
}

// Applies every step that is due, oldest first, then drops the applied prefix.
void runInjectSteps(const unsigned long now) {
  if (!g_scriptParsed) {
    g_scriptParsed = true;
    const char* script = std::getenv("CROSSPOINT_TOUCH_SCRIPT");
    if (script && *script) {
      // Parsed here rather than at static-init time so millis() is already
      // running and the script can be scheduled relative to the first frame.
      char buffer[2048];
      std::snprintf(buffer, sizeof(buffer), "%s", script);
      unsigned long at = now + kScriptSettleMs;
      for (char* raw = std::strtok(buffer, ";"); raw != nullptr; raw = std::strtok(nullptr, ";")) {
        char* cmd = raw;
        while (*cmd == ' ') ++cmd;
        int x1 = 0;
        int y1 = 0;
        int x2 = 0;
        int y2 = 0;
        int arg = 0;
        float px = 0.0f;
        float py = 0.0f;
        if (std::strncmp(cmd, "wait", 4) == 0 && std::sscanf(cmd, "wait %d", &arg) == 1) {
          at += static_cast<unsigned long>(arg < 0 ? 0 : arg);
        } else if (std::strncmp(cmd, "shot", 4) == 0) {
          pushStep(at, 4, 0.0f, 0.0f);
          LOG_DBG("TOUCH", "script shot");
        } else if (std::sscanf(cmd, " tap %d , %d", &x1, &y1) == 2) {
          logicalToPanel(static_cast<float>(x1), static_cast<float>(y1), px, py);
          pushTap(at, px, py);
          LOG_DBG("TOUCH", "script tap (%d,%d)", x1, y1);
        } else if (std::sscanf(cmd, " double %d , %d", &x1, &y1) == 2) {
          logicalToPanel(static_cast<float>(x1), static_cast<float>(y1), px, py);
          pushTap(at, px, py);
          pushTap(at + kInjectTapMs + kInjectDoubleGapMs, px, py);
          LOG_DBG("TOUCH", "script double-tap (%d,%d)", x1, y1);
        } else if (std::sscanf(cmd, " long %d , %d , %d", &x1, &y1, &arg) == 3) {
          logicalToPanel(static_cast<float>(x1), static_cast<float>(y1), px, py);
          pushLongPress(at, px, py, static_cast<unsigned long>(arg < 1 ? kInjectLongPressMs : arg));
          LOG_DBG("TOUCH", "script long-press (%d,%d) %dms", x1, y1, arg < 1 ? 0 : arg);
        } else if (std::sscanf(cmd, " swipe %d , %d %d , %d", &x1, &y1, &x2, &y2) == 4) {
          float sx = 0.0f;
          float sy = 0.0f;
          float ex = 0.0f;
          float ey = 0.0f;
          logicalToPanel(static_cast<float>(x1), static_cast<float>(y1), sx, sy);
          logicalToPanel(static_cast<float>(x2), static_cast<float>(y2), ex, ey);
          const int dur = parseSwipeDuration(cmd);
          const unsigned long swipeMs = dur > 0 ? static_cast<unsigned long>(dur) : kInjectSwipeMs;
          pushSwipe(at, sx, sy, ex, ey, swipeMs);
          LOG_DBG("TOUCH", "script swipe (%d,%d)->(%d,%d) %lums", x1, y1, x2, y2, swipeMs);
        } else {
          LOG_DBG("TOUCH", "script: ignoring '%s'", cmd);
          continue;
        }
        at += 60;  // small guard band between scripted gestures
      }
    }
  }

  while (g_stepNext < g_stepCount && static_cast<long>(now - g_steps[g_stepNext].at) >= 0) {
    if (g_steps[g_stepNext].op == 4) {
      sim_display_screenshot();
    } else {
      pointerEvent(g_steps[g_stepNext].op, g_steps[g_stepNext].px, g_steps[g_stepNext].py);
    }
    ++g_stepNext;
  }
  if (g_stepNext >= g_stepCount && g_stepCount > 0) {
    g_stepCount = 0;
    g_stepNext = 0;
  }
}

// Fire one standard gesture at the current pointer position. F1..F7 map onto
// these; see sim_input.h.
void injectGestureAtPointer(const Gesture gesture) {
  float lx = 0.0f;
  float ly = 0.0f;
  panelToLogical(g_touch.rawX, g_touch.rawY, lx, ly);
  const unsigned long now = millis();
  switch (gesture) {
    case Gesture::Tap:
      pushTap(now, g_touch.rawX, g_touch.rawY);
      break;
    case Gesture::DoubleTap:
      pushTap(now, g_touch.rawX, g_touch.rawY);
      pushTap(now + kInjectTapMs + kInjectDoubleGapMs, g_touch.rawX, g_touch.rawY);
      break;
    case Gesture::LongPress:
      pushLongPress(now, g_touch.rawX, g_touch.rawY, kInjectLongPressMs);
      break;
    case Gesture::SwipeLeft:
    case Gesture::SwipeRight:
    case Gesture::SwipeUp:
    case Gesture::SwipeDown: {
      float dx = 0.0f;
      float dy = 0.0f;
      if (gesture == Gesture::SwipeLeft) dx = -kHotkeySwipePx;
      if (gesture == Gesture::SwipeRight) dx = kHotkeySwipePx;
      if (gesture == Gesture::SwipeUp) dy = -kHotkeySwipePx;
      if (gesture == Gesture::SwipeDown) dy = kHotkeySwipePx;
      // Keep both endpoints on the panel so a swipe aimed near an edge still
      // has full travel instead of being clipped short.
      float sx = lx;
      float sy = ly;
      if (sx + dx < 0.0f) sx -= dx;
      if (sx + dx > static_cast<float>(kPanelH - 1)) sx -= dx;
      if (sy + dy < 0.0f) sy -= dy;
      if (sy + dy > static_cast<float>(kPanelW - 1)) sy -= dy;
      float startPx = 0.0f;
      float startPy = 0.0f;
      float endPx = 0.0f;
      float endPy = 0.0f;
      logicalToPanel(sx, sy, startPx, startPy);
      logicalToPanel(sx + dx, sy + dy, endPx, endPy);
      pushSwipe(now, startPx, startPy, endPx, endPy, kInjectSwipeMs);
      break;
    }
  }
  LOG_DBG("TOUCH", "inject gesture #%d at (%d,%d)", static_cast<int>(gesture), static_cast<int>(lx),
          static_cast<int>(ly));
}
}  // namespace

HalGPIO gpio;

void HalGPIO::begin() {}

void HalGPIO::prepareForDeepSleep() {}

void HalGPIO::update() {
  g_prevState = g_lastState;
  g_lastState = readButtonState();

  g_anyPressed = false;
  g_anyReleased = false;
  for (int i = 0; i <= BTN_POWER; ++i) {
    const uint8_t bit = static_cast<uint8_t>(1u << i);
    if ((g_lastState & bit) && !(g_prevState & bit)) g_anyPressed = true;
    if (!(g_lastState & bit) && (g_prevState & bit)) g_anyReleased = true;
  }

  // Like InputManager, the held time runs from the latest press while any button stays down.
  if (g_lastState == 0) {
    g_pressStartMs = 0;
  } else if (g_anyPressed || g_pressStartMs == 0) {
    g_pressStartMs = millis();
  }

  touchUpdate();

  const bool powerHeld = (g_lastState & (1 << BTN_POWER)) != 0;
  if (powerHeld) {
    if (g_powerPressStartMs == 0) g_powerPressStartMs = millis();
  } else {
    g_powerPressStartMs = 0;
  }

  const bool usbNow = isUsbConnected();
  usbStateChanged = (usbNow != lastUsbConnected);
  lastUsbConnected = usbNow;
}

bool HalGPIO::isPressed(uint8_t buttonIndex) const {
  if (buttonIndex > BTN_POWER) return false;
  return (g_lastState & (1 << buttonIndex)) != 0;
}

bool HalGPIO::wasPressed(uint8_t buttonIndex) const {
  if (buttonIndex > BTN_POWER) return false;
  return (g_lastState & (1 << buttonIndex)) != 0 && (g_prevState & (1 << buttonIndex)) == 0;
}

bool HalGPIO::wasAnyPressed() const { return g_anyPressed; }

bool HalGPIO::wasReleased(uint8_t buttonIndex) const {
  if (buttonIndex > BTN_POWER) return false;
  return (g_lastState & (1 << buttonIndex)) == 0 && (g_prevState & (1 << buttonIndex)) != 0;
}

bool HalGPIO::wasAnyReleased() const { return g_anyReleased; }

unsigned long HalGPIO::getHeldTime() const {
  if (g_pressStartMs == 0) return 0;
  return millis() - g_pressStartMs;
}

unsigned long HalGPIO::getPowerButtonHeldTime() const {
  if (g_powerPressStartMs == 0) return 0;
  return millis() - g_powerPressStartMs;
}

bool HalGPIO::rawInputActive() { return readButtonState() != 0; }

// Same board queries as the firmware's HalGPIO.cpp, answered by the emulated X4 profile.
bool HalGPIO::hasEdgeSideButtons() const {
  return BoardConfig::ACTIVE.board == BoardConfig::Board::XteinkX3 ||
         BoardConfig::ACTIVE.board == BoardConfig::Board::XteinkX3Uc8279 ||
         BoardConfig::ACTIVE.board == BoardConfig::Board::XteinkX4Pro ||
         BoardConfig::ACTIVE.board == BoardConfig::Board::XteinkX4Classic;
}

bool HalGPIO::isXteinkDevice() const {
  return BoardConfig::ACTIVE.board == BoardConfig::Board::XteinkX3 ||
         BoardConfig::ACTIVE.board == BoardConfig::Board::XteinkX3Uc8279 ||
         BoardConfig::ACTIVE.board == BoardConfig::Board::XteinkX4;
}

// The emulator presents the panel as a touch surface fed by the SDL pointer, so
// the firmware's touch-only UI paths (list taps, reader swipes, edge gestures)
// are reachable. Home key and slide switch stay absent, matching the X4 body.
bool HalGPIO::hasTouch() const { return true; }
bool HalGPIO::hasHomeKey() const { return false; }
bool HalGPIO::hasToggleSwitch() const { return false; }
bool HalGPIO::isToggleSwitchOn() const { return false; }
bool HalGPIO::wasHomeKeyPressed() const { return false; }
bool HalGPIO::wasHomeKeyTapped() const { return false; }
bool HalGPIO::wasHomeKeyLongPressed() const { return false; }

// Tap routes to the touch-down point, not the lift point: the reported position
// drifts as a finger rolls off during release (InputManager::wasTouchTap).
bool HalGPIO::wasTouchTap(float& nx, float& ny) const {
  if (!g_touch.releasedEvent || g_touch.suppressed || g_touch.movedBeyondReleaseSlop) return false;
  normalizePanel(g_touch.downX, g_touch.downY, nx, ny);
  return true;
}

// Press edge, used to highlight what is under the finger before release.
bool HalGPIO::wasTouchDown(float& nx, float& ny) const {
  if (!g_touch.pressedEvent) return false;
  normalizePanel(g_touch.downX, g_touch.downY, nx, ny);
  return true;
}

bool HalGPIO::wasTouchReleased() const { return g_touch.releasedEvent && !g_touch.suppressed; }

bool HalGPIO::isTouchTapCandidate(float& nx, float& ny, unsigned long& heldMs) const {
  if (!g_touch.pressed || g_touch.movedBeyondTapSlop || g_touch.suppressed) return false;
  normalizePanel(g_touch.downX, g_touch.downY, nx, ny);
  heldMs = millis() - g_touch.downAt;
  return true;
}

// Live contact position while the finger is down (no tap-slop gate): drag tracking.
bool HalGPIO::isTouchHeldAt(float& nx, float& ny) const {
  if (!g_touch.pressed || g_touch.suppressed) return false;
  normalizePanel(g_touch.curX, g_touch.curY, nx, ny);
  return true;
}

bool HalGPIO::wasTouchLongPress(float& nx, float& ny) const {
  if (!g_touch.longPressEvent) return false;
  normalizePanel(g_touch.downX, g_touch.downY, nx, ny);
  return true;
}

void HalGPIO::suppressTouchContact() {
  // Only meaningful mid-contact (or on its release-edge frame); the latch
  // self-clears in touchUpdate() once the contact is over.
  if (g_touch.pressed || g_touch.releasedEvent) g_touch.suppressed = true;
}

unsigned long HalGPIO::lastTouchHeldMs() const { return g_touch.lastHeldMs; }

// A flick: travelled past the swipe distance within the time window.
bool HalGPIO::wasSwipe(float& nxStart, float& nyStart, float& nxEnd, float& nyEnd) const {
  if (!g_touch.releasedEvent || g_touch.suppressed) return false;
  if (g_touch.lastHeldMs > kSwipeMaxMs) return false;
  const float dx = g_touch.upX - g_touch.downX;
  const float dy = g_touch.upY - g_touch.downY;
  const float adx = dx < 0.0f ? -dx : dx;
  const float ady = dy < 0.0f ? -dy : dy;
  if (adx < kSwipeMinPx && ady < kSwipeMinPx) return false;
  normalizePanel(g_touch.downX, g_touch.downY, nxStart, nyStart);
  normalizePanel(g_touch.upX, g_touch.upY, nxEnd, nyEnd);
  return true;
}

bool HalGPIO::wasTouchActivity() const { return g_touch.pressedEvent || g_touch.releasedEvent; }
void HalGPIO::setSharedConfirmPowerShortPressEmitsPower(bool) {}

bool HalGPIO::verifyPowerButtonWakeup() { return true; }

bool HalGPIO::isUsbConnected() const { return true; }

bool HalGPIO::coldBootImpliesPowerButton() const { return isXteinkDevice(); }

bool HalGPIO::wasUsbStateChanged() const { return usbStateChanged; }

HalGPIO::WakeupReason HalGPIO::getWakeupReason() const { return WakeupReason::PowerButton; }

// Fed by sim_display_pump_events (see sim_input.h). Maps a window pixel back to
// panel pixels -- the inverse of the display rotation -- and latches the edges
// the touch state machine consumes on the next HalGPIO::update().
void sim_gpio_on_pointer(const int kind, int x, int y, const int clientW, const int clientH) {
  if (clientW <= 0 || clientH <= 0) return;
  const int cx = x < 0 ? 0 : (x >= clientW ? clientW - 1 : x);
  const int cy = y < 0 ? 0 : (y >= clientH ? clientH - 1 : y);
  const float colT = clientW > 1 ? static_cast<float>(cx) / static_cast<float>(clientW - 1) : 0.0f;
  const float rowT = clientH > 1 ? static_cast<float>(cy) / static_cast<float>(clientH - 1) : 0.0f;

  // render_bw_to_texture(): window row runs along panel X, window column runs
  // back along panel Y.
  const float panelX = static_cast<float>(kPanelW - 1) * rowT;
  const float panelY = static_cast<float>(kPanelH - 1) * (1.0f - colT);

  const uint8_t op = kind == SIM_POINTER_DOWN ? 1 : (kind == SIM_POINTER_UP ? 3 : 2);
  pointerEvent(op, panelX, panelY);
}

// Gesture hotkeys: a standard gesture delivered at wherever the pointer is, with
// the emulator owning the timing (see the injection block above).
void sim_gpio_on_gesture_key(const int action) {
  const Gesture gesture = static_cast<Gesture>(action);
  if (gesture > Gesture::SwipeDown) return;
  injectGestureAtPointer(gesture);
}
