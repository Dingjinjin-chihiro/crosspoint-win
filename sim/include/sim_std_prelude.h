#pragma once

// Host-build prelude, force-included into every C++ translation unit.
//
// On the device the Arduino/ESP32 core pulls in a large set of standard headers
// through Arduino.h, so some firmware headers rely on transitive includes
// (std::function, uint8_t, std::round, ...). The emulator's sim headers are much
// smaller, so reproduce the useful part of that environment here instead of
// patching upstream firmware sources.

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// Arduino exposes min()/max() unqualified; some firmware code calls them that way.
using std::max;
using std::min;
