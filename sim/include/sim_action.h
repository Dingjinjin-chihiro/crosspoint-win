#pragma once

// Direct semantic actions: call the firmware's own navigation / reader APIs by
// name instead of synthesising touch coordinates and letting the gesture
// classifier guess. The intent is DECLARED, not recognised.
//
//   CROSSPOINT_EMULATOR_ACTIONS="wait 1200; library; wait 1500; open /The Quiet Shelf.epub; wait 2000; turn"
//
// Ops: home / library / browser [path] / settings / plugins / usb / join /
//      open <path> / sleep / boot / pop / refresh / turn[ -] / chapter[ -] /
//      menu / msg <text> / wait <ms>
void sim_action_init(void);
void sim_action_pump(void);
