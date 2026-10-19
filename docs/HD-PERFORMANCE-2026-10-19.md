# 8x CPU rendering candidate, 2026-10-19

Owner feedback: v0.1.7 is too slow at 8x and 16x. This candidate replaces the
HD renderer's repeated tile/map/palette walks with a persistent indexed affine
surface. Changed tiles/map cells update at each published scanline; palette
colors refresh every row. The integer bilinear filter processes three separate
channels in one portable 64-bit calculation. Ordinary rows expand once and copy
their subrows. Guest CPU execution and canonical rendering/state are unchanged.

The existing native tile-span compositor was already HLE in v0.1.7. These changes
optimize its separate high-resolution presentation service. Select the maintained
direct sampler with `GBARECOMP_HD_SAMPLER_IMPLEMENTATION=LLE`; the candidate uses
`HLE`. Selection is fixed at build time, reported as `hd_sampler_backend`, and
included in strict netplay source identity. No cross-build compatibility is
claimed, and canonical simulation/profile checks are unchanged.

## Matched Windows measurement

AMD Ryzen 7 9800X3D, Windows, D3D11; installed GPUs include RTX 3080 Ti and the
integrated Radeon. No GPU selection/isolated-host guarantee is inferred.
Release build, existing 64-shard generated corpus at O1, runtime at O3.
Owned AMKE0 ROM and retail BIOS, fixed logical 382x160 view, 8x raster
3056x1280, frontlit color model and affine filtering. Actual hidden 382x160 SDL
window, muted audio output with normal audio work, uncapped/vsync off, frozen RTC.
Expensive diagnostics disabled; phase counters enabled. Load the same frame-9000
checkpoint and present 300 frames through 9299. Frame dumps and startup are
outside the reported bounded interval. This small destination window does not
establish fullscreen performance on the owner's 3440x1440 monitor.

| Sampler | FPS | Bounded seconds | Inclusive scanline CPU ms/frame |
| --- | ---: | ---: | ---: |
| Direct LLE | 20.403 | 14.703795 | 38.70 |
| Cached HLE | 32.941 | 9.107101 | 21.92 |

The matched implementation pair shows **61.5% higher FPS**, still below full
speed. The HLE observation precedes the LLE observation. Host load varies: the
initial public-v0.1.7 observation was 15.749 FPS, so quoting the apparent 109%
gain over that older observation would overstate the better-matched result.
This is one stationary Peach Circuit route, not a gameplay-wide guarantee.
No automatic timing matrix or repeat-until-pass campaign was run.

The `render_scanline` phase is included in `guest_us`: treating all `guest_us`
as guest CPU execution would misidentify the bottleneck. Presentation still
costs roughly 5.5 ms/frame in the HLE run, including grading/upload. A GPU affine
service remains the larger next opportunity if 8x must reach full speed.

HLE executable SHA-256:
`7e4d3bc6d6c2a4559028a368216fe3a63d87511ed62ea21514f20a207ce41beb`.
LLE executable SHA-256:
`d243d2b9e0698f9a06fee3293bc08dad71aa55a1cb1d9e48895a3287d3f5ee61`.
Evidence: `build-release/qa/hd-perf-20261019/{inline,direct}.json`, corresponding
`*-8x/run.log`, phase CSVs, final states and images. Earlier `baseline`,
`indexed-ready` and `packed` observations retain the investigation steps.

## Correctness and handoff

Both final images match the v0.1.7 image exactly. PNG SHA-256:
`7f01b261b116e0aa36937750adc3682e905342d8ac109adf2d2abb865448c833`.
The complete final single-player snapshot matches in all measured builds:
`579999e6a3822043d738a015f72e815255537ad6003842bd600103a63ba19f39`.
All runs report zero static dispatch misses, interpreted instructions, healed
native calls, unmapped accesses and unhandled IO. A current image was inspected:
road, sprites, margin scenery and HUD are present without garbling.

Existing high-resolution, bus and presentation-mirror tests pass in the cached
HLE and direct LLE configurations. The raster fixture adds map/layout changes
and filter channel saturation/rounding, while retaining per-row source writes,
blend/HUD checks and reset/restore/resize coverage. Both title visibility tests
pass. Both samplers compile for wasm; browser presentation still uses the
existing native-density shared-memory protocol. Full non-Windows builds and
performance remain unqualified for this candidate.

The actual two-machine game view probe compares every canonical byte for all
72 frames of each existing native/wide/adaptive case and enhanced scales through
16x, including 12-frame restore/replay for each nonreference case. It passes
with the cached HLE sampler. Evidence: `hd-perf-20261019/paired.log`. This is a
local deterministic differential, not an actual transport or cross-build test.

The owner candidate must use the measured HLE executable above, fresh boot,
8x Adaptive/Fit to screen and normal audio. Keep the existing owner's saves and
prior playtest directory intact. The owner confirmed improved speed and
authorized default promotion. Engine `ec778f1` and title `9bc2520` are integrated
on main; the cached HLE sampler is the shared build default. The public v0.1.7
download remains unchanged. Central issue:
`beads-bce3`; other selected games in that issue are still pending.
