---
title: "can you explain open source widgets or UI toolkit of wxWidgets how to draw a simple button or checkbox or tadio buttons on each platform for Windows, macOS and Linux pure wayland backend"
date: 2026-08-22T15:56:27
category: pc-and-linux-history
related: [windows, apple-mac]
sources: [N]
original_query: "can you explain open source widgets or UI toolkit of wxWidgets how to draw a simple button or checkbox or tadio buttons on each platform for Windows, macOS and Linux pure wayland backend"
---

# can you explain open source widgets or UI toolkit of wxWidgets how to draw a simple button or checkbox or tadio buttons on each platform for Windows, macOS and Linux pure wayland backend

- **Date:** Aug 22, 2026, 3:56:27 PM EDT
- **Category:** `pc-and-linux-history`
- **Related:** `windows`, `apple-mac`
- **Sources:** 2026-10-10 export
- **Original query:** can you explain open source widgets or UI toolkit of wxWidgets how to draw a simple button or checkbox or tadio buttons on each platform for Windows, macOS and Linux pure wayland backend

---

## Prompt 1

can you explain open source widgets or UI toolkit of wxWidgets how to draw a simple button or checkbox or tadio buttons on each platform for Windows, macOS and Linux pure wayland backend

## Response 1

**wxWidgets** is a mature, open-source C++ GUI toolkit that uses **native system components (widgets)** on each platform rather than drawing custom controls from scratch. This means when you create a button or checkbox in wxWidgets, the library automatically maps it to a Win32 control on Windows, a Cocoa `NSView` on macOS, and a GTK4 or GTK3 widget on Linux (including a pure Wayland backend via `wxGTK`).

Because wxWidgets abstracts these details, you do not write platform-specific code. The exact same C++ lines generate native controls everywhere.

### Platform-Specific Backends Behind the Scenes

While your C++ code remains identical, wxWidgets maps your elements to different native rendering technologies on each platform:

| Control | Windows (`wxMSW`) | macOS (`wxOSX/Cocoa`) | Linux Pure Wayland (`wxGTK`) |
| --- | --- | --- | --- |
| **Button** | Native Win32 `BUTTON` class | Native `NSButton` | `GtkButton` |
| **Checkbox** | Win32 `BUTTON` with `BS_CHECKBOX` | Native `NSButton` (switch type) | `GtkCheckButton` |
| **Radio Button** | Win32 `BUTTON` with `BS_RADIOBUTTON` | Native `NSButton` (radio type) | `GtkCheckButton` (grouped) |
| **Display Server** | Desktop Window Manager (DWM) | Quartz Compositor / Metal | Wayland Compositor (via GTK backend) |

---

### C++ Code: Drawing the Controls the "Pure" Way

To compile this natively on Windows, macOS, or Linux (Wayland), use the following standard wxWidgets structure.

```
#include <wx/wx.h>

class MyFrame : public wxFrame {
public:
    MyFrame() : wxFrame(nullptr, wxID_ANY, "Native Controls Example", wxDefaultPosition, wxSize(300, 250)) {
        
        // Create a parent panel for proper tab navigation and native background rendering
        wxPanel* panel = new wxPanel(this, wxID_ANY);

        // Use a vertical box sizer for automatic, native DPI-aware layout scaling
        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

        // 1. Simple Push Button
        wxButton* button = new wxButton(panel, wxID_ANY, "Click Me");
        
        // 2. Checkbox
        wxCheckBox* checkbox = new wxCheckBox(panel, wxID_ANY, "Enable Feature");

        // 3. Radio Buttons (Grouped)
        // wxRB_GROUP starts a new mutually exclusive group
        wxRadioButton* radio1 = new wxRadioButton(panel, wxID_ANY, "Option A", wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
        wxRadioButton* radio2 = new wxRadioButton(panel, wxID_ANY, "Option B");

        // Add controls to sizer with 10px padding for clean cross-platform spacing
        sizer->Add(button, 0, wxALL, 10);
        sizer->Add(checkbox, 0, wxALL, 10);
        sizer->Add(radio1, 0, wxALL, 10);
        sizer->Add(radio2, 0, wxALL, 10);

        panel->SetSizer(sizer);
    }
};

class MyApp : public wxApp {
public:
    virtual bool OnInit() {
        MyFrame* frame = new MyFrame();
        frame->Show(true);
        return true;
    }
};

// Generates the platform-specific main() or WinMain() entry point automatically
wxIMPLEMENT_APP(MyApp);
```

---

### Core Concepts for "Pure" Native Rendering

To ensure your application targets each native backend flawlessly without glitches, follow these rules:

