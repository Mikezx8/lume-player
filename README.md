# Lume Media Player

<p align="center">
  <img src="Pictures/logo.png" alt="Lume Logo" width="150">
</p>

<p align="center">
  <b>Lume Media Player</b> is a Qt Widgets media player for the Lume environment.
</p>

---

## Overview

Lume Media Player is built with Qt Widgets and focuses on **controlled memory usage** instead of bloated abstraction layers.

- ~**100 MB RAM when idle**
- ~**230 MB during playback (peak)**

This is lower than many media players that sit around **300–400 MB+** under the same workload.

---

## Interface

<p align="center">
  <img src="Pictures/image1.png" width="420">
  <img src="Pictures/image2.png" width="420">
</p>

Clean Qt Widgets interface with:
- playback controls
- playlist drawer
- keyboard shortcuts
- fullscreen support

---

## Memory Behavior (Explained)

### Idle State (~100 MB)

<p align="center">
  <img src="Pictures/image3.png" width="500">
</p>

When the player is open but not playing anything, memory stays around **100 MB**.

This is because:
- no decoding pipelines are active
- no frame buffers are allocated
- Qt Widgets UI stays lightweight (no heavy scene graph or GPU layers)
- only the player core and UI are resident in memory

So what you're seeing here is basically the **base footprint of the app**, not inflated by media processing.

---

### Playback State (~230 MB)

<p align="center">
  <img src="Pictures/memory.png" width="500">
</p>

When media starts playing, memory rises to around **230 MB**.

That increase comes from:
- **decoder buffers** (video/audio streams)
- **frame storage** (multiple frames buffered for smooth playback)
- **Qt multimedia backend allocations**
- **resolution-dependent usage** (higher resolution → more memory)

The key point is:
- memory increases only when needed (during playback)
- it does not spike uncontrollably
- it stays within a predictable range

Compared to many players that:
- preload aggressively
- stack GPU buffers heavily
- or use heavier UI frameworks

this player keeps usage **tight and proportional to actual workload**.

---

## Features

- Qt Widgets interface
- Keyboard shortcuts
- Playlist support
- File and argument-based media opening
- Theme support from `~/.lumeconf/theme.json`
- Memory-conscious design
- Built for the Lume desktop environment

---

## Build

    qmake && make -j$(nproc)

## Usage

Open a media file directly:

    media /path/to/video.mp4

Open a folder:

    media /path/to/media-folder

## Theme File

Lume Media Player looks for:

    ~/.lumeconf/theme.json

Theme example:

    {
        "bg-primary":     "#1a0a0a #130942 45deg",
        "bg-secondary":   "#120505",
        "bg-tertiary":    "#1f0a0a",
        "bg-hover":       "#2a0f0f",
        "text-primary":   "#ffe4e4",
        "text-secondary": "#d4a5a5",
        "text-muted":     "#704040",
        "accent-purple":  "#ff6b6b",
        "accent-blue":    "#ff4757",
        "accent-cyan":    "#ff6348",
        "accent-green":   "#ff5252",
        "accent-red":     "#ff3838",
        "accent-yellow":  "#ff9f43",
        "border-color":   "#2a0f0f",
        "selection-bg":   "rgba(255, 56, 56, 0.15)",

        "border":               "on",
        "outline-glow":         "on",
        "border-pulse-effect":  "on",
        "rounding-corners":     "on",
        "border-thickness":     "2",
        "border-opacity":       "100",
        "background-opacity":   "10",

        "general-opacity":      "100",
        "onwindow-opacity":     "100",
        "outwindow-opacity":    "85",
        "title-opacity":        "100",
        "notification-opacity": "10",
        "toolbar-opacity":      "100",
        "text-follows-opacity": "on",

        "windows-blur":         "on",
        "blur-intensity":       "40",

        "onwindow-outline":     "on",
        "offwindow-outline":    "off",
        "fullscreen-corners":   "0",
        "minimized-corners":    "8",
        "taskbar-border":       "on",
        "quicksettings":        "right",
        "notification-position": "bottom,right",
        "toast-position":       "bottom,center",
        "taskbar-center":       "bottom",
        "taskbar-layer":        "fixed",
        "clock-font":           "DejaVu Sans Mono",
        "notification-sound":   "path/to/sound",

        "taskbar-position":     "bottom"
    }

## Create `theme.json` if missing

    #!/usr/bin/env bash
    set -e

    THEME_DIR="$HOME/.lumeconf"
    THEME_FILE="$THEME_DIR/theme.json"

    mkdir -p "$THEME_DIR"

    if [ ! -f "$THEME_FILE" ]; then
        cat > "$THEME_FILE" <<'EOF'
    {
        "bg-primary":     "#1a0a0a #130942 45deg",
        "bg-secondary":   "#120505",
        "bg-tertiary":    "#1f0a0a",
        "bg-hover":       "#2a0f0f",
        "text-primary":   "#ffe4e4",
        "text-secondary": "#d4a5a5",
        "text-muted":     "#704040",
        "accent-purple":  "#ff6b6b",
        "accent-blue":    "#ff4757",
        "accent-cyan":    "#ff6348",
        "accent-green":   "#ff5252",
        "accent-red":     "#ff3838",
        "accent-yellow":  "#ff9f43",
        "border-color":   "#2a0f0f",
        "selection-bg":   "rgba(255, 56, 56, 0.15)",

        "border":               "on",
        "outline-glow":         "on",
        "border-pulse-effect":  "on",
        "rounding-corners":     "on",
        "border-thickness":     "2",
        "border-opacity":       "100",
        "background-opacity":   "10",

        "general-opacity":      "100",
        "onwindow-opacity":     "100",
        "outwindow-opacity":    "85",
        "title-opacity":        "100",
        "notification-opacity": "10",
        "toolbar-opacity":      "100",
        "text-follows-opacity": "on",

        "windows-blur":         "on",
        "blur-intensity":       "40",

        "onwindow-outline":     "on",
        "offwindow-outline":    "off",
        "fullscreen-corners":   "0",
        "minimized-corners":    "8",
        "taskbar-border":       "on",
        "quicksettings":        "right",
        "notification-position": "bottom,right",
        "toast-position":       "bottom,center",
        "taskbar-center":       "bottom",
        "taskbar-layer":        "fixed",
        "clock-font":           "DejaVu Sans Mono",
        "notification-sound":   "path/to/sound",

        "taskbar-position":     "bottom"
    }
    EOF
    fi

## Keyboard Shortcuts

The player includes useful shortcuts for playback and navigation.

## License

Lume Media Player is part of the Lume app ecosystem.
