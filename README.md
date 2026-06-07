## catclock-sdl3
A modernized Kit-Cat Clock desktop widget. This project ports the [classic X11 catclock application](https://github.com/barkythedog/catclock) to the modern SDL3 framework, combining vintage pixel art aesthetics with highly optimized runtime architecture.

## 🛠️ Engineering Features

* SDL3 Native Port: Full desktop compositing support with borderless transparent windows and OS-level hit-testing.
* Low-CPU Architecture: Features pre-baked texture atlases and 1-bit binary threshold rendering to optimize alpha blending.
* Legacy Graphics Restored: Utilizes polygon scanline rasterization engines to patch geometry gaps, paired with sharp nearest-pixel integer scaling.
* Jitter-Free Pacing: Synchronizes monotonic ticks with the wall clock to eliminate frame jitter.
* Power Efficient: Implements adaptive pacing, focus-aware kernel sleeps, and zero-VSync spinlocks to minimize CPU overhead.

## Usage
Run the executable from the command line using the following syntax:

./catclock-sdl3 [flags]

## Available Flags

* --help — Display this interface parameter map.
* -noseconds — Completely hide the sweeping seconds hand.
* -nooutline — Disable the form-fitting 1px white halo background.
* -decorations — Restore standard desktop frame borders & window title-bars.
* -notop — Disable the forced 'Always on Top' window layer pinning.
* -fps [1-120] — Set a custom target frame rate pacing limit (Default: 30).

## 📋 Attribution & History

* The Original: Inspired by the classic X11/Motif catclock program.
* Asset Origins: XBM graphic assets are derived from the historical open-source X11 layout.
* 2026 Refactoring: Developed in collaborative partnership between the User and Google Gemini AI to optimize the core engine and refactor the architecture.

## 📄 License
Open Source / Educational. Please preserve all original attributions upon redistribution.
