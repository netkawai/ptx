---
title: "In another chat, you can successfully create a source code with Paul Evan’s libvterm on top of SDL3/SDL3-ttf with window resize and cursor caret. Now how to implement scroll buffer? As I mentioned UI toolkis I want to follow GNOME HIG and functions signature to follow libadwaita/gtk4 without using them using Clay and top of direct SDL3 rendering engine"
date: 2026-09-06T18:51:50
category: pc-and-linux-history
related: []
sources: [N]
original_query: "In another chat, you can successfully create a source code with Paul Evan’s libvterm on top of SDL3/SDL3-ttf with window resize and cursor caret. Now how to implement scroll buffer? As I mentioned UI toolkis I want to follow GNOME HIG and functions signature to follow libadwaita/gtk4 without using them using Clay and top of direct SDL3 rendering engine"
---

# In another chat, you can successfully create a source code with Paul Evan’s libvterm on top of SDL3/SDL3-ttf with window resize and cursor caret. Now how to implement scroll buffer? As I mentioned UI toolkis I want to follow GNOME HIG and functions signature to follow libadwaita/gtk4 without using them using Clay and top of direct SDL3 rendering engine

- **Date:** Sep 6, 2026, 6:51:50 PM EDT
- **Category:** `pc-and-linux-history`
- **Sources:** 2026-10-10 export
- **Original query:** In another chat, you can successfully create a source code with Paul Evan’s libvterm on top of SDL3/SDL3-ttf with window resize and cursor caret. Now how to implement scroll buffer? As I mentioned UI toolkis I want to follow GNOME HIG and functions signature to follow libadwaita/gtk4 without using them using Clay and top of direct SDL3 rendering engine

---

## Prompt 1

In another chat, you can successfully create a source code with Paul Evan’s libvterm on top of SDL3/SDL3-ttf with window resize and cursor caret. Now how to implement scroll buffer? As I mentioned UI toolkis I want to follow GNOME HIG and functions signature to follow libadwaita/gtk4 without using them using Clay and top of direct SDL3 rendering engine

## Response 1

Implementing a scrollback buffer with `libvterm` on a custom `SDL3` / `Clay` rendering engine requires shifting away from direct screen-space mapping to a virtualized view. In `libvterm`, the "screen" layer (`VTermScreen`) only tracks the active, visible terminal grid. To support scrolling, you must intercept lines as they are pushed off the top of the terminal screen and store them in a custom ring buffer.

Here is the architectural blueprint and function signatures designed to mirror **GTK4/Libadwaita** conventions and **GNOME HIG** structural paradigms, adapted for a custom `SDL3` and `Clay` UI hierarchy.

### 1. Architectural Strategy

