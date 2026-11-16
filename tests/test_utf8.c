/*
 * Regression test for the "stray b" bug.
 *
 * ptx must run libvterm in UTF-8 mode. Otherwise a TUI's UTF-8 box glyphs
 * (U+250C = e2 94 8c ...) are decoded one byte at a time through the
 * 94-character GL charset, and each byte lands as a 'b'-family cell —
 * opencode/ratatui panels render as "bbbb".
 *
 * This pins:
 *   - UTF-8 box-drawing glyphs decode to U+2500..U+257F,
 *   - ESC(0 DEC Special Graphics still maps to the same codepoints,
 *   - multi-byte text (CJK) survives.
 */
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <vterm.h>

static uint32_t cell_at(VTermScreen *vts, int row, int col) {
    VTermScreenCell cell;
    memset(&cell, 0, sizeof(cell));
    vterm_screen_get_cell(vts, (VTermPos){ row, col }, &cell);
    return cell.chars[0];
}

int main(void) {
    VTerm *vt = vterm_new(4, 20);
    vterm_set_utf8(vt, 1);
    VTermScreen *vts = vterm_obtain_screen(vt);
    vterm_screen_reset(vts, 1);

    /* U+250C U+2500 U+2500 then U+2510 on row 0. */
    static const char panel[] =
        "\xe2\x94\x8c\xe2\x94\x80\xe2\x94\x80\xe2\x94\x90";
    vterm_input_write(vt, panel, sizeof(panel) - 1);

    /* With UTF-8 off the first cell is 0x62 ('b', from U+2409). */
    assert(cell_at(vts, 0, 0) == 0x250C);
    assert(cell_at(vts, 0, 1) == 0x2500);
    assert(cell_at(vts, 0, 3) == 0x2510);

    /* Vertical stroke on its own row. */
    vterm_screen_reset(vts, 1);
    static const char vert[] = "\xe2\x94\x82";
    vterm_input_write(vt, vert, sizeof(vert) - 1);
    assert(cell_at(vts, 0, 0) == 0x2502);

    vterm_screen_reset(vts, 1);
    static const char dec[] = "\x1b" "(0lqk\x1b" "(B";
    vterm_input_write(vt, dec, sizeof(dec) - 1);
    assert(cell_at(vts, 0, 0) == 0x250C);
    assert(cell_at(vts, 0, 1) == 0x2500);
    assert(cell_at(vts, 0, 2) == 0x2510);

    vterm_screen_reset(vts, 1);
    static const char cjk[] = "\xe4\xb8\xad";      /* U+4E2D */
    vterm_input_write(vt, cjk, sizeof(cjk) - 1);
    assert(cell_at(vts, 0, 0) == 0x4E2D);

    vterm_free(vt);
    printf("all utf8 tests passed\n");
    return 0;
}
