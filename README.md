# Mario Kart: Super Circuit Recomp (Switch Port)

> **Nintendo Switch Homebrew Exclusive**
> This repository is a strict port of the `MarioKartSuperCircuitRecomp` project for the Nintendo Switch.

Static recompilation of **Mario Kart: Super Circuit** for Nintendo Switch, featuring optional 60 FPS track rendering, Adaptive Widescreen, and Higher Internal Resolution mods.

The game ROM and Nintendo GBA BIOS are **not included**. You must provide your own legally obtained dumps.

## SD Card Layout

Create a folder on your SD card at `sdmc:/switch/mksc_nx/` and ensure the following files are present:

```
/switch/mksc_nx/
 ├── mksc_nx.nro       (The game executable)
 ├── rom.gba           (Your Mario Kart: Super Circuit USA ROM)
 ├── gba_bios.bin      (Your retail GBA BIOS)
 ├── config.txt        (Optional auto-generated config)
 └── save/             (Auto-created directory for save files)
```

**Note:** For the best performance, it is highly recommended to run this homebrew in **Title Override** mode (by holding `R` while launching a retail game), rather than Applet Mode.

## Controls

| Switch Control | Action |
|---|---|
| D-Pad or Left Stick | Movement |
| A / B | A / B Button |
| + (Start) | Start |
| - (Select) | Select |
| L / R | L / R Triggers |
| **+ and - Held Together** | **Exit the game cleanly** |

## Configuration (`config.txt`)

On first boot, `config.txt` is automatically generated with these defaults:

```ini
widescreen = 1
fps60 = 1
linear_filter = 0
show_fps = 0
```
Change these values to `0` or `1` on your SD card to toggle the mods.

## Building from source

Building requires `devkitPro` with the `devkitA64` toolchain installed.

```bash
export DEVKITPRO=/opt/devkitpro
export DEVKITA64=/opt/devkitpro/devkitA64
export PATH=/opt/devkitpro/tools/bin:/opt/devkitpro/devkitA64/bin:$PATH

cmake -B build -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Switch.cmake
cmake --build build -j$(nproc)
```
The output `.nro` will be located at `build/mksc_nx.nro`.

## Credits

Special thanks to:
- **mstan** for `gbarecomp` and the initial recompilation framework.
- **antimattur** for the decompilation and 60 FPS patch.
- **jellees** for additional decompilation work.
- **devkitPro** for the Switch homebrew toolchain.

## Legal

This is an unofficial, non-commercial preservation and research project. It is not affiliated with or endorsed by Nintendo. Mario Kart and related names, characters, artwork, and game data are trademarks or copyrights of their respective owners. No copyrighted game ROM or Nintendo BIOS data is distributed by this project.
