---
title: "How to install SDL3 and SDL3_ttf on Mac from source code"
date: 2026-09-06T20:32:18
category: pc-and-linux-history
related: [apple-mac]
sources: [N]
original_query: "How to install SDL3 and SDL3_ttf on Mac from source code"
---

# How to install SDL3 and SDL3_ttf on Mac from source code

- **Date:** Sep 6, 2026, 8:32:18 PM EDT
- **Category:** `pc-and-linux-history`
- **Related:** `apple-mac`
- **Sources:** 2026-10-10 export
- **Original query:** How to install SDL3 and SDL3_ttf on Mac from source code

---

## Prompt

How to install SDL3 and SDL3_ttf on Mac from source code

## Response

To install **SDL3** and **SDL3\_ttf** on macOS from source code, you can use **CMake**, which is the official and recommended build system.

### Prerequisites

Before compilation, make sure you have the command-line developer tools and CMake installed. You can easily set this up via [Homebrew](https://brew.sh/):

```
xcode-select --install
brew install cmake pkg-config
```

---

### Step 1: Install SDL3 Core

You need to clone, configure, build, and system-install the core repository.

```
# Clone the main repository
git clone https://github.com/libsdl-org/SDL.git
cd SDL

# Create and navigate to a build directory
mkdir build && cd build

# Configure the build targeting macOS
cmake .. -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13

# Build the binaries
cmake --build .

# Install the library to your system (requires root privileges)
sudo cmake --install .
```

*Note: If you are on Apple Silicon and want to build a universal binary for both ARM64 and Intel architectures, replace the `cmake ..` command with:*

```
cmake .. "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13
```

---

### Step 2: Install SDL3\_ttf

The extension library `SDL3_ttf` relies on the core library installed in the previous step.

```
# Navigate back to your working directory and clone SDL3_ttf
cd ../..
git clone https://github.com/libsdl-org/SDL_ttf.git
cd SDL_ttf

# Create and navigate to a build directory
mkdir build && cd build

# Configure the build
cmake .. -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13

# Build the binaries
cmake --build .

# Install the library to your system
sudo cmake --install .
```

---

### Step 3: Verify the Installation

By default, the source installation places files under `/usr/local/include/SDL3/` and `/usr/local/lib/`. You can quickly verify that `pkg-config` can locate your source-built installations:

```
pkg-config --cflags --libs sdl3 sdl3-ttf
```

If successful, this will output the flags needed to compile your programs (e.g., `-I/usr/local/include/SDL3 -D_THREAD_SAFE -L/usr/local/lib -lSDL3 -lSDL3_ttf`).

Would you like help setting up a basic **C++ Makefile** or configuring a **CMakeLists.txt** project to link against your new SDL3 source build?
