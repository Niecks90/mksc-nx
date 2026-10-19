# v0.1.8: faster HD rendering and experimental high scales

Ships the approved cached HLE affine sampler from engine `7a05e76`, with the
maintained direct LLE sampler available for source builds. Windows and Linux
release recipes select the HLE native compositor and HD sampler explicitly.
The separate row-reuse experiment is excluded.

The sampler caches indexed track surfaces and refreshes palette/source changes
at each published row, and uses a portable integer filter. It reduces CPU cost
while retaining native guest execution, state and strict netplay compatibility
checks. Both players must update; no compatibility with older builds is claimed.

Up to 4x is recommended and 4x remains the mod's initial selection. The optional
8x and 16x choices are labeled `(experimental)`; all in-game scales above 4x
carry that label. Higher scales can still run very slowly. Sprites and HUD retain
their original artwork. Existing Display view choices and adaptive margin
rendering remain available. No ROM, retail BIOS, player saves or local settings
are included.

Previous measurements (one MKSC route, not a universal guarantee) show the
cached sampler improving 8x from 20.403 to 32.941 FPS on the desktop, and from
11.048 to 14.509 FPS on the older i5-4670/GTX 760 NAS. These measurements
explain the optimization; they do not qualify 8x as full speed. See
[performance evidence](HD-PERFORMANCE-2026-10-19.md).

## Packaged validation

Built from title source `43ee1cc`, engine `7a05e76`, clean recomp-ui `0546fbc` and
the same generated ROM/BIOS corpus on both platforms. Later validation
documentation does not change the runtime sources, CMake inputs or pins.

- Both packaged executables report v0.1.8. Archive/import/catalog checks pass;
  enhancements remain disabled initially and the mod's selected scale is 4x.
  No ROM, retail BIOS, player configuration, saves, CD Speed or Fast Loading
  packages are present.
- Windows and Linux each complete a 6,000-frame strict-static cold boot with
  zero dispatch misses, interpreted/healed instructions or unmapped/unhandled
  IO. The final Windows ZIP also passes 6,000 frames without a supplied config
  on the older i5-4670/GTX 760 NAS running Windows 10 build 19044. These are
  deployment checks, not performance measurements.
- The actual packaged Windows native-1x and Linux adaptive-16x pair completes
  360 cold-boot/no-input ticks in both rollback and input-delay modes, with
  matching complete 1,402,786-byte canonical states in each mode. This is a
  bounded package-pair check, not race-stage or long Internet-session coverage.
- A 120-present race check from frame 9000 at fixed 16:9, 4x (1136x640) matches
  state and raster across the actual packages. SDL dummy drivers mute output
  and avoid owner-facing windows; this is not GPU/display performance QA. The
  captured image was inspected and is clear. The rejected row-reuse code is
  absent from this release.

Strict source identity:
`c682d9043acb256bed01adb9a0e851cbf468ba0fcf24bd1db87ec6129e44b59e`.

Private evidence: `build-release/qa/release-0.1.8/`, including package
verification, cross-platform netplay checkpoints, race results and images;
NAS result/logs in the integration worktree's `.local/release-0.1.8/` and
its dedicated NAS validation directory. Private inputs are not published.

## SHA-256


- `MarioKartSuperCircuitRecomp-windows-x64-v0.1.8.zip`: `708130460c8d9aa1eec23d66a63ec8f61cc09a45e2d1820d9ec790d8bfba4e58`.

- `MarioKartSuperCircuitRecomp-linux-x86_64-v0.1.8.AppImage`: `3596ff3c7b9f769dff7b6fc50d69fd8911108b2d6c11897e8cf4b324940d22e4`.

- `4x-internal-resolution.png`: `0c3c9d51e617efbe4d6caf1a4fd4f167f7b53d4ce73957baefbc9f88e4fdcbae`.
