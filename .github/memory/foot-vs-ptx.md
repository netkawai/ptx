# ptx: scrollback + clipboard, and why coming from foot is expensive

## What I added to ptx

| Feature | How |
|---|---|
| **Scrollback** (5000 lines) | libvterm `sb_pushline`/`sb_popline`/`sb_clear` callbacks into a compact ring buffer; viewport stays anchored while scrolled up |
| **Scrollbar** | drawn in a reserved 12 px right gutter; click/drag jumps; thumb size reflects history |
| **Mouse selection** | drag across scrollback + live screen; auto-copies on release |
| **Cross-window copy/paste** | `Ctrl+Shift+C/V` (and `Cmd+C/V`) → `SDL_SetClipboardText` + `SDL_SetPrimarySelectionText` (Linux middle-click); paste normalizes `\n`→`\r` and writes to the pty |
| **Mouse wheel scroll** | 3 lines/notch |
| **Real key map** | arrows, Home/End, PgUp/PgDn, Ins/Del, F1–F12, `Ctrl+<letter>`, Alt-prefix, Tab/Esc/Enter/Backspace |
| **Thread safety** | mutex around *all* libvterm access — the old code raced the reader thread against rendering |
| **Cheap idle** | dirty-flag rendering + blink timer, so an idle terminal costs ~0% CPU |

Two latent bugs fixed along the way: the old UTF-8 encoder silently dropped every codepoint ≥ U+0800 (so most non-Latin text vanished), and wide chars/reverse video/default colors weren't handled.

Build fix: `libvterm` records its install name as `/usr/local/lib/...` even though it lives in `~/.local/lib`. rpath doesn't apply to absolute install names, so I added an `install_name_tool` rewrite (plus rpath for Linux) to `build-ptx.sh`. It now launches with no `DYLD_*` hacks.

---

## Why "coming from foot" is expensive

**1. foot doesn't use libvterm — it *is* a VT engine.** Your stack is `SDL3 + libvterm`. foot's is `Wayland + its own parser`. So "port foot features" isn't adding features to your engine, it's replacing your engine. foot's parser alone is ~15k LOC (`vt.c` 1133, `csi.c` 2271, `osc.c` 1735, `dcs.c`, `escape`), and it exists *because* libvterm deliberately doesn't implement what foot needs.

**2. The features are welded to foot's data model.** foot's sixel (`sixel.c` 2228), selection (2969), search (1512), and renderer all read from `struct terminal`/`struct grid` — a different representation than libvterm's screen. You can't copy `sixel.c` in; you'd rewrite each feature against libvterm's API. That's a rewrite, not a port.

**3. libvterm isn't yours.** It's upstream (Paul Evans, also used by Neovim) and intentionally small: no sixel, no kitty keyboard/graphics protocols, no undercurl. You can't cheaply bolt foot's versions on, because those live inside foot, not behind a library boundary.

**4. Even the "just swap Wayland" path is welded.** Wayland is genuinely the *easy* part to replace. The cost is that foot's core references it everywhere:

| file | `wl_`/`pixman` references |
|---|---|
| `render.c` | 347 |
| `input.c` | 113 |
| `selection.c` | 117 |
| `sixel.c` | 107 |
| `terminal.h` | embeds `struct wl_window *` |

So it's not "replace `wayland.c`" — it's decoupling `render/input/selection/sixel` from Wayland structs first.

**5. Two event loops, two input models.** foot runs an `epoll`/`timerfd`/`eventfd` loop (`fdm.c`, `wayland.c`) and drives keys through xkbcommon + `timerfd` key repeat directly (`input.c`). SDL3 owns its own event loop and abstracts keyboard/IME/repeat. Merging them means tearing out foot's input model and re-expressing every keybinding through SDL's — which changes how *all* bindings resolve.

**6. Overlays and scaling.** foot composites overlays (search box, scrollback indicator, URL hints) as Wayland **subsurfaces**; SDL3 has no subsurface concept, so you'd reimplement overlay compositing in a single surface. Same story for fractional HiDPI scaling (`wp_fractional_scale`) and server-side decorations.

**7. Economics.** foot is **53,414 LOC** with active upstream churn. Reimplementing its feature set is a multi-month project you'd then maintain forever against upstream fixes — for features that are "nice to have, not primary."

**Contrast:** everything actually needed is ~600 lines on top of SDL3 + libvterm, because libvterm already does the hard VT parsing and SDL3 already does clipboard + multi-platform windows. The expensive parts of foot are exactly the parts not needed yet.

If foot-class features are ever wanted, the cheapest route isn't porting into ptx or forking foot — it's vendoring foot's platform-agnostic files (`vt.c`, `csi.c`, `osc.c`, `dcs.c`, `grid.c`, `char32.c`, `box-drawing.c`) and writing your own SDL3 renderer, keeping ptx's front-end. That reuses the parser without dragging in Wayland.
