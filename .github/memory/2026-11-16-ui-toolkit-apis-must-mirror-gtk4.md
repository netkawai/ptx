---
title: "UI toolkit APIs in ptx must mirror the latest GTK4"
date: 2026-11-16T00:00:00
category: ui-toolkit
related: [tech-other]
sources: [N]
original_query: "keep UI toolkits APIs compatible with gtk4; scrollback/scrollbar was not API compatible"
---

# UI toolkit APIs in ptx must mirror the latest GTK4

## Decision

ptx renders its own widgets on SDL3, but **every widget-facing API we write is
a port of the current GTK4 API, not a fresh design**. This applies to names,
split of types, object ownership, signal names and signal ordering, and
property semantics. If GTK4 has a type or method, we use it with the same
name and meaning (prefixed `ptx_` / `Ptx`); if GTK4 has no equivalent, the
addition is marked as a ptx extension.

The scrollbar is the worked example and the reference implementation:

| GTK4 | ptx |
| --- | --- |
| `GtkAdjustment` (`gtk_adjustment_*`) | `PtxAdjustment` (`ptx_adjustment_*`), same methods incl. `clamp_page`, `configure`, `get_minimum_increment` |
| `::changed`, `::value-changed` signals | `PtxAdjustmentClass.changed` / `.value_changed` default handlers |
| `GtkRange` | `PtxRange` (base class: `change_value`, `move_slider`, `value_changed`, `adjust_bounds`) |
| `GtkScrollbar` (`gtk_scrollbar_*`) | `PtxScrollbar` (`ptx_scrollbar_*`) |
| `GtkOrientable` | `PtxOrientable` interface, `ptx_orientable_get/set_orientation` |
| `GtkScrollType` / `GtkOrientation` / `GtkPositionType` | `PtxScrollType` / `PtxOrientation` / `PtxPositionType`, same members |

## Rationale

- Keeps behaviour (steppers, page-jump centring, fine-tune, wheel step of
  `min(3 * step, page * 0.9)`, `page_size`-derived thumb) identical to what a
  GTK4 user already knows, instead of drifting to an ad-hoc scrollbar.
- Makes reviews mechanical: diff our header against docs.gtk.org/gtk4.
- Widget code stays backend-portable: only drawing and hit-testing are
  SDL-specific, the model (adjustment) and controller (range) are ordinary C.

## Consequences

- `scroll_offset` is gone. The viewport is `value` in "lines scrolled from
  the bottom" (0 = live screen, `upper - page_size` = oldest line).
- Anything that used to poke `app->scroll_offset` now goes through
  `ptx_adjustment_set_value()` and the `value_changed` handler in
  `src/buffer.c` (which adjusts the selection so it points at the same text
  while the view moves).
- Geometry remains the terminal's: vertical, `SCROLLBAR_W` (12 px) wide,
  libadwaita-like overlay styling (rounded translucent trough + slider that
  thickens on hover/while dragging). GTK4 has no `get_trough_widget()`;
  the slider rectangle comes from `ptx_range_slider_geometry()`.

## Files

- `src/adjustment.h`, `src/adjustment.c`
- `src/range.h`, `src/range.c`
- `src/scrollbar.h`, `src/scrollbar.c`
- `src/orientable.h`, `src/orientable.c`
- `src/buffer.c` (adjustment owner + `value_changed` handler)
- `src/render.c`, `src/main.c`, `src/layout.c`, `src/input.c`

Reference: <https://docs.gtk.org/gtk4/class.Adjustment.html>,
<https://docs.gtk.org/gtk4/class.Range.html>,
<https://docs.gtk.org/gtk4/class.Scrollbar.html>
