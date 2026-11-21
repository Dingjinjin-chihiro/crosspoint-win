#include "sim_action.h"

#include <Arduino.h>

#include "activities/ActivityManager.h"
#include "MappedInputManager.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

// Globals defined in Crosspoint/src/main.cpp (external linkage there).
extern ActivityManager activityManager;
extern MappedInputManager mappedInputManager;

namespace {
struct Action {
  unsigned long at;
  std::string op;
  std::string arg;
};

std::vector<Action> g_actions;
size_t g_next = 0;
bool g_armed = false;

std::string trim(const std::string& s) {
  const size_t a = s.find_first_not_of(" \t\r\n");
  if (a == std::string::npos) return {};
  const size_t b = s.find_last_not_of(" \t\r\n");
  return s.substr(a, b - a + 1);
}

void dispatch(const std::string& op, const std::string& arg) {
  std::printf("[ACTION] %s%s%s\n", op.c_str(), arg.empty() ? "" : " ", arg.c_str());
  std::fflush(stdout);

  // Navigation: these queue a pending transition, so they never fight an
  // in-flight frame and cannot be "eaten" by the input layer.
  if (op == "home") return activityManager.goHome();
  if (op == "library") return activityManager.goToLibrary();
  if (op == "browser") return activityManager.goToFileBrowser(arg);
  if (op == "settings") return activityManager.goToSettings();
  if (op == "plugins") return activityManager.goToPlugins();
  if (op == "usb") return activityManager.goToUsbDrive();
  if (op == "join") return activityManager.goToJoinNetwork();
  if (op == "open") return activityManager.goToReader(arg);
  if (op == "sleep") return activityManager.goToSleep(false);
  if (op == "boot") return activityManager.goToBoot();
  if (op == "pop") return activityManager.popActivity();
  if (op == "refresh") {
    activityManager.handleForcedRefresh();
    return;
  }
  if (op == "msg") return activityManager.goToFullScreenMessage(arg);

  // Reader actions call the reader directly (no button/touch path). These
  // return whether the action was accepted; logging it makes a rejected save
  // (e.g. mid-render) visible instead of silent.
  if (op == "turn" || op == "chapter") {
    const bool ok = activityManager.remoteTurn(arg != "-", op == "chapter");
    std::printf("[ACTION]   -> %s\n", ok ? "accepted" : "rejected (not in a rendered reader)");
    return;
  }
  if (op == "menu") {
    const bool ok = activityManager.remoteShortcut(true);
    std::printf("[ACTION]   -> %s\n", ok ? "accepted" : "rejected (reader not foreground)");
    return;
  }

  std::printf("[ACTION] unknown op '%s'\n", op.c_str());
}
}  // namespace

void sim_action_init(void) {
  const char* spec = std::getenv("CROSSPOINT_EMULATOR_ACTIONS");
  if (!spec || !*spec) return;

  // Start after boot so the first navigation lands on a live UI.
  unsigned long at = millis() + 800;
  const std::string s(spec);
  size_t pos = 0;
  while (pos <= s.size()) {
    const size_t sep = s.find(';', pos);
    const std::string item = trim(s.substr(pos, sep == std::string::npos ? std::string::npos : sep - pos));
    pos = (sep == std::string::npos) ? s.size() + 1 : sep + 1;
    if (item.empty()) continue;

    const size_t sp = item.find_first_of(" \t");
    const std::string op = trim(sp == std::string::npos ? item : item.substr(0, sp));
    const std::string arg = sp == std::string::npos ? std::string() : trim(item.substr(sp + 1));

    if (op == "wait") {
      at += static_cast<unsigned long>(std::strtoul(arg.c_str(), nullptr, 10));
      continue;
    }
    g_actions.push_back({at, op, arg});
    at += 300;  // small guard band between back-to-back actions
  }

  g_armed = !g_actions.empty();
  std::printf("[ACTION] %zu action(s) scheduled\n", g_actions.size());
  std::fflush(stdout);
}

void sim_action_pump(void) {
  if (!g_armed) return;
  const unsigned long now = millis();
  while (g_next < g_actions.size() && now >= g_actions[g_next].at) {
    dispatch(g_actions[g_next].op, g_actions[g_next].arg);
    ++g_next;
  }
  if (g_next >= g_actions.size()) g_armed = false;
}
