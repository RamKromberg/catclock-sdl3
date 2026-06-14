# catclock-sdl3

A modernized, high-performance Kit-Cat Clock desktop widget. This project ports the [classic X11 `catclock` application](https://github.com/barkythedog/catclock) to the modern SDL3 framework. It combines vintage pixel-art aesthetics with a low-overhead, multi-platform runtime architecture.

---

## 🚀 Engineering Features

*   **SDL3 Native Presenter**: Full desktop compositing support featuring borderless transparent windows and OS-level hit-testing for dragging.
*   **Low-CPU Texture Atlas Engine**: Drastically cuts overhead by pre-baking high-DPI specialized texture atlases for clock hands, eyes, and tail animations.
*   **Sharp Scaling**: High-efficiency, 1-bit binary threshold rendering to optimize alpha blending with custom color overlays, paired with sharp nearest-neighbor GPU integer scaling.
*   **Mathematical Pupil Shader**: Stateless GPU-accelerated triangle fan geometry meshes render perfectly centered moving eyes with zero scanline gaps.
*   **Jitter-Free Pacing**: Eliminates frame stuttering by synchronizing monotonic system ticks smoothly with the hardware wall clock.
*   **Power Efficient**: Focus-aware adaptive pacing, focus kernel sleeps, and zero-VSync spinlocks minimize CPU wakeups.

---

## 🔧 Installation & Building

The build system relies on standard GNU development tools and requires the **SDL3** library.

### Native Linux Build
```bash
make
```

### Windows Cross-Compilation (via Nix Shell)
```bash
make windows
```
*Outputs a fully standalone `catclock-sdl3.exe` and bundles the required `SDL3.dll` asset with cryptographic checksum validation.*

---

## 💻 Usage

Run the executable from the command line using the following syntax:

```bash
./catclock-sdl3 [flags]
```

### Available Parameter Flags

| Flag | Description | Default |
| :--- | :--- | :--- |
| `--help` | Displays the parameters map documentation layout. | N/A |
| `-noscalesteps` / `-noseconds` | Completely hides the sweeping red second hand layer. | False |
| `-nooutline` | Disables the form-fitting white halo drop-shadow background. | False |
| `-notop` | Disables forcing the widget to stay pinned on the "Always on Top" window layer. | False |
| `-decorations [hex]` | Restores standard desktop borders & window title-bars with optional background hex color overrides. | False |
| `-fps [1-120]` | Sets custom target frame rate pacing limit constraints. | 30 |
| `-catcolor [hex]` | Overrides the default black cat body base layout. | 000000 |
| `-detailcolor [hex]`| Overrides default white accents and sclera layout. | ffffff |
| `-tiecolor [hex]` | Overrides default necktie hex color channel. | fdfdfd |
| `-pupilcolor [hex]` | Overrides moving eye pupil hex color channel. | 000000 |
| `-hourcolor [hex]` | Overrides default hour clock hand hex color. | 000000 |
| `-minutecolor [hex]`| Overrides default minute clock hand hex color. | 000000 |
| `-secondcolor [hex]`| Overrides default sweeping second hand hex color. | ff0000 |
| `-scleracolor [hex]`| Overrides the static eye background socket color layout. | ffffff |

### Interactive Runtime Controls
*   Press **`+`** or **`=`**: Scale window widget up by `0.5x`.
*   Press **`-`**: Scale window widget down by `0.5x`.
*   Press **`Escape`**: Safely terminates the execution loop.
*   **Mouse Wheel**: Scroll up to zoom in, scroll down to zoom out.
*   **Left Click & Drag**: Freely moves the borderless clock across the desktop workspace.

---

## 🐧 Linux / GNOME (Wayland) Note

Due to security constraints in native Wayland desktop environments, applications cannot programmatically force themselves to stay pinned as "Always on Top".

If you want the clock window permanently floating above other application windows in GNOME:
1. Click the clock window to focus it.
2. Press **`Alt` + `Space`** to open the native window manager menu.
3. Select **`Always on Top`**.

---

## 📜 Attribution & History

*   **The Original**: Inspired by the classic X11/Motif `catclock` desktop widget program.
*   **Asset Origins**: 1-bit XBM graphic assets are derived from the historical open-source X11 layout repository.
*   **2026 Refactoring Pass**: Engineered in a collaborative partnership between the User and Google Gemini AI to optimize the core asset pipeline, structure pre-baked memory packing, and achieve production-grade performance.

---

## 📄 License

Open Source / Educational. Please preserve all original authorship attributions upon redistributing any source components.
