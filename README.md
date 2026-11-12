# Mario Kart: Super Circuit Recomp (Switch Port)

> **Nintendo Switch Homebrew Exclusive**  
> Ported and optimized for the Nintendo Switch by **Niecks90** (v0.2.0).

Static recompilation of **Mario Kart: Super Circuit** running natively on the Nintendo Switch at a smooth **60 FPS**, featuring multi-threaded PPU rasterization, Adaptive Widescreen (16:9), optional 60 FPS track rendering, and configurable internal resolution.

The game ROM and Nintendo GBA BIOS are **not included**. You must provide your own legally obtained dumps.

---

## What's New in v0.2.0

- **Multi-Core PPU Architecture**: PPU scanline rasterization is parallelized across **Core 0** and **Core 2** in 8-line bands, while the main emulation thread runs isolated on **Core 1** with a 32 MiB stack.
- **Horizon-Native FramePacer**: Custom frame pacer using libnx `svcSleepThread` with spin-tails and quantum yields, eliminating the ~50 FPS timer drift and fixing all audio crackling / buffer underruns.
- **Full Widescreen Support (16:9)**: Fixed widescreen margin detection across **all 40 courses** (including Riverside Park, Sky Garden, Boo Lake, Cheese Land, Rainbow Road) and Battle arenas.
- **Un-throttled GPU & Memory Bus**: Default boost mode runs the CPU at 1020 MHz while leaving the Tegra X1 GPU and EMC memory bus at full speed (1600 MHz), removing memory bus starvation.
- **ARM64 NEON Texture Presentation**: Fast SIMD ABGR8888 conversion for instant SDL2 presentation.
- **Robust Configuration**: Case-insensitive and alias-friendly `config.txt` parser supporting `60fps`, `fps60`, `uncapped`, `cpu_boost`, and more.
- **Custom Homebrew Icon & NACP Metadata**: Embedded 0.2.0 metadata and high-resolution application icon.

---

## SD Card Layout

Create a directory on your SD card at `sdmc:/switch/mksc_nx/` and place your files as follows:

```
/switch/mksc_nx/
 ├── mksc_nx.nro       (The game executable)
 ├── rom.gba           (Your Mario Kart: Super Circuit USA ROM - SHA1: 9d327c030c3e2d9007990518594f70c3340ac56f)
 ├── gba_bios.bin      (Your retail GBA BIOS - optional if HLE BIOS is used)
 ├── config.txt        (Configuration file, auto-created on first launch)
 └── save/             (Auto-created directory for game saves)
```

> [!IMPORTANT]
> For optimal performance, always launch this homebrew using **Title Override** (hold `R` while launching an installed retail game from the Switch home menu), rather than Applet Mode (Album).

---

## Controls

| Switch Control | Action |
|---|---|
| **D-Pad / Left Stick** | Steer / Navigation |
| **A** | Accelerate |
| **B** | Brake / Reverse |
| **L / R** | Hop / Drift / Use Item |
| **+ (Start)** | Pause / Menu |
| **- (Select)** | Change Camera / View |
| **+ and - Held Together** | **Exit the game cleanly to Homebrew Menu** |

---

## Configuration (`config.txt`)

On first launch, `config.txt` is automatically created in `sdmc:/switch/mksc_nx/config.txt` with default settings:

```ini
widescreen = 1
fps60 = 1
vsync = 0
uncapped = 0
cpu_boost = 0
high_res_affine = 0
linear_filter = 0
show_fps = 0
audio = 1
```

### Options Explained:

- **`widescreen`** (`1` or `0`):
  - `1` *(Default)*: Expands rendering to 16:9 (284×160) without stretching. HUD elements are edge-anchored and real course margins are drawn.
  - `0`: Authentic original 3:2 aspect ratio (240×160) with black pillarboxes.
- **`fps60`** or **`60fps`** (`1` or `0`):
  - `1` *(Default)*: Recalculates the Mode 7 perspective track every frame (60 FPS) for maximum visual smoothness.
  - `0`: Generates the track every second frame (30 FPS), exactly as authored on the original GBA hardware (saves CPU load).
- **`vsync`** (`0` or `1`):
  - `0` *(Default)*: Uses the Horizon-native `FramePacer` to pace to the GBA's exact 59.7275 Hz refresh rate without blocking the emulation thread on HDMI scanout.
  - `1`: Synchronizes presentation directly with the Switch display refresh.
- **`cpu_boost`** (`0`, `1`, or `2`):
  - `0` *(Default / Recommended)*: Normal mode (CPU 1020 MHz, GPU un-throttled up to 768 MHz, memory EMC at full 1600 MHz).
  - `1`: FastLoad mode (CPU clocked up to 1785 MHz for heavy workloads or high-res affine).
  - `2`: Type2 legacy mode (CPU 1020 MHz, GPU throttled to 76.8 MHz).
- **`uncapped`** (`0` or `1`):
  - `0` *(Default)*: Paced to 59.7275 Hz.
  - `1`: Disables frame pacing for benchmarking and profiling.
- **`high_res_affine`** (`0` or `1`): Enables higher internal resolution for the Mode 7 perspective track.
- **`linear_filter`** (`0` or `1`): Enables bilinear texture filtering.
- **`show_fps`** (`0` or `1`): Displays an on-screen FPS counter in the upper-left corner.
- **`audio`** (`1` or `0`): Enables or disables sound output.

---

## Building from Source

Building requires [devkitPro](https://devkitpro.org/) with the `devkitA64` toolchain and Switch portlibs installed.

```bash
export DEVKITPRO=/opt/devkitpro
export DEVKITA64=/opt/devkitpro/devkitA64
export PATH=/opt/devkitpro/tools/bin:$DEVKITA64/bin:$PATH

./build_switch.sh
```

The output executable `mksc_nx.nro` will be placed in `build/`.

---

## Credits

- **Niecks90**: Nintendo Switch port, multi-core architecture, performance optimizations, Horizon FramePacer, and build integration.
- **mstan**: Original `gbarecomp` framework and `MarioKartSuperCircuitRecomp` project.
- **antimattur**: 60 FPS track patch research and decompilation contributions.
- **jellees**: Additional decompilation and reverse-engineering work.
- **devkitPro**: Nintendo Switch homebrew toolchain (`devkitA64`, `libnx`).

---

## Legal

This is an unofficial, non-commercial preservation and educational project. It is not affiliated with, authorized, or endorsed by Nintendo. All trademarks, registered trademarks, game titles, and assets are property of their respective owners. No copyrighted game assets, ROMs, or BIOS binaries are provided.
