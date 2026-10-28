// Input seams: the game's keyboard reads. The original keyboard ISR (0x64b8) and its BIOS
// buffer are not run; host key events become the words the game's key readers return.
//
//   FUN_5f02  get_key: AX = 0 (none), ASCII, or scan << 8 for keys without ASCII
//   FUN_6840  poll_joystick: AX = 0xFFFF if a key is waiting; else 0 with the axes [13F8h],[13FAh]
#include "seams/seams.h"

#include <deque>

namespace game::seams {
namespace {

std::deque<uint16_t> g_keys;
bool g_shift = false;
bool g_alt = false;

constexpr char kPlain[] = "\0\x1b" "1234567890-=\b\tqwertyuiop[]\r\0asdfghjkl;'`\0\\zxcvbnm,./\0*\0 ";
constexpr char kShift[] = "\0\x1b" "!@#$%^&*()_+\b\tQWERTYUIOP{}\r\0ASDFGHJKL:\"~\0|ZXCVBNM<>?\0*\0 ";

}  // namespace

SeamList Input() {
    using hlekit::Machine;
    using hlekit::Reg;
    using hlekit::Seam;
    return {
        {0x5f02, "get_key", "input",
         "FUN_00005f02: int 16h/00h,01h,02h key read. Native: the host key queue.",
         [](Machine& m) {
             uint16_t ax = 0;
             if (!g_keys.empty()) { ax = g_keys.front(); g_keys.pop_front(); }
             m.vm.Set16(Reg::Eax, ax);
         }, 0, 0, Seam::Ret::Near16},
        {0x6840, "poll_joystick", "input",
         "FUN_00006840: int 16h/01h then a read of the game port 201h. Native: no joystick; centred axes.",
         [](Machine& m) {
             const uint32_t base = (uint32_t)DataSeg() << 4;
             m.vm.Wr16(base + 0x13f8, 0);
             m.vm.Wr16(base + 0x13fa, 0);
             m.vm.Set16(Reg::Eax, g_keys.empty() ? 0 : 0xFFFF);
         }, 0, 0, Seam::Ret::Near16},
    };
}

void OnKey(hlekit::Machine& m, uint8_t xtScan, bool down, bool extended) {
    (void)m;
    (void)extended;
    if (xtScan == 0x2a || xtScan == 0x36) { g_shift = down; return; }
    if (xtScan == 0x38) { g_alt = down; return; }
    if (down && g_alt && xtScan < 0x80) { g_keys.push_back((uint16_t)xtScan << 8); return; }  // Alt-Z = 2C00h
    if (!down || xtScan >= sizeof kPlain - 1) {
        if (down && xtScan < 0x80) g_keys.push_back((uint16_t)xtScan << 8);
        return;
    }
    const char c = g_shift ? kShift[xtScan] : kPlain[xtScan];
    g_keys.push_back(c ? (uint16_t)(uint8_t)c : (uint16_t)xtScan << 8);
}

void OnMouse(hlekit::Machine& m, int x, int y, uint8_t buttons) { (void)m; (void)x; (void)y; (void)buttons; }
void OnPad(hlekit::Machine& m, const hlekit::PadState& pad) { (void)m; (void)pad; }

}  // namespace game::seams
