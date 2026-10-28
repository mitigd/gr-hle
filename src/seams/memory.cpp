// Memory seam: the game's startup memory setup (0x452a). The original shrinks its program block
// and asks DOS for three more (int 21h/4Ah, 48h). Here the function is replaced as a whole: the
// blocks are laid out directly above the program and the game's own variables are set to what
// the DOS allocations would have given it. Nothing answers an interrupt.
//
// Decomp of 0x452a (DS = the game's data segment, [14E2h] = stack segment, [14E8h] = PSP):
//   [158Eh] = 1;  FUN_7c09(0xFF, 0)                    ; display-class setup, game logic: called
//   [0F4Bh] = [14E2h] - [14E8h]                        ; paragraphs the program keeps
//   heap    = alloc([DS+1000h] - [14E2h])              ; block that ends 64 KB past DS: stack + heap
//   [8BAh] = [8BCh] = (heap - DS) << 4 ; [0F4Dh] = heap paragraphs
//   [8C0h] = ([0F4Dh] << 4) + [8BCh]
//   [738h] = alloc(180h) ; [11A7h] = alloc(690h) ; [158Eh] = 0
#include "seams/seams.h"

namespace game::seams {

SeamList Memory() {
    using hlekit::Machine;
    using hlekit::Reg;
    using hlekit::Seg;
    return {
        {0x452a, "init_memory", "memory",
         "FUN_0000452a: shrinks the program block, allocates heap/stack group (ends DS+1000h) and two "
         "work blocks via int 21h/4Ah,48h. Native: lays them out above the program and sets the "
         "game's variables; calls the game's FUN_00007c09 for the display setup.",
         [](Machine& m) {
             const uint16_t ds = m.vm.GetSeg(Seg::Ds);
             const uint32_t base = (uint32_t)ds << 4;
             auto rd = [&](uint16_t off) { return m.vm.Rd16(base + off); };
             auto wr = [&](uint16_t off, uint16_t v) { m.vm.Wr16(base + off, v); };

             wr(0x158e, 1);
             m.CallGuestNear16(0x7c09, {0xff, 0});

             const uint16_t stackSeg = rd(0x14e2), psp = rd(0x14e8);
             wr(0xf4b, stackSeg - psp);
             const uint16_t heapParas = (uint16_t)(0x1000 + ds - stackSeg);
             wr(0xf4d, heapParas);
             const uint16_t heapSeg = stackSeg;                // directly above the kept program
             wr(0x8ba, (uint16_t)((heapSeg - ds) << 4));
             wr(0x8bc, rd(0x8ba));
             wr(0x8c0, (uint16_t)((heapParas << 4) + rd(0x8bc)));
             const uint16_t work = (uint16_t)(heapSeg + heapParas);
             wr(0x738, work);
             wr(0x11a7, (uint16_t)(work + 0x180));
             wr(0x158e, 0);
         }, 0, 0, hlekit::Seam::Ret::Near16},
    };
}

}  // namespace game::seams
