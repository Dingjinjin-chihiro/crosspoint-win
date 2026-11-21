# ArduinoJson (vendored)

`src/ArduinoJson.h` is the single-header distribution of **ArduinoJson 7.4.2**, the
version pinned by the Crosspoint firmware's `platformio.ini` (`bblanchon/ArduinoJson @ 7.4.2`).

- Upstream: <https://github.com/bblanchon/ArduinoJson>
- Download: <https://github.com/bblanchon/ArduinoJson/releases/download/v7.4.2/ArduinoJson-v7.4.2.h>
- Copyright © 2014-2025, Benoit BLANCHON
- License: MIT (stated in the header of the file)

The firmware's `CMakeLists.txt` normally picks ArduinoJson up from the PlatformIO
library directory (`.pio/libdeps/default/ArduinoJson`), which only exists after
running PlatformIO. Vendoring it here keeps `git clone` + `build.cmd` working
without PlatformIO.
