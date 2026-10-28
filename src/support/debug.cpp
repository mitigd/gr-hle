// Debug aid, not a seam: GR_WATCH=1 prints where the guest is twice a second from the 60 Hz timer
// task (delivered only where the guest has IF=1), and how much of the VGA window is lit.
#include "support/debug.h"

#include <cstdio>
#include <cstdlib>

namespace game::support {

void DebugWatch(hlekit::Machine& m) {
    static const bool on = getenv("GR_WATCH") != nullptr;
    if (!on || m.frames % 30) return;
    using hlekit::Reg;
    using hlekit::Seg;
    const uint32_t ss = m.vm.SegBase(Seg::Ss) + (uint16_t)m.vm.Get(Reg::Esp);
    const uint32_t ip = m.vm.Get(Reg::Eip) & 0xffff;
    size_t lit = 0;
    if (const uint8_t* fb = m.vm.Ptr(0xA0000, 64000)) for (int i = 0; i < 64000; ++i) lit += fb[i] != 0;
    fprintf(stderr, "[watch] t=%.1fs linked %05x flags %04x sp %04x [%04x %04x %04x] mode %02x lit %zu\n",
            m.Seconds(), m.Link(m.vm.SegBase(Seg::Cs) + ip), (unsigned)m.vm.Get(Reg::Eflags) & 0xffff,
            (unsigned)m.vm.Get(Reg::Esp) & 0xffff, m.vm.Rd16(ss), m.vm.Rd16(ss + 2), m.vm.Rd16(ss + 4),
            m.videoMode.load(), lit);
}

}  // namespace game::support
