# v0.1.7: higher scales and display views

Adds optional 8x and 16x internal resolution alongside 2x, 3x and 4x.
The mod remains disabled by default and its initial scale remains 4x.
8x renders four times as many pixels as 4x; 16x renders sixteen times as many.
Performance depends on the host and scene; full speed is not guaranteed.
Sprite and HUD artwork retains its authored resolution.

The issue #12 tester screenshot is 3840x2160 with a native 3240x2160 race view
and 300-pixel pillars. Its selected settings were not supplied. In v0.1.6,
single-player view controls were hidden and the Netplay page's 16:9 option
only applied online. This identifies a settings ambiguity, rather than
establishing a defect in the already-expanded 16:9 renderer.

Display now offers Default (Mods), Native (3:2), 16:9 and Adaptive. Default
preserves existing mod selections when upgrading; explicit choices take
precedence. Adaptive fits the window or fullscreen display. The widescreen
mod also offers Fit to screen / 16:9. Menus retain native framing. Netplay's
local view choice remains separate and changes no simulated state.

The engine expands its subpixel offset arrays with the scale limit. Large
surfaces that exceed GPU texture limits are reduced for upload only; the
effective upload size is logged and cached, without changing the CPU raster
or guest state.
Live changes between Native, fixed and adaptive views also reset SDL's logical
coordinate space and viewport, preventing stale native scaling from cropping
the wider destination.

Validation uses the owned AMKE USA revision 0 ROM and retail BIOS, never
distributed with the release. Focused renderer tests check fractional detail,
native sample parity, HUD/blend ordering, state isolation and invalidation at
2x/3x/4x/8x/16x. Both HLE and LLE compositor builds are covered.

A 72-frame paired-session differential compares native and expanded views,
including 16:9 8x and adaptive 16x (up to 9216x2560). Complete canonical state
is byte-identical each frame; every enhanced case restores and replays twelve
frames exactly. Source/build identity checks remain strict.

An actual SDL/Direct3D11 observation loads the existing frame-9000 Peach
Circuit checkpoint, uses saved mod choices, and presents 120 frames in a
1920x1080 client. A 284x160 race view occupies 1917x1080, leaving only pixel
rounding padding. Scale 8x produces 2272x1280; 16x produces 4544x2560.
Automated launches use dummy audio. Bounded timings are observations on this
host, not performance promises or campaign coverage.

A separate Direct3D11 upload probe requests 32768x160: the host selects
16384x80 once, reuses that surface, then successfully resizes and uploads a
4544x2560 frame. The browser host/runtime also compile with Emscripten;
browser output still uses its existing native-density protocol.