- **Always use a `wxPanel` parent**: Do not place controls directly onto a `wxFrame`. Windows requires a panel for native keyboard tab navigation, while macOS and Wayland rely on it for correct background contrast themes (like Dark Mode support).
- **Use Sizers over absolute positioning**: Fixed coordinates (`x, y`) break cross-platform styling. A Linux Wayland environment running GNOME might require significantly larger padding around a `GtkCheckButton` than a Windows 11 system requires for a Win32 checkbox. Sizers query the native OS for these exact dimension requirements at runtime.
- **Wayland Specifics**: To force your compiled binary to use the pure Wayland backend instead of fallback XWayland on Linux, ensure your environment variables are set before execution (`export GDK_BACKEND=wayland`).

If you want to move forward with building this, tell me:

- What **operating system** are you currently writing code on?
- Which **build tool** (CMake, Makefiles, or Visual Studio) do you prefer using to compile the binary?

I can give you the exact terminal commands or configuration files needed to compile this code.

---

## Prompt 2

No then, if my Linux doesn’t have GTK3 or GTK4 then how wxWidgets draw button? unlike Tk(a part of Tcl) there is no UI toolkit drawing feature?

## Response 2

If a Linux environment completely lacks GTK3 or GTK4 libraries, **a standard `wxGTK` binary will fail to run entirely**, throwing a "shared library not found" error.

However, contrary to popular belief, **wxWidgets *does* have its own UI toolkit drawing engine**. It is called **`wxUniversal`**.

When wxWidgets is compiled with `wxUniversal`, it operates exactly like Tk, Qt, or Flutter—it bypasses native OS widgets completely and draws buttons, checkboxes, and radio buttons using manual pixel-drawing commands.

---

### How wxWidgets Draws UI Without GTK

If GTK is absent, wxWidgets shifts from an "abstraction layer" to a "rendering engine" via two alternative backends:

#### 1. The `wxX11` Backend (Via `wxUniversal`)

If the system uses an X11 server but lacks a high-level toolkit like GTK, wxWidgets can be compiled as a **`wxX11`** build.

- **The Drawing Process:** wxWidgets communicates directly with the low-level `libX11` library to open a blank window canvas.
- **The Toolkit Element:** The `wxUniversal` sub-library takes over. It handles mouse/keyboard events and uses basic vector graphics (`wxDC` / Device Context) to manually draw the borders, shading, text, and checkmarks for every button or checkbox directly onto the X11 surface.

#### 2. The `wxQt` Backend

wxWidgets also features a **`wxQt`** port. If the Linux machine lacks GTK but has Qt installed, wxWidgets acts as a wrapper around Qt. In this scenario, `wxButton` maps to `QPushButton`, and Qt's internal style engine handles the actual pixel rendering.

---

### Key Differences: wxWidgets (`wxUniversal`) vs. Tk

While both can draw their own UI elements from scratch, they handle the process differently:

| Feature | Tk (Tcl) | wxWidgets (`wxUniversal`) |
| --- | --- | --- |
| **Default Mode** | **Self-drawn** by default. It emulates the native look using its own rendering engine. | **Native wrappers** by default. It only draws its own widgets if explicitly compiled via `wxUniversal`. |
| **Drawing Engine** | Uses a custom cross-platform engine (`Tk_DrawFocusHighlight`, Xlib primitives). | Uses the core **`wxGraphicsContext`** and standard paint events (`wxPaintEvent`) to draw pixels. |
| **Theme System** | Uses `ttk` (Themed Tk) for modern engines or falls back to retro, flat styling. | Uses pluggable **Renderers** (`wxRendererNative`) to swap between a Win32 look, GTK look, or standard generic theme. |

---

### The Catch with Pure Wayland

`wxUniversal` relies explicitly on X11 protocols (`wxX11`). Because a **pure Wayland** environment completely removes the X11 subsystem, **wxWidgets cannot draw its own UI on pure Wayland without GTK**.

To run a wxWidgets application on a pure Wayland Linux machine without any GTK packages, the environment requires **XWayland** (the X11 compatibility layer for Wayland). This configuration allows a `wxX11` + `wxUniversal` binary to render its custom-drawn buttons successfully.

If you are setting up a minimal or custom Linux environment, let me know:

- Is this for an **embedded system**, a minimal **distro (like Alpine)**, or a custom desktop?
- Does your environment include the **XWayland** compatibility layer?

I can provide the configuration flags to compile wxWidgets with the self-drawing `wxUniversal` engine.

---

## Prompt 3

ok that’s fine can you show me wxUniversal looks and feel image URL

## Response 3

When compiled using **`wxUniversal`**, wxWidgets bypasses the underlying operating system's themes. It draws all window decorations, title bars, buttons, checkboxes, and tabs manually using internal pixel-drawing rules.