- **The Scrollback Storage:** Treat `libvterm` as the active "view." When `libvterm` triggers its `sb_pushline` callback, you deep-copy the cell array (`VTermScreenCell`) into a custom, dynamically allocated array or ring buffer managed by your application.
- **The Viewport Adjustment:** Track a `vterm_scroll_offset` variable. If it is `0`, you render the live `libvterm` screen. If it is `> 0`, you offset the rows being drawn, pulling rows out of your custom scrollback buffer instead of `libvterm`.
- **The Clay UI Integration:** In [GNOME HIG](https://developer.gnome.org/hig/), scrollable areas are managed by a viewport wrapper. In your `Clay` layout code, you will wrap your terminal canvas component inside a scroll container element that captures mouse wheel inputs and structural scrollbar updates.

### 2. GTK4/Libadwaita-Style Interface Design

To make your internal terminal components feel familiar to a GTK4/Libadwaita developer, use an object-oriented C structural layout with explicit instantiation, properties, and signal-like mutation patterns.

```
#include <vterm.h>
#include <sdl3/SDL.h>
#include <clay.h>

// Represents a historical line in the scrollback buffer
typedef struct _AdwTerminalLine {
    int width;
    VTermScreenCell *cells;
} AdwTerminalLine;

// The main widget structure following GObject/Libadwaita design patterns
typedef struct _AdwTerminalView {
    // Instance properties
    VTerm           *vterm;
    VTermScreen     *vterm_screen;
    
    // Scrollback state
    AdwTerminalLine *scrollback_buffer; // Array acting as a ring buffer
    int              scrollback_max_lines;
    int              scrollback_count;
    int              scrollback_head;   // Pointer to oldest line
    
    // Viewport adjustment matching GtkAdjustment functionality
    int              scroll_value;      // Current scroll position (0 = bottom)
    int              scroll_upper;      // Maximum scrollable historical height
    int              page_size;         // Visible rows in terminal window
} AdwTerminalView;

/**
 * adw_terminal_view_new:
 * Creates a new pseudo-Libadwaita terminal controller.
 */
AdwTerminalView* adw_terminal_view_new(int cols, int rows, int max_scrollback) {
    AdwTerminalView *self = malloc(sizeof(AdwTerminalView));
    memset(self, 0, sizeof(AdwTerminalView));
    
    self->vterm = vterm_new(rows, cols);
    self->vterm_screen = vterm_obtain_screen(self->vterm);
    vterm_screen_reset(self->vterm_screen, 1);
    
    self->scrollback_max_lines = max_scrollback;
    self->scrollback_buffer = calloc(max_scrollback, sizeof(AdwTerminalLine));
    self->page_size = rows;
    
    // Wire up the libvterm screen callbacks (shown below)
    setup_vterm_callbacks(self);
    
    return self;
}
```

### 3. Implementing the Libvterm Push Callback

`libvterm` handles scrollback by notifying you when a line is erased from the top of the visible screen due to a scroll event (like newlines at the bottom of the page).

```
// Callback registered to VTermScreenCallbacks.sb_pushline
static int on_vterm_sb_pushline(int cols, const VTermScreenCell *cells, void *user_data) {
    AdwTerminalView *self = (AdwTerminalView*)user_data;
    
    // Determine the slot in our ring buffer
    int slot = (self->scrollback_head + self->scrollback_count) % self->scrollback_max_lines;
    
    // If the buffer is completely full, free the memory of the overwritten line
    if (self->scrollback_count == self->scrollback_max_lines) {
        free(self->scrollback_buffer[slot].cells);
        self->scrollback_head = (self->scrollback_head + 1) % self->scrollback_max_lines;
    } else {
        self->scrollback_count++;
    }
    
    // Store the line data
    self->scrollback_buffer[slot].width = cols;
    self->scrollback_buffer[slot].cells = malloc(cols * sizeof(VTermScreenCell));
    memcpy(self->scrollback_buffer[slot].cells, cells, cols * sizeof(VTermScreenCell));
    
    // Update structural scroll limits
    self->scroll_upper = self->scrollback_count;
    
    return 1; // Success
}
```

### 4. Viewport Translation & Rendering Logic

When drawing the terminal, your rendering function must switch context from reading directly from `vterm_screen` to reading from your historical `scrollback_buffer` depending on where the user has scrolled.

```
/**
 * adw_terminal_view_get_cell:
 * Emulates widget cell-fetching architecture while abstracting historical lookups.
 */
void adw_terminal_view_get_cell(AdwTerminalView *self, int col, int row, VTermScreenCell *out_cell) {
    int total_history_viewable = self->scroll_upper;
    
    // Check if the requested row falls into the historical scrollback zone
    if (self->scroll_value > 0 && row < self->scroll_value) {
        int history_index = total_history_viewable - self->scroll_value + row;
        if (history_index >= 0 && history_index < self->scroll_count) {
            int slot = (self->scrollback_head + history_index) % self->scrollback_max_lines;
            if (col < self->scrollback_buffer[slot].width) {
                *out_cell = self->scrollback_buffer[slot].cells[col];
                return;
            }
        }
        // Fallback fallback for out-of-bounds column space
        memset(out_cell, 0, sizeof(VTermScreenCell));
        return;
    }
    
    // Otherwise, calculate the adjusted relative row within libvterm's live view
    VTermPos pos = { .row = row - self->scroll_value, .col = col };
    vterm_screen_get_cell(self->vterm_screen, pos, out_cell);
}
```

### 5. Integrating with Clay UI (GNOME HIG Layout)

Following **GNOME HIG**, your terminal element should look seamless inside a clean window border alongside a thin scrollable trough. In `Clay`, you create a declarative horizontal container holding the terminal text grid and a sleek, rounded scroll indicator.

```
/**
 * adw_terminal_view_render_ui:
 * Generates the declarative layout using Clay blocks and direct SDL3 render commands.
 */
void adw_terminal_view_render_ui(AdwTerminalView *self, float font_width, float font_height) {
    // Parent Container mimicking AdwViewStack/GtkScrolledWindow
    CLAY(
        CLAY_LAYOUT({ 
            .layoutDirection = CLAY_LAYOUT_DIRECTION_HORIZONTAL, 
            .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) } 
        }),
        CLAY_BACKGROUND_COLOR((Clay_Color){30, 30, 30, 255}) // GNOME dark theme background
    ) {
        
        // 1. The Terminal Screen Matrix
        CLAY(
            CLAY_LAYOUT({ 
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) } 
            }),
            CLAY_ID("TerminalTextCanvas")
        ) {
            // Your custom logic will loop over self->page_size and column counts
            // calling adw_terminal_view_get_cell() and submitting glyphs via SDL3.
        }
        
        // 2. GNOME HIG Style Minimalist Scrollbar Track
        float scroll_percentage = (self->scroll_upper == 0) ? 1.0f : 
            1.0f - ((float)self->scroll_value / self->scroll_upper);
            
        CLAY(
            CLAY_LAYOUT({ 
                .width = CLAY_SIZING_FIXED(8), // Subtle thumb lane matching GNOME look
                .height = CLAY_SIZING_GROW(0),
                .padding = { .top = 4, .bottom = 4, .left = 2, .right = 2 }
            }),
            CLAY_BACKGROUND_COLOR((Clay_Color){45, 45, 45, 255})
        ) {
            // Scrollbar Thumb
            CLAY(
                CLAY_LAYOUT({
                    .width = CLAY_SIZING_GROW(0),
                    .height = CLAY_SIZING_PERCENT(0.2f) // Can be calculated based on page ratio
                }),
                CLAY_BACKGROUND_COLOR((Clay_Color){100, 100, 100, 150}) // Translucent grey capsule
            ) {}
        }
    }
}
```

### 6. Mutation API (Following GtkAdjustment Signals)

To complete the implementation, mutate your state via standard accessor methods. When processing mouse wheel events from `SDL_PollEvent`, update the values through this functional entry point:

```
/**
 * adw_terminal_view_scroll_to:
 * Mutates viewport positioning safely within bounds.
 */
void adw_terminal_view_scroll_to(AdwTerminalView *self, int target_value) {
    if (target_value < 0) target_value = 0;
    if (target_value > self->scroll_upper) target_value = self->scroll_upper;
    
    if (self->scroll_value != target_value) {
        self->scroll_value = target_value;
        // Equivalent to emitting an "adjustment-changed" signal to queue a redraw
        Clay_QueueRefresh(); 
    }
}
```

If you are ready to stitch this into your core main loop, would you like me to map out how the **SDL3 mouse-wheel scroll integration** updates `adw_terminal_view_scroll_to`, or should we detail how to reset the `scroll_value` automatically to `0` whenever a **new text stroke input** arrives?

---

## Prompt 2

but I need to separate scrollbars rendering into libadwaita-sdl

## Response 2

To isolate the scrollbar logic and align completely with the object-oriented structure of GObject, GTK4, and Libadwaita, we can decouple the rendering and layout of the scroll container from the core terminal logic.

In GTK4/Libadwaita, a scrollable layout is achieved by wrapping a scrollable widget inside a `GtkScrolledWindow` (or an Adwaita-specific container), which implicitly tracks a `GtkAdjustment` for vertical metrics.

Here is the clean separation of concerns using C, **Clay**, and **SDL3**, providing you with a reusable `AdwScrolledWindow` implementation for your **libadwaita-sdl** architecture.

### 1. The Scroll Architecture (The Adjustment)

First, we create an abstraction of `GtkAdjustment`. This structure holds the current position, the scrolling bounds, and step increments, fully decoupled from the terminal logic itself.

```
// adw-adjustment.h
#pragma once

typedef struct _AdwAdjustment {
    double value;          // Current scroll position
    double lower;          // Minimum value (usually 0)
    double upper;          // Maximum value (e.g., total history height)
    double step_increment; // Scroll distance per wheel tick (e.g., 1 line)
    double page_size;      // Visible height (e.g., visible rows)
} AdwAdjustment;

static inline AdwAdjustment* adw_adjustment_new(double value, double lower, double upper, double step, double page) {
    AdwAdjustment *adj = malloc(sizeof(AdwAdjustment));
    adj->value = value;
    adj->lower = lower;
    adj->upper = upper;
    adj->step_increment = step;
    adj->page_size = page;
    return adj;
}
```

### 2. The Scrolled Window Object (libadwaita-sdl Interface)

This component functions exactly like a `GtkScrolledWindow`. It accepts any generic UI rendering callback function as its child content, manages the **Clay** structural layout, and draws a GNOME-style minimalist scrollbar.

```
// adw-scrolled-window.h
#pragma once
#include <clay.h>
#include <sdl3/SDL.h>
#include "adw-adjustment.h"

typedef void (*AdwWidgetRenderFunc)(void *widget_data);

typedef struct _AdwScrolledWindow {
    AdwAdjustment *v_adjustment;
    bool           v_scrollbar_visible;
    
    // Child registration
    void                *child_data;
    AdwWidgetRenderFunc  child_render_callback;
} AdwScrolledWindow;

/**
 * adw_scrolled_window_new:
 * Instantiates an encapsulated scrolling wrapper frame.
 */
AdwScrolledWindow* adw_scrolled_window_new(AdwAdjustment *v_adjust) {
    AdwScrolledWindow *self = malloc(sizeof(AdwScrolledWindow));
    self->v_adjustment = v_adjust ? v_adjust : adw_adjustment_new(0, 0, 0, 1, 10);
    self->v_scrollbar_visible = true;
    self->child_data = NULL;
    self->child_render_callback = NULL;
    return self;
}

/**
 * adw_scrolled_window_set_child:
 * Registers the inner content widget (like your terminal canvas).
 */
void adw_scrolled_window_set_child(AdwScrolledWindow *self, void *data, AdwWidgetRenderFunc callback) {
    self->child_data = data;
    self->child_render_callback = callback;
}
```

### 3. GNOME HIG Layout & Rendering (Clay Integration)

Following the **GNOME HIG**, scrollbars should be overlayed or placed flush to the edge of the viewport. They feature an ultra-thin minimalist design that dynamically sizes the thumb handle relative to the scroll upper limits.

```
// adw-scrolled-window.c
#include "adw-scrolled-window.h"

/**
 * adw_scrolled_window_render:
 * Builds the cross-widget layout using visual paradigms extracted from libadwaita.
 */
void adw_scrolled_window_render(AdwScrolledWindow *self) {
    AdwAdjustment *adj = self->v_adjustment;
    
    // Outermost layout layout mimicking GtkScrolledWindow
    CLAY(
        CLAY_LAYOUT({
            .layoutDirection = CLAY_LAYOUT_DIRECTION_HORIZONTAL,
            .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }
        }),
        CLAY_ID("AdwScrolledWindowContainer")
    ) {
        
        // 1. Content Viewport Box
        CLAY(
            CLAY_LAYOUT({
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }
            }),
            CLAY_ID("AdwScrolledViewport")
        ) {
            if (self->child_render_callback && self->child_data) {
                // Execute the layout definitions of the terminal view inside this block
                self->child_render_callback(self->child_data);
            }
        }
        
        // 2. GNOME Adwaita Minimalist Scrollbar Trough
        if (self->v_scrollbar_visible && adj->upper > adj->page_size) {
            double range = adj->upper - adj->lower;
            double progress = (range > 0) ? (adj->value / range) : 0.0;
            double thumb_ratio = (range > 0) ? (adj->page_size / adj->upper) : 0.2;
            if (thumb_ratio < 0.1) thumb_ratio = 0.1; // Establish minimum handle safety height
            
            CLAY(
                CLAY_LAYOUT({
                    .width = CLAY_SIZING_FIXED(10), // Minimalist 10px track matching modern GTK4
                    .height = CLAY_SIZING_GROW(0),
                    .layoutDirection = CLAY_LAYOUT_DIRECTION_VERTICAL,
                    .padding = { .top = 4, .bottom = 4, .left = 3, .right = 3 }
                }),
                CLAY_BACKGROUND_COLOR((Clay_Color){35, 35, 35, 255}), // Adw-dark sidebar track accent
                CLAY_ID("AdwVerticalScrollbarTrough")
            ) {
                // Top Empty Spacer to drop down the Thumb based on progress position
                if (progress > 0.0) {
                    CLAY(CLAY_LAYOUT({ 
                        .width = CLAY_SIZING_GROW(0), 
                        .height = CLAY_SIZING_PERCENT(progress * (1.0 - thumb_ratio)) 
                    }));
                }
                
                // The Capsule Scroll Handle
                CLAY(
                    CLAY_LAYOUT({
                        .width = CLAY_SIZING_GROW(0),
                        .height = CLAY_SIZING_PERCENT(thumb_ratio)
                    }),
                    // Translucent grey pill that illuminates on system focus actions
                    CLAY_BACKGROUND_COLOR((Clay_Color){120, 120, 120, 160}),
                    CLAY_CORNER_RADIUS(5), // Perfect capsule geometry match
                    CLAY_ID("AdwScrollbarThumb")
                ) {}
            }
        }
    }
}
```

### 4. Handling SDL3 Inputs via the libadwaita-sdl Layer

To connect your core system events up cleanly, parse mouse-wheel actions through your new scroll structure wrapper inside your main event processing switch block:

```
/**
 * adw_scrolled_window_handle_event:
 * Translates mouse actions to adjustment state alterations natively.
 */
void adw_scrolled_window_handle_event(AdwScrolledWindow *self, SDL_Event *event) {
    if (event->type == SDL_EVENT_MOUSE_WHEEL) {
        AdwAdjustment *adj = self->v_adjustment;
        
        // Reverse direction match for classical cross-platform scroll standards
        double delta = -event->wheel.y * adj->step_increment;
        double target = adj->value + delta;
        
        double max_scroll = adj->upper - adj->page_size;
        if (max_scroll < 0) max_scroll = 0;
        
        if (target < adj->lower) target = adj->lower;
        if (target > max_scroll) target = max_scroll;
        
        if (adj->value != target) {
            adj->value = target;
            Clay_QueueRefresh(); // Notify layout redraw is necessary
        }
    }
}
```

### 5. Tying it to your AdwTerminalView

Now, when initializing your custom terminal pipeline components, link them via the shared `AdwAdjustment` pointer payload:

```
// Creating and binding components together inside your initialization code
AdwTerminalView *terminal = adw_terminal_view_new(80, 24, 1000);

// Instantiate your shared adjustment pointer architecture tracking bounds
AdwAdjustment *shared_adj = adw_adjustment_new(0, 0, 0, 1, 24);

// Connect adjustments to the terminal backend core references
terminal->v_adjust = shared_adj; 
// Inside the push line code, simply update: shared_adj->upper = terminal->scroll_count;

// Instantiate the standalone container widget
AdwScrolledWindow *scrolled_win = adw_scrolled_window_new(shared_adj);

// Pipe terminal canvas draw call into the scrolling window container component
adw_scrolled_window_set_child(scrolled_win, terminal, (AdwWidgetRenderFunc)adw_terminal_view_render_content);
```

Would you like to build out standard structural layout styles next to ensure your container layout maps clean **margins, window headers, and action bars** using **Clay** to perfectly match Libadwaita patterns, or would you like to review how **touch drag gestures** can be bound to the adjustment calculations?

---

## Prompt 3

Can you regenerate libadwaita-sdl.c and libadwaita-sdl.h include all virtual scrollbar rendering, tab, humber menu (3 line bars) and dropdown

## Response 3

To implement a cohesive UI matching GNOME 40+ HIG, we must abstract standard Libadwaita elements—`AdwHeaderBar`, `AdwScrolledWindow`, `AdwViewStack` (for tabs), and popover actions—into a self-contained code footprint. This implementation uses **SDL3** for event processing, custom drawing coordinates, and text measurement, alongside **Clay** for declarative layout assembly.

Here is the structural implementation for your decoupled `libadwaita-sdl` framework.

### 1. Header Declaration (`libadwaita-sdl.h`)

This interface defines your GObject-inspired structural paradigms, wrapping component data wrappers and layout emission hooks.

```
// libadwaita-sdl.h
#pragma once

#include <stdbool.h>
#include <stdlib.h>
#include <clay.h>
#include <sdl3/SDL.h>

// --- Core Adjustments ---
typedef struct _AdwAdjustment {
    double value;
    double lower;
    double upper;
    double step_increment;
    double page_size;
} AdwAdjustment;

AdwAdjustment* adw_adjustment_new(double value, double lower, double upper, double step, double page);

// --- Generic Widget Rendering Callback ---
typedef void (*AdwWidgetRenderFunc)(void *widget_data);

// --- Scrolled Window ---
typedef struct _AdwScrolledWindow {
    AdwAdjustment *v_adjustment;
    bool           v_scrollbar_visible;
    void          *child_data;
    AdwWidgetRenderFunc child_render_callback;
} AdwScrolledWindow;

AdwScrolledWindow* adw_scrolled_window_new(AdwAdjustment *v_adjust);
void adw_scrolled_window_set_child(AdwScrolledWindow *self, void *data, AdwWidgetRenderFunc callback);
void adw_scrolled_window_render(AdwScrolledWindow *self);
void adw_scrolled_window_handle_event(AdwScrolledWindow *self, SDL_Event *event);

// --- Tabs (View Stack / Tab Bar) ---
#define ADW_MAX_TABS 16
typedef struct _AdwTabBar {
    char *titles[ADW_MAX_TABS];
    int   tab_count;
    int   active_tab_idx;
    bool  tab_close_requested;
    int   target_close_idx;
} AdwTabBar;

AdwTabBar* adw_tab_bar_new(void);
void adw_tab_bar_append(AdwTabBar *self, const char *title);
void adw_tab_bar_render(AdwTabBar *self);
void adw_tab_bar_handle_event(AdwTabBar *self, SDL_Event *event);

// --- Dropdown Menu & Hamburger Infrastructure ---
typedef struct _AdwDropdown {
    const char **items;
    int          item_count;
    int          selected_idx;
    bool         is_open;
    Clay_Vector2 last_click_pos; 
} AdwDropdown;

AdwDropdown* adw_dropdown_new(const char **items, int count);
void adw_dropdown_render(AdwDropdown *self, const char *id_string, bool is_hamburger);
void adw_dropdown_handle_event(AdwDropdown *self, SDL_Event *event, const char *id_string);

// --- Header Bar ---
typedef struct _AdwHeaderBar {
    char        *title;
    AdwTabBar   *tab_bar;      // Optional inline tabs (like Epiphany/Console)
    AdwDropdown *hamburger_menu;
} AdwHeaderBar;

AdwHeaderBar* adw_header_bar_new(const char *title);
void adw_header_bar_render(AdwHeaderBar *self);
void adw_header_bar_handle_event(AdwHeaderBar *self, SDL_Event *event);
```

---

### 2. Core Implementation (`libadwaita-sdl.c`)

This implementation contains the complete UI rendering blocks and input processing loop adapters. Emojis and visual paths map directly to standard GNOME symbolic icon styles (e.g., matching the Adwaita icon theme).

```
// libadwaita-sdl.c
#include "libadwaita-sdl.h"
#include <string.h>

// --- Helper Palette Constants (Libadwaita Dark Theme / AdwDark) ---
static const Clay_Color ADW_COLOR_BG =          { 30,  30,  30, 255 }; // Window base
static const Clay_Color ADW_COLOR_HEADER =      { 45,  45,  45, 255 }; // Headerbar fill
static const Clay_Color ADW_COLOR_CARD =        { 54,  54,  54, 255 }; // Card components
static const Clay_Color ADW_COLOR_ACCENT =      { 53, 132, 228, 255 }; // Blue accent (adw_accent)
static const Clay_Color ADW_COLOR_TEXT_MUTED =  { 150, 150, 150, 255 };
static const Clay_Color ADW_COLOR_TEXT_LIGHT =  { 255, 255, 255, 255 };
static const Clay_Color ADW_COLOR_BORDER =      { 20,  20,  20, 255 };

// --- AdwAdjustment Implementation ---
AdwAdjustment* adw_adjustment_new(double value, double lower, double upper, double step, double page) {
    AdwAdjustment *adj = malloc(sizeof(AdwAdjustment));
    adj->value = value;
    adj->lower = lower;
    adj->upper = upper;
    adj->step_increment = step;
    adj->page_size = page;
    return adj;
}

// --- AdwScrolledWindow Implementation ---
AdwScrolledWindow* adw_scrolled_window_new(AdwAdjustment *v_adjust) {
    AdwScrolledWindow *self = malloc(sizeof(AdwScrolledWindow));
    self->v_adjustment = v_adjust ? v_adjust : adw_adjustment_new(0, 0, 0, 1, 10);
    self->v_scrollbar_visible = true;
    self->child_data = NULL;
    self->child_render_callback = NULL;
    return self;
}

void adw_scrolled_window_set_child(AdwScrolledWindow *self, void *data, AdwWidgetRenderFunc callback) {
    self->child_data = data;
    self->child_render_callback = callback;
}

void adw_scrolled_window_render(AdwScrolledWindow *self) {
    AdwAdjustment *adj = self->v_adjustment;
    
    CLAY(
        CLAY_LAYOUT({ .layoutDirection = CLAY_LAYOUT_DIRECTION_HORIZONTAL, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) } }),
        CLAY_ID("AdwScrolledWindow")
    ) {
        // Main view content panel
        CLAY(CLAY_LAYOUT({ .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) } }), CLAY_ID("AdwScrolledViewport")) {
            if (self->child_render_callback && self->child_data) {
                self->child_render_callback(self->child_data);
            }
        }
        
        // GNOME HIG Smooth Track overlay calculation
        if (self->v_scrollbar_visible && adj->upper > adj->page_size) {
            double range = adj->upper - adj->lower;
            double progress = adj->value / (range - adj->page_size);
            if (progress < 0.0) progress = 0.0;
            if (progress > 1.0) progress = 1.0;
            
            double thumb_ratio = adj->page_size / adj->upper;
            if (thumb_ratio < 0.1) thumb_ratio = 0.1;
            
            CLAY(
                CLAY_LAYOUT({ .width = CLAY_SIZING_FIXED(8), .height = CLAY_SIZING_GROW(0), .layoutDirection = CLAY_LAYOUT_DIRECTION_VERTICAL }),
                CLAY_BACKGROUND_COLOR(ADW_COLOR_HEADER),
                CLAY_ID("AdwScrollTrough")
            ) {
                if (progress > 0.0) {
                    CLAY(CLAY_LAYOUT({ .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_PERCENT(progress * (1.0 - thumb_ratio)) }));
                }
                CLAY(
                    CLAY_LAYOUT({ .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_PERCENT(thumb_ratio) }),
                    CLAY_BACKGROUND_COLOR(ADW_COLOR_TEXT_MUTED),
                    CLAY_CORNER_RADIUS(4),
                    CLAY_ID("AdwScrollThumb")
                ) {}
            }
        }
    }
}

void adw_scrolled_window_handle_event(AdwScrolledWindow *self, SDL_Event *event) {
    if (event->type == SDL_EVENT_MOUSE_WHEEL) {
        AdwAdjustment *adj = self->v_adjustment;
        double max_scroll = adj->upper - adj->page_size;
        if (max_scroll < 0) max_scroll = 0;
        
        adj->value += -event->wheel.y * adj->step_increment;
        if (adj->value < adj->lower) adj->value = adj->lower;
        if (adj->value > max_scroll) adj->value = max_scroll;
        
        Clay_QueueRefresh();
    }
}

// --- AdwTabBar Implementation ---
AdwTabBar* adw_tab_bar_new(void) {
    AdwTabBar *self = calloc(1, sizeof(AdwTabBar));
    return self;
}

void adw_tab_bar_append(AdwTabBar *self, const char *title) {
    if (self->tab_count < ADW_MAX_TABS) {
        self->titles[self->tab_count] = strdup(title);
        self->tab_count++;
    }
}

void adw_tab_bar_render(AdwTabBar *self) {
    CLAY(
        CLAY_LAYOUT({ .layoutDirection = CLAY_LAYOUT_DIRECTION_HORIZONTAL, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(36) }, .alignment = { .alignmentVertical = CLAY_ALIGN_VERTICAL_CENTER } }),
        CLAY_BACKGROUND_COLOR(ADW_COLOR_BORDER),
        CLAY_ID("AdwTabBarContainer")
    ) {
        for (int i = 0; i < self->tab_count; i++) {
            bool is_active = (i == self->active_tab_idx);
            
            CLAY(
                CLAY_LAYOUT({ .padding = { .left = 12, .right = 12 }, .height = CLAY_SIZING_GROW(0), .layoutDirection = CLAY_LAYOUT_DIRECTION_HORIZONTAL, .alignment = { .alignmentVertical = CLAY_ALIGN_VERTICAL_CENTER } }),
                CLAY_BACKGROUND_COLOR(is_active ? ADW_COLOR_BG : ADW_COLOR_HEADER),
                CLAY_ID_INDEX("AdwTabItem", i)
            ) {
                CLAY_TEXT(Clay_String_FromCstring(self->titles[i]), CLAY_TEXT_CONFIG({ .textColor = ADW_COLOR_TEXT_LIGHT, .fontSize = 13 }));
                
                // Spacing to small close capsule
                CLAY(CLAY_LAYOUT({ .width = CLAY_SIZING_FIXED(8) }));
                
                CLAY(
                    CLAY_LAYOUT({ .width = CLAY_SIZING_FIXED(14), .height = CLAY_SIZING_FIXED(14), .alignment = { .alignmentHorizontal = CLAY_ALIGN_HORIZONTAL_CENTER, .alignmentVertical = CLAY_ALIGN_VERTICAL_CENTER } }),
                    CLAY_CORNER_RADIUS(7),
                    CLAY_BACKGROUND_COLOR(is_active ? ADW_COLOR_CARD : ADW_COLOR_BORDER),
                    CLAY_ID_INDEX("AdwTabCloseBtn", i)
                ) {
                    CLAY_TEXT(Clay_String_FromCstring("×"), CLAY_TEXT_CONFIG({ .textColor = ADW_COLOR_TEXT_MUTED, .fontSize = 11 }));
                }
            }
            // Vertical split thread border between inactive slots
            CLAY(CLAY_LAYOUT({ .width = CLAY_SIZING_FIXED(1), .height = CLAY_SIZING_GROW(0) }), CLAY_BACKGROUND_COLOR(ADW_COLOR_BORDER));
        }
    }
}

void adw_tab_bar_handle_event(AdwTabBar *self, SDL_Event *event) {
    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && event->button.button == SDL_BUTTON_LEFT) {
        Clay_Vector2 mouse_pos = { event->button.x, event->button.y };
        
        for (int i = 0; i < self->tab_count; i++) {
            if (Clay_PointerOver(Clay_GetElementIdWithIndex(Clay_String_FromCstring("AdwTabCloseBtn"), i))) {
                self->tab_close_requested = true;
                self->target_close_idx = i;
                // Defer freeing structure array updates to application controller scope
                Clay_QueueRefresh();
                return;
            }
            if (Clay_PointerOver(Clay_GetElementIdWithIndex(Clay_String_FromCstring("AdwTabItem"), i))) {
                self->active_tab_idx = i;
                Clay_QueueRefresh();
                return;
            }
        }
    }
}

// --- AdwDropdown & Hamburger Menu Implementation ---
AdwDropdown* adw_dropdown_new(const char **items, int count) {
    AdwDropdown *self = malloc(sizeof(AdwDropdown));
    self->items = items;
    self->item_count = count;
    self->selected_idx = 0;
    self->is_open = false;
    self->last_click_pos = (Clay_Vector2){ 0, 0 };
    return self;
}

void adw_dropdown_render(AdwDropdown *self, const char *id_string, bool is_hamburger) {
    CLAY(
        CLAY_LAYOUT({ .padding = { .left = 10, .right = 10, .top = 6, .bottom = 6 }, .layoutDirection = CLAY_LAYOUT_DIRECTION_HORIZONTAL, .alignment = { .alignmentVertical = CLAY_ALIGN_VERTICAL_CENTER } }),
        CLAY_BACKGROUND_COLOR(ADW_COLOR_CARD),
        CLAY_CORNER_RADIUS(6),
        CLAY_ID(id_string)
    ) {
        if (is_hamburger) {
            // Standard GNOME HIG 3-line bar layout symbol tracking
            CLAY_TEXT(Clay_String_FromCstring("☰"), CLAY_TEXT_CONFIG({ .textColor = ADW_COLOR_TEXT_LIGHT, .fontSize = 14 }));
        } else {
            CLAY_TEXT(Clay_String_FromCstring(self->items[self->selected_idx]), CLAY_TEXT_CONFIG({ .textColor = ADW_COLOR_TEXT_LIGHT, .fontSize = 13 }));
            CLAY(CLAY_LAYOUT({ .width = CLAY_SIZING_FIXED(6) }));
            CLAY_TEXT(Clay_String_FromCstring("▾"), CLAY_TEXT_CONFIG({ .textColor = ADW_COLOR_TEXT_MUTED, .fontSize = 12 }));
        }
    }

    // Floating overlay rendering when open
    if (self->is_open) {
        CLAY(
            CLAY_LAYOUT({ 
                .layoutDirection = CLAY_LAYOUT_DIRECTION_VERTICAL, 
                .sizing = { CLAY_SIZING_FIXED(160), CLAY_SIZING_FIT(0) },
                .padding = { .top = 4, .bottom = 4, .left = 4, .right = 4 },
                // Absolute positions calculating off the base activation trigger context bounds
                .floatingData = {
                    .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
                    .attachment = { .parent = CLAY_ATTACHMENT_POINT_LEFT_BOTTOM, .widget = CLAY_ATTACHMENT_POINT_LEFT_TOP }
                }
            }),
            CLAY_BACKGROUND_COLOR(ADW_COLOR_HEADER),
            CLAY_CORNER_RADIUS(8),
            CLAY_BORDER({ .width = {1,1,1,1}, .color = ADW_COLOR_CARD }),
            CLAY_ID_INDEX("AdwDropdownMenuPopover", 1)
        ) {
            for (int i = 0; i < self->item_count; i++) {
                CLAY(
                    CLAY_LAYOUT({ .padding = { .left = 12, .right = 12, .top = 8, .bottom = 8 }, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) } }),
                    CLAY_BACKGROUND_COLOR((i == self->selected_idx) ? ADW_COLOR_ACCENT : (Clay_Color){0,0,0,0}),
                    CLAY_CORNER_RADIUS(4),
                    CLAY_ID_INDEX("AdwDropdownItem", i)
                ) {
                    CLAY_TEXT(Clay_String_FromCstring(self->items[i]), CLAY_TEXT_CONFIG({ .textColor = ADW_COLOR_TEXT_LIGHT, .fontSize = 13 }));
                }
            }
        }
    }
}

void adw_dropdown_handle_event(AdwDropdown *self, SDL_Event *event, const char *id_string) {
    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && event->button.button == SDL_BUTTON_LEFT) {
        if (Clay_PointerOver(Clay_GetElementId(Clay_String_FromCstring(id_string)))) {
            self->is_open = !self->is_open;
            Clay_QueueRefresh();
            return;
        }

        if (self->is_open) {
            for (int i = 0; i < self->item_count; i++) {
                if (Clay_PointerOver(Clay_GetElementIdWithIndex(Clay_String_FromCstring("AdwDropdownItem"), i))) {
                    self->selected_idx = i;
                    self->is_open = false;
                    Clay_QueueRefresh();
                    return;
                }
            }
            // Dismiss panel if click hits external viewport bounds
            self->is_open = false;
            Clay_QueueRefresh();
        }
    }
}

// --- AdwHeaderBar Implementation ---
AdwHeaderBar* adw_header_bar_new(const char *title) {
    AdwHeaderBar *self = malloc(sizeof(AdwHeaderBar));
    self->title = strdup(title);
    self->tab_bar = NULL;
    self->hamburger_menu = NULL;
    return self;
}

void adw_header_bar_render(AdwHeaderBar *self) {
    CLAY(
        CLAY_LAYOUT({ 
            .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(46) }, 
            .layoutDirection = CLAY_LAYOUT_DIRECTION_HORIZONTAL, 
            .alignment = { .alignmentVertical = CLAY_ALIGN_VERTICAL_CENTER },
            .padding = { .left = 12, .right = 12 }
        }),
        CLAY_BACKGROUND_COLOR(ADW_COLOR_HEADER),
        CLAY_BORDER({ .width = { .bottom = 1 }, .color = ADW_COLOR_BORDER }),
        CLAY_ID("AdwHeaderBar")
    ) {
        // Left side branding or Tab section integration
        if (self->tab_bar) {
            adw_tab_bar_render(self->tab_bar);
        } else {
            CLAY_TEXT(Clay_String_FromCstring(self->title), CLAY_TEXT_CONFIG({ .textColor = ADW_COLOR_TEXT_LIGHT, .fontSize = 15, .fontId = 0 }));
        }

        // Variable elastic spacer shoving utilities rightward
        CLAY(CLAY_LAYOUT({ .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(1) } }));

        // Right hand menu section actions
        if (self->hamburger_menu) {
            adw_dropdown_render(self->hamburger_menu, "AdwHeaderHamburgerBtn", true);
        }
    }
}

void adw_header_bar_handle_event(AdwHeaderBar *self, SDL_Event *event) {
    if (self->tab_bar) {
        adw_tab_bar_handle_event(self->tab_bar, event);
    }
    if (self->hamburger_menu) {
        adw_dropdown_handle_event(self->hamburger_menu, event, "AdwHeaderHamburgerBtn");
    }
}
```

---

### 3. Verification & Usage Pattern

To wire these modules into your standard execution block alongside `AdwTerminalView`, call the event and rendering layouts inside your master loops:

```
// Sample application layout frame loop execution definition
void main_render_loop(AdwHeaderBar *header, AdwScrolledWindow *scroller) {
    Clay_BeginLayout();
    
    CLAY(CLAY_LAYOUT({ .layoutDirection = CLAY_LAYOUT_DIRECTION_VERTICAL, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) } })) {
        // 1. Output the header menu ribbon frame
        adw_header_bar_render(header);
        
        // 2. Render viewport area containing scroller track and cell child canvas
        adw_scrolled_window_render(scroller);
    }
    
    Clay_EndLayout();
}

void main_event_loop(SDL_Event *event, AdwHeaderBar *header, AdwScrolledWindow *scroller) {
    adw_header_bar_handle_event(header, event);
    adw_scrolled_window_handle_event(scroller, event);
}
```

Would you like to build out the event propagation logic to handle **window resizing** cleanly across the text margins, or should we design the **Clay click-and-drag interactions** for the scrollbar thumb?
