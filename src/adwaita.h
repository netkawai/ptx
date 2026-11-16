#ifndef PTX_ADWAITA_H
#define PTX_ADWAITA_H

/*
 * Adwaita on SDL, "libadwaita.h"-style umbrella for ptx.
 *
 * This header is ptx's stand-in for <libadwaita.h>: it pulls in the
 * mini-toolkit that ptx implements on top of SDL3 (currently the
 * GtkAdjustment / GtkRange / GtkScrollbar family). Include this header,
 * not the individual widget headers, when you need the toolkit.
 *
 * Every public API here is a port of the current GTK4/libadwaita one, so a
 * function call and its documentation read the same as upstream:
 *   https://gitlab.gnome.org/GNOME/gtk/-/tree/main/gtk
 *   https://gitlab.gnome.org/GNOME/libadwaita/-/tree/main/src
 *
 * The rules (see .github/memory/2026-11-16-ui-toolkit-apis-must-mirror-gtk4.md):
 *   - Names are gtk_* equivalent ptx_* names with the same arguments, the
 *     same return values and the same side effects.
 *   - Objects are "classes" as in GObject: a struct with an embedded
 *     parent and a Class struct for the signals/virtual methods.
 *   - Signals become function pointers in the Class struct (there is no
 *     g_signal_connect(); use ptx_<object>_connect()).
 *   - Anything without a GTK4 counterpart is a ptx extension and is marked
 *     as such in the header.
 */

#include "adjustment.h"
#include "orientable.h"
#include "range.h"
#include "scrollbar.h"

#endif /* PTX_ADWAITA_H */
