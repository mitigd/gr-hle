// Video seams. The game's VGA overlay (vg_graf.ovl) loads into the game's own code segment at
// 0x9d50 and is almost all plain guest memory work on a linear A000h buffer: that stays the
// original code. Only the parts that call the BIOS are replaced, and they are re-armed by
// load_overlay each time the overlay is loaded (OverlaySeams below).
//
// Overlay map (offset from 0x9d50): +15h init_graphics (int 10h mode 13h), +37h text mode,
// +116h palette flash (int 10h/1012h with a wait on the tick counter).
#include "seams/seams.h"

#include <cstring>

namespace game::seams {

SeamList Video() {
    using hlekit::Machine;
    using hlekit::Reg;
    using hlekit::Seam;
    return {
        {0x5eed, "get_video_mode", "video",
         "FUN_00005eed: int 10h/0Fh current mode, AX = AL. Native: the host's current mode.",
         [](Machine& m) { m.vm.Set16(Reg::Eax, (uint16_t)(m.videoMode.load() & 0xff)); },
         0, 0, Seam::Ret::Near16},
        // The game only asks for mode 3 when it quits, to hand the text screen back to DOS. The host
        // has no DOS screen to hand back; the graphics overlay already owns the display.
        {0x5ed7, "set_video_mode", "video",
         "FUN_00005ed7: int 10h/00h set mode (the argument). Native: nothing to restore.",
         [](Machine&) {}, 0, 0, Seam::Ret::Near16},
    };
}

const SeamList& OverlaySeams() {
    using hlekit::Machine;
    using hlekit::Seam;
    static const SeamList seams = {
        {kVgOverlay + 0x15, "vg_init_graphics", "video",
         "vg_graf +15h: int 10h mode 13h, border colour, clear A000h; [11A9h] = A000h. Native: select "
         "the 320x200x256 host window and clear it.",
         [](Machine& m) {
             m.videoMode = 0x13;
             LeaveTextScreen();
             m.vm.Wr16(((uint32_t)DataSeg() << 4) + 0x11a9, 0xA000);
             if (uint8_t* p = m.vm.Ptr(0xA0000, 64000)) memset(p, 0, 64000);
             // What the BIOS mode set loads: the 16 default colours (6-bit DAC), which the
             // overlay's 4-bit pixels index directly.
             static const uint8_t kDac[16][3] = {
                 {0, 0, 0},    {0, 0, 42},    {0, 42, 0},    {0, 42, 42},   {42, 0, 0},    {42, 0, 42},
                 {42, 21, 0},  {42, 42, 42},  {21, 21, 21},  {21, 21, 63},  {21, 63, 21},  {21, 63, 63},
                 {63, 21, 21}, {63, 21, 63},  {63, 63, 21},  {63, 63, 63}};
             std::lock_guard<std::mutex> lock(m.paletteMutex);
             memcpy(m.palette, kDac, sizeof kDac);
         }, 0, 0, Seam::Ret::Near16},
        {kVgOverlay + 0x37, "vg_text_mode", "video",
         "vg_graf +37h: int 10h text mode 40x25 (clear, cursor home) for full-screen text such as the "
         "inventory. Native: the 8x8 text console takes over the 320x200 window.",
         [](Machine& m) { EnterTextScreen(m); }, 0, 0, Seam::Ret::Near16},
        {kVgOverlay + 0x116, "vg_palette_flash", "video",
         "vg_graf +116h: flashes the DAC with int 10h/1012h, waiting on the tick counter. Native: skipped.",
         [](Machine&) {}, 0, 0, Seam::Ret::Near16},
    };
    return seams;
}

}  // namespace game::seams
