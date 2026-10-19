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

## Final package evidence

Engine source: `23f43770682e6bb06358c80ded8590f8a6ccbbf0`. Title source at
build time: `2339fb8`; subsequent validation documentation does not change
runtime source, CMake configuration or submodule pins. Every fixed 16:9 path
uses 284x160, matching adaptive rounding and the netplay view.

The final packaged Windows native-1x and Linux adaptive-16x executables complete
360 cold-boot/no-input ticks in both rollback and delay-sync modes. Complete
1,402,786-byte canonical states match within each mode, without ignored fields:

- rollback: `316098462bfe44a594004158eeebd5c385a775b3cd8854e5a06a34a0dd5a779f`.
- delay-sync: `2e66a113ef7f0a31ed2eff2b78433d37ce5c34d9b4674be6d6c19b36a25529fa`.

This qualifies the observed package pair and bounded setup. It does not claim
cross-version compatibility, race-stage network coverage or a long Internet
session. The source compatibility gate remains unchanged.

Actual Windows package observations use nine saved-mod / explicit Display
combinations, 120 presentations each. Fixed-view 4x/8x/16x snapshots match
exactly with a frozen test RTC. Native retains its own presentation framebuffer.
The new source produces the same published 8x/16x capture bytes. Frame timings
vary with host load and are not a full-speed promise.

Final distributable SHA-256 values:

- `MarioKartSuperCircuitRecomp-windows-x64-v0.1.7.zip`: `8a2d56f35aed382eb72997cf5c413c2f9b592d7a8fcbc76b059bd7513aedc305`.
- `MarioKartSuperCircuitRecomp-linux-x86_64-v0.1.7.AppImage`: `8388fe1de71e645e1eddda4156fe98cc75d5ab55cb4648a307967ca838d3233b`.
- `8x-16-9.png`: `259a6e5a493117121c2e38b9f26a756febe83bf6c304e9542369ed2141a8e5ca`.
- `16x-16-9.png`: `d1fa426618e51ccca6620e899e4d7d56f6b3742ed8ee8b8f36e5c17fcb70bce3`.

The source screenshots on main were subsequently losslessly recompressed for
inline display (307,069 bytes at 8x and 815,392 bytes at 16x). Their decoded
pixel rows and all other PNG chunks match the original release captures.
The downloadable original captures and package checksums above are unchanged.
