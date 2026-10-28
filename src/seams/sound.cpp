// Sound seams: the game's PC-speaker voice (machine class 8) goes straight to the host. The
// sequencer (FUN_8430, run by the timer task) stays the game's own; only its three leaf functions
// that wrote PIT ch2 / port 61h / port C0h are native, and they feed a square-wave generator.
//
//   FUN_8507  set_tone(DH,DL tone bytes, BX voice*2)   period N = (DH & 0F) | (DL & 3F) << 4
//             the original divisor is 12 * N on the 1.193182 MHz PIT -> frequency 99.43 kHz / N
//   FUN_8576  set_volume(BX voice*2): envelope step on the voice, then attenuation to the chip
//   FUN_84d5  silence_all: all voices off, [1090h] = 0, then FUN_7922([10A2h]) (set flag)
#include "seams/seams.h"

#include <atomic>
#include <cmath>
#include <vector>

namespace game::seams {
namespace {

using hlekit::Machine;
using hlekit::Reg;
using hlekit::Seam;

constexpr int kRate = 22050;
std::atomic<float> g_freq{0};     // Hz; 0 = silent
std::atomic<float> g_level{0};
double g_phase = 0;
bool g_started = false;

void Fill(Machine& m) {
    if (!m.audio) return;
    constexpr int kChunk = 1024;
    std::vector<int16_t> pcm(kChunk);
    const double f = g_freq, a = g_level * 6000.0;
    for (auto& s : pcm) {
        s = f > 0 ? (int16_t)(g_phase < 0.5 ? a : -a) : 0;
        g_phase += f / kRate;
        if (g_phase >= 1) g_phase -= 1;
    }
    m.audio->Queue((const uint8_t*)pcm.data(), pcm.size() * 2);
}

void EnsureStarted(Machine& m) {
    if (g_started) return;
    g_started = m.StartSound(kRate, 1, 16, 2048, Fill);
}

// Guest-state accessors on the game's data segment.
uint32_t Ds() { return (uint32_t)DataSeg() << 4; }

}  // namespace

SeamList Sound() {
    return {
        {0x8507, "sound_set_tone", "sound",
         "FUN_00008507: tone bytes DH/DL for the voice in BX; PIT ch2 reload + speaker gate. Native: "
         "frequency 1193182 / (12 * N) to the host square wave; CL == 9Fh or volume 0Fh is silence.",
         [](Machine& m) {
             EnsureStarted(m);
             const uint8_t cl = (uint8_t)m.vm.Get(Reg::Ecx);
             const uint16_t bx = m.vm.Get16(Reg::Ebx);
             if (cl == 0x9f || m.vm.Rd8(Ds() + 0x15df + bx) == 0x0f) { g_freq = 0; return; }
             const uint16_t dx = m.vm.Get16(Reg::Edx);
             const uint32_t n = ((dx >> 8) & 0xf) | (uint32_t)(dx & 0x3f) << 4;
             g_freq = n ? 1193182.0f / (12.0f * n) : 0;
             g_level = 1;
         }, 0, 0, Seam::Ret::Near16},
        {0x8576, "sound_set_volume", "sound",
         "FUN_00008576: envelope step for voice BX, then the attenuation byte to the chip (port C0h). "
         "Native: the same envelope update in the game's arrays; attenuation 0Fh silences.",
         [](Machine& m) {
             const uint16_t bx = m.vm.Get16(Reg::Ebx);
             const uint32_t ds = Ds();
             uint8_t al = m.vm.Rd8(ds + 0x15df + bx);
             if (al != 0x0f) {
                 const uint16_t pos = m.vm.Rd16(ds + 0x15d7 + bx);
                 if (pos != 0xffff) {
                     const uint16_t tbl = m.vm.Rd16(ds + 0x15e7 + bx);
                     const uint8_t step = m.vm.Rd8(ds + tbl + pos);
                     if (step == 0x80) {
                         m.vm.Wr16(ds + 0x15d7 + bx, 0xffff);
                         al = m.vm.Rd8(ds + 0x15e0 + bx);
                         m.vm.Wr8(ds + 0x15df + bx, al);
                     } else {
                         m.vm.Wr16(ds + 0x15d7 + bx, pos + 1);
                         int v = (int8_t)(al + step);
                         if (v < 0) v = 0;
                         if (v > 15) v = 15;
                         m.vm.Wr8(ds + 0x15e0 + bx, (uint8_t)v);
                         v += m.vm.Rd8(ds + 0x20);
                         al = (uint8_t)(v > 15 ? 15 : v);
                     }
                 }
             }
             if (al == 0x0f) g_freq = 0;
             else g_level = (15 - al) / 15.0f;
         }, 0, 0, Seam::Ret::Near16},
        {0x84d5, "sound_silence", "sound",
         "FUN_000084d5: all voices off (port C0h / speaker gate), [1090h] = 0, set flag [10A2h]. Native: "
         "silence the host voice and set the flag bit (FUN_7945's byte/mask).",
         [](Machine& m) {
             g_freq = 0;
             const uint32_t ds = Ds();
             m.vm.Wr16(ds + 0x1090, 0);
             const uint8_t f = m.vm.Rd8(ds + 0x10a2);
             const uint32_t at = ds + 0x109 + (f >> 3);
             m.vm.Wr8(at, m.vm.Rd8(at) | (uint8_t)(0x80 >> (f & 7)));
         }, 0, 0, Seam::Ret::Near16},
    };
}

}  // namespace game::seams
