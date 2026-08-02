# catclock-sdl3

A modernized, high-performance Kit-Cat Clock desktop widget. This project ports the [classic X11 `catclock` application](https://github.com/barkythedog/catclock) to the modern SDL3 framework and Sokol GFX pipeline. It combines vintage pixel-art aesthetics with a low-overhead, multi-platform runtime architecture.

---

## 🚀 Engineering Features

*   **SDL3 Native Presenter**: Full desktop compositing support featuring borderless transparent windows and OS-level hit-testing for dragging.
*   **Sokol GFX Pipeline**: Completely replaces legacy immediate-mode drawing commands with hardware-accelerated offscreen shader passes and unified vertex staging.
*   **Low-CPU Texture Atlas Engine**: Cuts overhead by pre-baking high-DPI specialized texture atlases for clock hands, eyes, and tail animations.
*   **Sharp Scaling**: High-efficiency, nearest-neighbor GPU integer scaling locks texture sheets cleanly into exact viewport aspect constraints, preserving sharp pixel edges.

---

## 🎛️ Usage

Run the executable from the command line using the following syntax:

```bash
./catclock-sdl3 [flags]
```

### Available Parameter Flags

| Flag | Description | Default |
| :--- | :--- | :--- |
| `--help` | Displays the parameter map documentation layout. | N/A |
| `--notop` | Disables forcing the widget to stay pinned on the "Always on Top" window layer. | False |
| `--decorations` | Restores standard desktop borders and title bars with a zero horizontal axis asset alignment baseline. | False |
| `--decorationscolor [hex]` | Overrides the main window background clear color slot when `--decorations` borders are drawn (supports Alpha transparency channels). | ffffffff |
| `--fps [1-120]` | Sets custom target frame rate pacing limit constraints. | 30 |
| `--scale [0.5-10.0]` | Sets the initial window integer sizing step multiplier factor layout metrics. | 1.0 |
| `--catcolor [hex]` | Overrides the default black cat body base layout canvas channels. | 000000ff |
| `--detailcolor [hex]` | Overrides default white accents and static foreground detail channels. | ffffffff |
| `--tiecolor [hex]` | Overrides default necktie hex color fill payload. | ffffffff |
| `--pupilcolor [hex]` | Overrides tracking eye pupil hex color payload. | 000000ff |
| `--scleracolor [hex]` | Overrides the static eye backing socket background grid block. | ffffffff |
| `--hourscolor [hex]` | Overrides default hour clock hand vector line layout. | 000000ff |
| `--minutescolor [hex]` | Overrides default minute clock hand vector line layout. | 000000ff |
| `--secondscolor [hex]` | Overrides default sweeping seconds clock hand vector line layout. | ff0000ff |
| `--outlinecolor [hex]` | Overrides the 1px form-fitting outline outer boundary rim edge color profile. | ffffffff |

---

### Interactive Runtime Controls

*   **Press `+` or `=`**: Scale window widget up by `0.5x` steps and trigger automated offscreen VRAM texture re-allocations.
*   **Press `-`**: Scale window widget down by `0.5x` steps safely.
*   **Mouse Wheel**: Scroll up to zoom/scale in, scroll down to zoom/scale out dynamically.
*   **Press `Escape` or `Q`**: Safely terminates the main execution loop and releases GPU frame resources cleanly.
*   **Left Click & Drag**: Freely moves the borderless transparent clock app container layout coordinates seamlessly across the desktop workspace desktop bounds.

---

## 🐧 Linux / GNOME (Wayland) Note

Due to security constraints in native Wayland desktop environments, applications cannot programmatically force themselves to stay pinned as "Always on Top".

If you want the clock window permanently floating above other application windows in GNOME:
1. Click the clock window to focus it.
2. Press **`Alt` + `Space`** to open the native window manager utility menu.
3. Select **`Always on Top`**.

---

## 📜 Attribution & History

*   **The Original**: Inspired by the classic X11/Motif `catclock` desktop widget program.
*   **Asset Origins**: 1-bit XBM graphic assets are derived from the historical open-source X11 layout repository.
*   **Modernization Refactoring**: Engineered in a collaborative partnership between the User and Google Gemini AI to structure pre-baked texture atlases, transition layout loops down to Sokol GFX passes, and achieve production-grade low-overhead graphics rendering.

---

## 📄 License

Open Source / Educational. Please preserve all original authorship attributions upon redistributing or modifying any structural source components.