By default, its default style mimics a retro, late-1990s to early-2000s desktop environment (similar to Windows Classic or older X11 window managers).

### Visual Examples of wxUniversal

Because `wxUniversal` must draw every single component manually, it includes built-in themes to draw standard widgets, window frames, and alert boxes from scratch:

### Visual Characteristics of the Self-Drawn Widgets

- **Buttons & Controls**: Buttons feature a rigid, highly geometric look with stark pixelated drop-shadow borders to simulate depth when clicked.
- **Checkboxes & Radios**: Rendered as simple white squares and circles that display a clean, hard-coded "X" or inner dot when selected.
- **Window Decorations**: If a window manager is absent, `wxUniversal` draws its own title bars, complete with retro minimize, maximize, and exit controls.

Would you like the **compilation flags** required to build wxWidgets into this custom-drawn `wxUniversal` mode, or do you need help **setting up a fallback rendering engine** for your Linux system?

---

## Prompt 4

wxUniversal works on Cocoa as 64bit library?

## Response 4

**No, `wxUniversal` does not work on Cocoa.** It is fundamentally incompatible with the way macOS handles 64-bit applications.

There are two major technical reasons why you cannot run a 64-bit `wxUniversal` app natively on macOS:

### 1. Architectural Architecture Clash

- **`wxUniversal`** is a widget library designed exclusively for *low-level, direct-to-pixel* drawing platforms like **wxX11** or **wxMSW**. It requires a backend that allows it to manually paint over a blank canvas window.
- **Cocoa** (the macOS 64-bit API framework) does not expose standard blank canvas window structures to third-party software in a way that allows a foreign library to draw core UI elements like buttons from scratch. Instead, Cocoa forces programs to inherit from Apple's native `NSView` object model.

Because of this, the core macOS port of wxWidgets (**`wxOSX/Cocoa`**) completely bypasses `wxUniversal`. It maps commands directly to native, 64-bit AppKit primitives (like `NSButton`) to ensure proper Metal/Quartz compositing.

### 2. Confusing `wxUniversal` with Apple "Universal Binaries"

Developers often confuse the two because of similar naming conventions on macOS:

- **`wxUniversal`**: The specialized sub-toolkit that draws retro-looking, custom UI pixels from scratch on bare-metal systems (like Unix X11 or custom embedded architectures). **(Not used or supported on modern macOS)**.
- **Apple Universal Binaries (`--enable-universal_binary`)**: A compilation flag used in Xcode or terminal `configure` chains. This creates a single executable containing separate binary slices for multiple hardware architectures (such as combined 64-bit Intel `x86_64` and 64-bit Apple Silicon `arm64`).

If you configure wxWidgets on macOS with the architecture flag `--enable-universal_binary`, it compiles standard, native **`wxOSX/Cocoa`** widgets that natively run on 64-bit Mac hardware. It will look exactly like a modern Apple app, not a retro custom-drawn UI.

Would you like help setting up a configuration script to properly compile a **64-bit Mac Universal Binary** using the standard Cocoa layout?

---

## Prompt 5

No I don’t get confused, so wxUniversal works Win32 or X11?

## Response 5

Yes, **`wxUniversal` works on both Win32 (`wxMSW`) and X11 (`wxX11`)**.

When you build wxWidgets in `wxUniversal` mode on these platforms, it overrides the host operating system's default button, checkbox, and window rendering styles. Instead, it forces the application to look like the custom-drawn, retro UI shown in the previous screenshots.

### How it operates on each platform

#### 1. Win32 Platform (`wxMSW` + `wxUniversal`)

- **What it does:** Instead of using the native Windows API (`CreateWindowEx` with the `BUTTON` class), wxWidgets registers a blank, generic window class.
- **The Result:** It captures standard Windows mouse and keyboard message loops, but manually paints the pixels for buttons, tabs, and scrollbars inside that window canvas. It runs as a standard Win32 `.exe` file but completely bypasses modern Windows 10/11 visual styles.

#### 2. X11 Platform (`wxX11` + `wxUniversal`)

- **What it does:** This is the most common use case for `wxUniversal`. It communicates directly with low-level `libX11` protocols on Linux/Unix systems.
- **The Result:** Because raw X11 does not have its own buttons or checkboxes, `wxUniversal` provides the entire UI toolkit engine from scratch. It draws the visual components directly onto the raw X11 window surfaces.

---

### Comparison of Backend Types

| Target Backend | Does `wxUniversal` Work? | What Actually Renders the Pixels? |
| --- | --- | --- |
| **Win32 (`wxMSW`)** | **Yes** (Optional Build) | wxWidgets internal drawing code via Windows GDI/GDI+ |
| **X11 (`wxX11`)** | **Yes** (Standard Configuration) | wxWidgets internal drawing code via X11 drawing primitives |
| **Wayland (`wxGTK`)** | **No** | GTK3 or GTK4 libraries via Wayland compositor protocol |
| **macOS (`wxOSX`)** | **No** | Core Cocoa / AppKit framework via Quartz & Metal |

