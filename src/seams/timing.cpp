// Timing seams: the game reprograms the PIT to 60 Hz and hooks the timer vectors (0x87b3). Here the
// install function is replaced by a 60 Hz host timer whose task is the native body of the timer ISR:
// the game's own sequencer step and, every third tick, its clock tick (the int 1Ch handler).
//
// Decomp of the ISR at 0x8927 (DS = data segment):
//   if ([1090h]) FUN_8430();                 ; sound sequencer step
//   if (--byte[1690h] == 0) { byte[1690h] = 3; chain to BIOS int 8 -> int 1Ch -> FUN_8310() }
#include "seams/seams.h"
#include "support/debug.h"

namespace game::seams {

SeamList Timing() {
    using hlekit::Machine;
    using hlekit::Seam;
    return {
        {0x87b3, "install_timer", "timing",
         "FUN_000087b3: saves/sets vectors 05,08,09,1C,1F,23,24 and programs PIT ch0 to 60 Hz. "
         "Native: 60 Hz host timer running the ISR body.",
         [](Machine& m) {
             const uint16_t ds = DataSeg();
             m.vm.Wr8(((uint32_t)ds << 4) + 0x1690, 3);
             m.SetTimer(60.0, [ds](Machine& mm) {
                 const uint32_t base = (uint32_t)ds << 4;
                 ++mm.frames;
                 support::DebugWatch(mm);
                 if (mm.vm.Rd16(base + 0x1090)) mm.CallGuestNear16(0x8430, {}, ds);
                 uint8_t n = mm.vm.Rd8(base + 0x1690) - 1;
                 if (n == 0) {
                     n = 3;
                     mm.vm.Wr8(base + 0x1690, n);
                     mm.CallGuestNear16(0x8310, {}, ds);
                 } else {
                     mm.vm.Wr8(base + 0x1690, n);
                 }
             });
         }, 0, 0, Seam::Ret::Near16},
        {0x763e, "random", "timing",
         "FUN_0000763e: linear-congruential random byte; first call seeds [1548h] from the BIOS tick count "
         "(int 1Ah). Native: seeded from the host clock.",
         [](Machine& m) {
             const uint32_t ds = (uint32_t)DataSeg() << 4;
             uint16_t seed = m.vm.Rd16(ds + 0x1548);
             if (!seed) seed = (uint16_t)(m.Seconds() * 1000) | 1;
             seed = (uint16_t)(seed * 0x7c4d + 1);
             m.vm.Wr16(ds + 0x1548, seed);
             m.vm.Set16(hlekit::Reg::Eax, (uint8_t)((seed & 0xff) ^ (seed >> 8)));
         }, 0, 0, Seam::Ret::Near16},
        {0x88a5, "restore_timer", "timing",
         "FUN_000088a5: restores the saved vectors. Native: stops the host timer.",
         [](Machine& m) { m.SetTimer(0, nullptr); }, 0, 0, Seam::Ret::Near16},
    };
}

}  // namespace game::seams
