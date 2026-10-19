# v0.1.6: higher internal resolution

Adds the optional Higher Internal Resolution mod with 2x, 3x and 4x choices.
Affine course views receive finer samples before the race HUD appears, avoiding
the quality change during the course fly-in. Adaptive Widescreen also renders
resident opaque roadside billboards beyond the original draw boundary and
handles wrapped sprite X coordinates. Sprites and HUD retain their original
artwork. Enhancement packages remain disabled by default.

The release integrates the pilot with current engine main, preserving its
canonical netplay raster suppression, per-game view ceilings, replay diagnostics,
pause handling and newer CPU/discovery fixes. ROM and BIOS sources were freshly
regenerated with that engine; generated bodies were not hand-edited or committed.
The pinned engine also retains the browser host's native-density fallback for
sampled input, without changing its shared-memory video protocol.

The Windows ZIP and Linux AppImage require the supported USA ROM and retail
GBA BIOS. Neither package contains those inputs, cartridge saves, savestates,
remembered settings or private paths. Packaging verifies the executable's
`--version` against the archive version. The Windows ZIP's DLL imports and
portable entry paths are checked after extraction. Its file-list change from
v0.1.5 adds the internal-resolution manifest; the old extra PROJECT_README.md
is superseded by the packaged player README.

Validation covers the focused title tests and shared high-resolution, bus and
netplay-presentation tests under both HLE and maintained LLE configurations.
The course-intro route reaches Peach Circuit through ordinary inputs from a
fresh boot. All 23 captured native frame hashes match the original pilot's
baseline; the visible pre-HUD fly-in receives new enhanced samples throughout.
Read-only actor observations and screenshots verify roadside trees in margins.

The current-main session schema rejects the older pilot's raw paired fixture.
A replacement fixture was obtained from the release packages' own confirmed
360-tick checkpoint, with both archive checksums verified, then advanced into
a race with ordinary controller inputs. The local view differential compares
every canonical byte for 72 frames at native, 16:9, 21:9, 32:9 and adaptive
views, plus native 4x, 16:9 2x and adaptive 4x. Every non-reference case also
restores twelve frames and replays to the same state. Production checkpoint
identity checks were preserved; no archive identity was rewritten.

Actual packaged Windows and Linux executables also connect in both rollback
and input-delay modes. Windows uses native 1x while Linux uses adaptive 4x.
Each cold-boot, no-input run agrees on the complete paired state at tick 360.
This is a bounded interoperability check, not a long Internet or race test.
Both packaged platforms receive strict 6,000-frame boot checks with zero
dispatch misses, interpreted instructions or healed functions.

The browser host implementation and runtime compile with Emscripten and its
required tail-call configuration. This checks portability of the new interface;
it does not establish enhanced-density browser display or a browser gameplay audit.

Local commands, raw logs, checksums, images and snapshots are retained under
`build-release/qa/`. Actual course/prop evidence is limited to Peach Circuit.
Other ROM revisions, a full course/prop audit, translucent or OBJ-window
billboard cells and long online matches remain unverified. The host was shared
with other work; no new performance guarantee is inferred from these checks.

Release engine: `76f4d40a7ac4d7989f274e81fd9e622ca1aa2605`.
The Windows executable reads back `MarioKartSuperCircuitRecomp v0.1.6` and
has SHA-256 `712d1705e9c281739d8a1f4c8d58debf212fed8ed49554fc005049d30a4010a6`.

| Artifact | SHA-256 |
|---|---|
| Windows ZIP | `8ce4179de727df85c3d64a9ac7f4054339cabd9cc863b606f591a6c65f38c86e` |
| Linux AppImage | `5b606b309f40c15d80f905b249f944a0044794490cd948ca7f6006bb11b1ad42` |

Final cross-platform checkpoint state SHA-256 values (equal between peers
within each mode): rollback
`14e15bc3da1b54a111c51449a2f0f6b12f2ffd5ef28573c75d071a847da5b1cb`,
input delay `613164061736a4f928910cf188fbd70914a97137b89e5f3b24842935b833abdc`.
Their complete paired-state payloads are 1,402,786 bytes at tick 360.