If you want to test this out yourself, are you looking to compile the `wxUniversal` build using **Visual Studio on Windows** or via a **Linux terminal utilizing X11**?

---

## Prompt 6

is there libX11 or xcb library convert to wayland protocol plus EGL rendering something?

## Response 6

**Yes.** There are two completely different ways to translate legacy `libX11` or `xcb` calls into native Wayland protocols using hardware-accelerated **EGL rendering**. One is the traditional industry standard, and the other is a cutting-edge in-process translation library.

---

### 1. The Industry Standard: `Xwayland` + `GLAMOR`

The most common way this happens is through **Xwayland**. It acts as a bridge between old X11 apps and modern Wayland compositors.

- **How it handles `libX11`/`xcb`:** Xwayland runs a real, headless X Server inside your Wayland session. When an old app speaks `libX11` or `xcb` protocol codes, Xwayland catches them.
- **The EGL Rendering Part (`GLAMOR`):** Instead of using old, slow CPU rendering, Xwayland utilizes an internal layer called **GLAMOR**. GLAMOR translates 2D X11 drawing commands directly into modern 3D OpenGL ES instructions.
- **The Wayland Output:** These OpenGL commands are bound to a native **Wayland-EGL surface (`wl_egl_window`)**. The final rendered pixels are shared directly with the main Wayland compositor (like Sway or Mutter) via hardware-accelerated EGL buffers.

---

### 2. The Pure Library Approach: `libx11-compat` (Atop SDL)

If you want to entirely bypass running an external heavy Xwayland background server process, a highly innovative translation project exists called **`libx11-compat`**.

- **How it handles `libX11`/`xcb`:** Instead of acting like a network proxy server, `libx11-compat` is a standalone, binary-compatible `.so` library that safely intercepts and replaces your system's native `libX11.so`.
- **The EGL Rendering Part:** It maps legacy Xlib window states, mouse/keyboard inputs, and contexts internally into an **SDL3 / SDL2 container**.
- **The Wayland Output:** Because SDL natively supports pure Wayland and hardware-accelerated EGL contexts, the legacy application instantly renders its window as a **native Wayland client with direct EGL support**—without modifications to the source code.

---

### Summary Architectural Pipeline

Depending on which path your system takes, the data flows like this to reach your GPU driver:

```
[Legacy App Binary] 
       │
       ▼ (Speaks LibX11 / XCB)
 ─────────────────────────────────────────────────────────────────
  OPTION A: Xwayland Server          │ OPTION B: libx11-compat
 ─────────────────────────────────────────────────────────────────
  [Xwayland Protocol Listener]       │ [Intercepts LibX11 .so API]
       │                             │            │
       ▼ (Converts via GLAMOR)       │            ▼
  [OpenGL / OpenGLES Instructions]   │ [Maps directly to SDL Context]
 ─────────────────────────────────────────────────────────────────
       │                                          │
       └───────────────────►   ◄──────────────────┘
                             │
                             ▼
                 [Wayland-EGL Buffer Wrapper]
                             │
                             ▼
                [Native Wayland Compositor]
                             │
                             ▼
                    [Kernel DRM / EGL / GPU]
```

Are you looking to use these technologies to **run a pre-compiled retro application** smoothly on a minimal Wayland desktop, or are you trying to **manually write a graphics wrapper** in pure C/C++?

---

## Prompt 7

where is libx11-compat project upstream repository?

## Response 7

The upstream repository for the **libx11-compat** project is hosted on GitHub at **[sysprog21/libx11-compat](https://github.com/sysprog21/libx11-compat)**.

### Project Fast Facts

- **Primary Author:** Maintained by developer Ching-Chun (Jim) Huang under the sysprog21 research group.
- **Core Technology:** It provides a lightweight, in-process replacement for standard `libX11` by routing calls directly through **SDL2/SDL3**, **SDL\_ttf**, and **Pixman**.
- **Target Goal:** Bypasses the need for heavy external background X11 emulation layers entirely, enabling legacy apps (such as classic Qt2 tools, XMMS, or XCircuit) to load natively inside bare-metal Wayland or Android graphics environments.
- **Origins:** It is a significantly modernized, highly active fork of the older, unmaintained [SDL2X11Emulation repository](https://github.com/Abestanis/SDL2X11Emulation).

If you are looking to get this running locally, I can help write a quick bash snippet to **clone the repo and compile it** with your active system dependencies. Would that be helpful?
