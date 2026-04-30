# Lume Media Player

![Lume Media Player Logo](Pictures/logo.png)

A lightweight **Lume** media player built with **Qt Widgets**.

It is designed with memory safety and practical use in mind, while still keeping a clean interface, keyboard shortcuts, playlist handling, file opening support, theme loading, and media playback features.

## Preview

![App preview 1](Pictures/image1.png)
![App preview 2](Pictures/image2.png)

## Memory Usage

![Idle memory usage](Pictures/image3.png)
![Memory usage while watching](Pictures/memory.png)

This player stays around **100 MB when idle** and around **230 MB at max** in normal use, which is solid for a media player with a full widget-based interface.

## Features

- Built with **Qt Widgets**
- Media opening by file argument
- File manager support through `.desktop`
- Keyboard shortcuts
- Playlist drawer
- Playback controls
- Theme support through `~/.lumeconf/theme.json`
- Custom overlay UI
- Memory-conscious layout and media handling

## Build

```bash
qmake && make -j$(nproc)
```

## Desktop entry

Use the included `.desktop` file so file managers and desktop environments can open media files directly with the player.

## Theme

The app looks for:

```bash
~/.lumeconf/theme.json
```

If the file exists, it uses it. If not, it falls back to built-in defaults.

### Create the theme file

```bash
#!/usr/bin/env bash
set -e

theme_dir="$HOME/.lumeconf"
theme_file="$theme_dir/theme.json"

mkdir -p "$theme_dir"

if [ -f "$theme_file" ]; then
    exit 0
fi

cat > "$theme_file" <<'EOF'
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
```

Save that as a shell script if you want to generate the theme file automatically.

## API

Launch with a media file or folder path:

```bash
media /path/to/video.mp4
media /path/to/folder
```

The player will open the argument directly and load media from it.

## Keyboard shortcuts

- `Space` — play / pause
- `Left` / `Right` — seek backward / forward
- `Alt + Left` — previous file
- `Alt + Right` — next file
- `Up` / `Down` — volume control
- `F` / `F11` — fullscreen
- `Esc` — exit fullscreen

## Notes

The player is meant to stay simple, usable, and efficient while still feeling polished.
