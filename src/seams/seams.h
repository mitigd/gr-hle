// Native seam modules for Gold Rush!.
//
// A seam module is just a function returning std::vector<Seam>. Keep the hardware boundary
// split by area, not in one growing GameSeams() function: video/, input/, sound/, timing/ can
// each grow independently, and later decompiled modules can live beside them.
#pragma once

#include "hlekit/machine.h"
#include "hlekit/seam.h"

#include <cstdint>
#include <vector>

namespace game::seams {

using SeamList = hlekit::SeamList;

// Linked address of the graphics overlay inside the game's code segment (jump table at +0).
constexpr uint32_t kVgOverlay = 0x9d50;
// The game's data segment (0A86h in the image) after the loader's relocation.
constexpr uint16_t DataSeg() { return 0xa86 + hlekit::Machine::kRealPsp + 0x10; }
// The 40x25 text screen the game switches to for its full-screen text (inventory): the 8x8 cells fill
// the same 320x200 window; init_graphics returns to the picture.
void EnterTextScreen(hlekit::Machine& m);
void LeaveTextScreen();
const SeamList& OverlaySeams();   // re-armed by load_overlay after the VGA overlay is read

SeamList Video();
SeamList Input();
SeamList Sound();
SeamList Timing();
SeamList File();
SeamList Text();
SeamList Memory();
SeamList Platform();
SeamList All();

// Input entry points: the native bodies of the game's keyboard/mouse/pad handlers. They run at
// an interruptible point, where the original IRQ could have arrived.
void OnKey(hlekit::Machine& m, uint8_t xtScan, bool down, bool extended);
void OnMouse(hlekit::Machine& m, int x, int y, uint8_t buttons);
void OnPad(hlekit::Machine& m, const hlekit::PadState& pad);

}  // namespace game::seams
