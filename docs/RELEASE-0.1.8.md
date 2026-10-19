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

Packaged validation and hashes are recorded after the release builds complete.
