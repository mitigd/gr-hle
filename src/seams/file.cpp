// File seams: the game's own C-library file wrappers (open/create/read/write/close/lseek/dup/unlink,
// directory search, drive and cwd queries around DOS int 21h). Each wrapper is replaced as a
// function; the host side is the kit's sandboxed file layer (hlekit/files.h, `m.files`): every guest
// path resolves inside the game directory, reads see the overlay first, writes go to the overlay
// only, so saved games land there and are read back from it.
//
// Convention of every wrapper (decomp, 0x613b..0x6329): near cdecl, `push si/di/bp` prologue,
// arguments at [bp+8].., result in AX (0xFFFF = failure for handle/position results). Pointers
// are near, relative to the caller's DS. The caller pops the arguments (pop = 0).
#include "seams/seams.h"

#include <cstdio>
#include <string>
#include <vector>

namespace game::seams {
namespace {

using hlekit::Machine;
using hlekit::Reg;
using hlekit::Seg;

constexpr uint16_t kFail = hlekit::HostFiles::kFail;
constexpr uint16_t kTableSeg = 0x9b9;  // overlay table segment (unrelocated; + load segment)

uint16_t Arg(Machine& m, int n) { return m.vm.Rd16(m.StackAddr(2 + 2 * (uint32_t)n)); }

// The sandboxed path the near pointer argument `n` names.
std::string PathArg(Machine& m, int n) { return m.files.GuestRel(m.vm.SegBase(Seg::Ds) + Arg(m, n)); }

// Directory search state shared by findfirst/findnext: the DTA the game gave and the matches.
uint32_t g_dta = 0;
std::vector<hlekit::HostFiles::Entry> g_found;
size_t g_next = 0;

// Writes the next match into the DTA and sets AX: 0 on success, 0xFFFF when there is none.
void FillDta(Machine& m) {
    if (g_next >= g_found.size()) { m.vm.Set16(Reg::Eax, kFail); return; }
    m.files.WriteDta(g_dta, g_found[g_next++]);
    m.vm.Set16(Reg::Eax, 0);
}

}  // namespace

SeamList File() {
    using hlekit::Seam;
    const auto Near16 = Seam::Ret::Near16;
    return {
        {0x615c, "file_open", "file",
         "open(path, mode): int 21h/3Dh wrapper, AL = access mode; AX = handle or 0xFFFF. Native: "
         "overlay/game-dir lookup; write modes go to the overlay.",
         [](Machine& m) { m.vm.Set16(Reg::Eax, m.files.Open(PathArg(m, 0), Arg(m, 1))); }, 0, 0, Near16},
        {0x617d, "file_read", "file",
         "read(handle, buf, count): int 21h/3Fh wrapper; AX = bytes read (0 on error). Native: fread.",
         [](Machine& m) {
             m.vm.Set16(Reg::Eax, (uint16_t)m.files.ReadTo(Arg(m, 0), m.vm.SegBase(Seg::Ds) + Arg(m, 1), Arg(m, 2)));
         }, 0, 0, Near16},
        {0x61a0, "file_write", "file",
         "write(handle, buf, count): int 21h/40h wrapper; AX = bytes written (0 on error).",
         [](Machine& m) {
             m.vm.Set16(Reg::Eax, (uint16_t)m.files.WriteFrom(Arg(m, 0), m.vm.SegBase(Seg::Ds) + Arg(m, 1), Arg(m, 2)));
         }, 0, 0, Near16},
        {0x61c3, "file_unlink", "file",
         "unlink(path): int 21h/41h wrapper. Native: removes the overlay copy only.",
         [](Machine& m) {
             m.files.Remove(PathArg(m, 0));
             m.vm.Set16(Reg::Eax, 0);
         }, 0, 0, Near16},
        {0x6c3b, "load_overlay", "file",
         "load_overlay(n): opens the n-th entry of the overlay table (DS=9B9h, 16-byte rows: +8 load "
         "segment, +0Ch name, +12h paragraphs), reads it to seg:0 with int 21h/3Fh, closes it. Native: "
         "the whole read, no DOS handle.",
         [](Machine& m) {
             const uint32_t table = ((uint32_t)(kTableSeg + Machine::kRealPsp + 0x10)) << 4;
             const uint32_t row = table + ((uint32_t)Arg(m, 0) - 1) * 16 + 4;
             const uint16_t seg = m.vm.Rd16(row + 4), name = m.vm.Rd16(row + 8);
             const uint32_t bytes = (uint32_t)m.vm.Rd16(row + 0xe) * 16;
             const std::string rel = m.files.GuestRel(table + name, 64);
             const uint16_t h = m.files.Open(rel, 0);
             if (h != kFail) {
                 const size_t n = m.files.ReadTo(h, (uint32_t)seg << 4, bytes);
                 m.files.Close(h);
                 if (m.logServices) fprintf(stderr, "[file] overlay %u '%s' -> seg %04x, %zu of %u bytes\n", Arg(m, 0), rel.c_str(), seg, n, bytes);
                 if (Arg(m, 0) == 5) m.InstallSeams(OverlaySeams(), {});   // vg_graf: re-arm its BIOS seams
             } else {
                 fprintf(stderr, "[file] overlay %u: %s not found\n", Arg(m, 0), rel.c_str());
             }
         }, 0, 0, Near16},
        {0x61e0, "file_close", "file",
         "close(handle): int 21h/3Eh wrapper.",
         [](Machine& m) { m.files.Close(Arg(m, 0)); }, 0, 0, Near16},
        {0x61f9, "file_lseek", "file",
         "lseek(handle, off_lo, off_hi, whence): int 21h/42h wrapper; DX:AX = new position or -1.",
         [](Machine& m) {
             const long off = (long)(int32_t)((uint32_t)Arg(m, 2) << 16 | Arg(m, 1));
             uint32_t pos = 0;
             if (!m.files.Seek(Arg(m, 0), off, Arg(m, 3) & 3, pos)) {
                 m.vm.Set16(Reg::Eax, kFail);
                 m.vm.Set16(Reg::Edx, kFail);
                 return;
             }
             m.vm.Set16(Reg::Eax, (uint16_t)pos);
             m.vm.Set16(Reg::Edx, (uint16_t)(pos >> 16));
         }, 0, 0, Near16},
        {0x613b, "file_create", "file",
         "create(path, attr): int 21h/3Ch wrapper; AX = handle or 0xFFFF. Native: a new (truncated) "
         "file in the overlay.",
         [](Machine& m) { m.vm.Set16(Reg::Eax, m.files.Create(PathArg(m, 0))); }, 0, 0, Near16},
        {0x6222, "file_dup", "file",
         "dup(handle): int 21h/45h wrapper; a second handle on the same stream (shared position).",
         [](Machine& m) { m.vm.Set16(Reg::Eax, m.files.Dup(Arg(m, 0))); }, 0, 0, Near16},
        {0x6240, "file_getcwd", "file",
         "getcwd(buf): int 21h/47h; buf = '\\' + directory. Native: the virtual root, \"\\\".",
         [](Machine& m) {
             const uint32_t buf = m.vm.SegBase(Seg::Ds) + Arg(m, 0);
             m.vm.Wr8(buf, '\\');
             m.vm.Wr8(buf + 1, 0);
         }, 0, 0, Near16},
        {0x6278, "file_getdrive", "file",
         "getdrive(): int 21h/19h as a lower-case letter. Native: 'c', the game's only drive.",
         [](Machine& m) { m.vm.Set16(Reg::Eax, 'c'); }, 0, 0, Near16},
        {0x62cc, "file_setdrive", "file",
         "setdrive(letter): int 21h/0Eh then /19h; AX = 1 if the drive took. Native: only 'c' exists.",
         [](Machine& m) { m.vm.Set16(Reg::Eax, (uint8_t)Arg(m, 0) == 'c' ? 1 : 0); }, 0, 0, Near16},
        {0x628f, "file_findfirst", "file",
         "findfirst(spec, attr, dta): int 21h/1Ah + 4Eh; AX = 0 and the DTA filled (+15h attr, +1Ah size, "
         "+1Eh name) or 0xFFFF. Native: matches the spec in the overlay and game directory.",
         [](Machine& m) {
             const std::string spec = PathArg(m, 0);
             g_dta = m.vm.SegBase(Seg::Ds) + Arg(m, 2);
             const size_t slash = spec.rfind('/');
             g_found = m.files.List(slash == std::string::npos ? "" : spec.substr(0, slash),
                                    slash == std::string::npos ? spec : spec.substr(slash + 1), false);
             g_next = 0;
             FillDta(m);
         }, 0, 0, Near16},
        {0x62b4, "file_findnext", "file",
         "findnext(): int 21h/4Fh; next match into the DTA set by findfirst, 0xFFFF at the end.",
         [](Machine& m) { FillDta(m); }, 0, 0, Near16},
        {0x6301, "file_time", "file",
         "filetime(handle): int 21h/57h; AX = the time word. Native: a fixed time (the files' own are not kept).",
         [](Machine& m) { m.vm.Set16(Reg::Eax, 0x6000); }, 0, 0, Near16},
        {0x6329, "disk_reset", "file",
         "reset_disks(): int 21h/0Dh and int 13h/00h after a disk swap. Native: nothing to flush.",
         [](Machine&) {}, 0, 0, Near16},
    };
}

}  // namespace game::seams
