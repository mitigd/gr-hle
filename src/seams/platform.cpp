// Platform seams: the game's startup probe of the host PC (0x4416). The original reads BIOS and ROM
// bytes and asks the BIOS (int 11h, 15h, 10h) what it is running on. Nothing is asked: the function
// is replaced and sets the two variables the rest of the game branches on, choosing the best
// graphics path (the VGA overlay, vg_graf.ovl), then runs init_memory.
//
//   [0F4Fh] machine class : 8 = PS/2-class with a PC-speaker voice
//   [0F51h] display       : 4 = VGA overlay (8630h picks overlay 5 for it)
//
// And the other end of the process: the game's exit wrapper (0x1a7, DOS terminate) just stops the
// host.
#include "seams/seams.h"

#include <string>

namespace game::seams {

SeamList Platform() {
    using hlekit::Machine;
    using hlekit::Seg;
    return {
        {0x4416, "select_machine", "machine",
         "FUN_00004416: BIOS/ROM/equipment probing, then FUN_0000452a. Native: selects the VGA graphics "
         "path and runs init_memory.",
         [](Machine& m) {
             const uint32_t base = (uint32_t)m.vm.GetSeg(Seg::Ds) << 4;
             m.vm.Wr16(base + 0xf4f, 8);
             m.vm.Wr16(base + 0xf51, 4);
             m.vm.Wr16(base + 0xf49, 0);
             m.CallGuestNear16(0x452a);
         }, 0, 0, hlekit::Seam::Ret::Near16},
        {0x1a7, "exit", "machine",
         "FUN_000001a7: int 21h/4Ch terminate with the code in AL. Native: stop the host.",
         [](Machine& m) {
             const uint16_t code = m.vm.Rd16(m.StackAddr(2)) & 0xff;
             m.Stop("exit(" + std::to_string(code) + ")");
         }, 0, 0, hlekit::Seam::Ret::Near16},
    };
}

}  // namespace game::seams
