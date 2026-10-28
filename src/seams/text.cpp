// Text seams: the game's console helpers, which wrapped BIOS int 10h (cursor, scroll/clear, write
// character) and drew glyphs from the ROM font. They are replaced by a native text console that
// draws 8x8 cells straight into the 320x200 VGA window; the cursor lives here, not in a BIOS.
//
// Decomp (near cdecl, arguments at [bp+8]..):
//   FUN_2eb9 set_cursor(row, col)           FUN_2ece push_cursor()    FUN_2eef pop_cursor()
//   FUN_2f4a clear_window(top,left,bottom,right,attr)   cursor unchanged
//   FUN_2f87 scroll_window(top,left,bottom,right,attr)  scrolls up one line, cursor = (bottom,left)
//   FUN_2dac put_char(c): 08h backspace, 0Dh/0Ah newline, else a glyph at the cursor
//   FUN_9a8e / FUN_9a39 draw_menu_item(item): unselected / selected title or dropdown line
// The cursor stack is the game's own: word [0CA5h] (depth in bytes) over words at [0C9Bh+n].
#include "seams/seams.h"

#include "hlekit/font.h"

#include <cstring>

namespace game::seams {
namespace {

using hlekit::Machine;
using hlekit::Reg;
using hlekit::Seam;

int g_row = 0, g_col = 0;
bool g_opaque = false;
bool g_textScreen = false;  // the game's 40x25 text screen (inventory, ...): cells carry their background      // set while a menu item is printed: its cell carries a background

uint32_t Ds() { return (uint32_t)DataSeg() << 4; }
uint16_t Arg(Machine& m, int n) { return m.vm.Rd16(m.StackAddr(2 + 2 * (uint32_t)n)); }

// Colour of a BIOS attribute byte used as a fill: on the text screen the background is the high
// nibble, in the picture it is the whole value (the game passes 0..15).
uint8_t FillColor(uint8_t attr) { return g_textScreen ? (attr >> 4) & 0x0f : attr & 0x0f; }

// The 320x200 VGA window as an 8-pixel-cell surface. The graphics modes draw the 8x8 face (what the
// BIOS used there); the kit resolves it (nothing of the game's own to prefer).
hlekit::TextSurface Surface(Machine& m) {
    static const hlekit::Font font = hlekit::ResolveFont(m, {});
    return hlekit::TextSurface(m, font);
}

void Fill(Machine& m, int top, int left, int bottom, int right, uint8_t color) {
    Surface(m).FillCells(top, left, bottom, right, color);
}

void Scroll(Machine& m, int top, int left, int bottom, int right, uint8_t color) {
    Surface(m).ScrollUp(top, left, bottom, right, color);
}

// Transparent glyph, as the BIOS draws in graphics modes: only set bits are plotted.
void Glyph(Machine& m, int row, int col, uint8_t ch, uint8_t color) { Surface(m).DrawGlyph(row, col, ch, color); }

void NewLine(Machine& m) {
    if (g_row < 0x18) ++g_row;
    g_col = m.vm.Rd8(Ds() + 0xc99);
}

void PutChar(Machine& m, uint8_t c) {
    const uint32_t ds = Ds();
    if (c == 8) {
        if (g_col) --g_col;
        else if (g_row > 0x15) { g_col = 0x27; --g_row; }
        Fill(m, g_row, g_col, g_row, g_col, g_textScreen ? FillColor(m.vm.Rd8(ds + 0x3f1)) : m.vm.Rd8(ds + 0x3ef) & 0x0f);
        return;
    }
    if (c == 0x0d || c == 0x0a) return NewLine(m);
    // FUN_7c09(fg, bg) keeps the real foreground colour in [03EDh]; the attribute word [03F1h] is only a
    // coarse per-machine-class code (0..3, or 8Fh for "inverse"), so it is not used as a colour here.
    // On the text screen ([158Eh] = 1) FUN_7c09 stores a real text attribute, (bg << 4) | fg, in [03F1h].
    const uint8_t attr = m.vm.Rd8(ds + 0x3f1);
    const uint8_t color = g_textScreen ? attr & 0x0f : m.vm.Rd8(ds + 0x3ed) & 0x0f;
    if (g_textScreen) Fill(m, g_row, g_col, g_row, g_col, FillColor(attr));
    // Menu items carry a background ([03EFh]: white for the bar, black for the selected item); the BIOS
    // glyph is transparent, so a menu item's cell is filled first (draw_menu_item sets g_opaque).
    // A space has no pixels to plot: it exists to blank a cell (the restore list erases its selection
    // arrow with one), so it always takes the background.
    if ((g_opaque || c == ' ') && !g_textScreen) Fill(m, g_row, g_col, g_row, g_col, m.vm.Rd8(ds + 0x3ef) & 0x0f);
    Glyph(m, g_row, g_col, c, color);
    if (++g_col > 0x27) NewLine(m);
}

// FUN_9a8e / FUN_9a39: item = word pointer, +4 text, +6 row, +8 col, +0Ah enabled. The original sets
// the colours with FUN_7c09(fg, bg) - unselected 0 on white (bg 0FFh), selected 15 on black - moves the
// cursor and prints the text with FUN_26fe; the dimming through [0CABh] only applies off the VGA path.
void DrawMenuItem(Machine& m, bool selected) {
    const uint16_t item = Arg(m, 0);
    const uint32_t ds = Ds();
    if (selected) m.CallGuestNear16(0x7c09, {0x0f, 0x00}, DataSeg());
    else m.CallGuestNear16(0x7c09, {0x00, 0xff}, DataSeg());
    g_row = (uint8_t)m.vm.Rd16(ds + item + 6);
    g_col = (uint8_t)m.vm.Rd16(ds + item + 8);
    g_opaque = true;
    m.CallGuestNear16(0x26fe, {m.vm.Rd16(ds + item + 4)}, DataSeg());
    g_opaque = false;
}

}  // namespace

void EnterTextScreen(Machine& m) {
    // The overlay's text-mode entry clears the screen with BH = [03F1h]: the attribute's background.
    g_textScreen = true;
    if (uint8_t* fb = m.vm.Ptr(0xA0000, 64000)) memset(fb, FillColor(m.vm.Rd8(Ds() + 0x3f1)), 64000);
    g_row = g_col = 0;
}

void LeaveTextScreen() { g_textScreen = false; }

SeamList Text() {
    return {
        {0x2eb9, "text_set_cursor", "video",
         "FUN_00002eb9: int 10h/02h cursor position (row, col). Native: the console cursor.",
         [](Machine& m) { g_row = (uint8_t)Arg(m, 0); g_col = (uint8_t)Arg(m, 1); },
         0, 0, Seam::Ret::Near16},
        {0x2ece, "text_push_cursor", "video",
         "FUN_00002ece: int 10h/03h read cursor, push it on the game's cursor stack [0CA5h].",
         [](Machine& m) {
             const uint32_t ds = Ds();
             const uint16_t depth = m.vm.Rd16(ds + 0xca5);
             if (depth < 0x0a) {
                 m.vm.Wr16(ds + 0xc9b + depth, (uint16_t)(g_row << 8 | g_col));
                 m.vm.Wr16(ds + 0xca5, depth + 2);
             }
         }, 0, 0, Seam::Ret::Near16},
        {0x2eef, "text_pop_cursor", "video",
         "FUN_00002eef: pop the cursor stack, int 10h/02h to set it.",
         [](Machine& m) {
             const uint32_t ds = Ds();
             uint16_t depth = m.vm.Rd16(ds + 0xca5);
             if (!depth) return;
             depth -= 2;
             m.vm.Wr16(ds + 0xca5, depth);
             const uint16_t rc = m.vm.Rd16(ds + 0xc9b + depth);
             g_row = rc >> 8;
             g_col = rc & 0xff;
         }, 0, 0, Seam::Ret::Near16},
        {0x2f4a, "text_clear_window", "video",
         "FUN_00002f4a: int 10h/06h AL=0 clear of (top,left,bottom,right) with attribute, cursor kept.",
         [](Machine& m) {
                      Fill(m, (uint8_t)Arg(m, 0), (uint8_t)Arg(m, 1), (uint8_t)Arg(m, 2), (uint8_t)Arg(m, 3),
                  FillColor((uint8_t)Arg(m, 4)));
         }, 0, 0, Seam::Ret::Near16},
        {0x2f87, "text_scroll_window", "video",
         "FUN_00002f87: int 10h/06h AL=1 scroll up one line, cursor to (bottom, left).",
         [](Machine& m) {
             Scroll(m, (uint8_t)Arg(m, 0), (uint8_t)Arg(m, 1), (uint8_t)Arg(m, 2), (uint8_t)Arg(m, 3), FillColor((uint8_t)Arg(m, 4)));
             g_row = (uint8_t)Arg(m, 2);
             g_col = (uint8_t)Arg(m, 1);
         }, 0, 0, Seam::Ret::Near16},
        {0x9a8e, "menu_item", "video",
         "FUN_00009a8e: draws an unselected menu title / line: 7c09(0, 0FFh), cursor, print. Native: the "
         "same, with the cell background drawn behind the glyphs.",
         [](Machine& m) { DrawMenuItem(m, false); }, 0, 0, Seam::Ret::Near16},
        {0x9a39, "menu_item_selected", "video",
         "FUN_00009a39: draws the selected menu title / line: 7c09(15, 0), cursor, print. Native: the same, "
         "with a black cell background behind white glyphs.",
         [](Machine& m) { DrawMenuItem(m, true); }, 0, 0, Seam::Ret::Near16},
        {0x2dac, "text_put_char", "video",
         "FUN_00002dac: BIOS teletype-style output of one character (int 10h/03h,02h,06h,09h). Native: "
         "the console draws the glyph at the cursor and advances it.",
         [](Machine& m) { PutChar(m, (uint8_t)Arg(m, 0)); }, 0, 0, Seam::Ret::Near16},
    };
}

}  // namespace game::seams
