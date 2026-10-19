"""Run one bounded, muted MKSC rendering observation from a supplied checkpoint."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import time

parser = argparse.ArgumentParser(description=__doc__)
for key in ('exe', 'rom', 'bios', 'state', 'config', 'output'):
    parser.add_argument('--' + key, type=Path, required=True)
parser.add_argument('--scale', type=int, choices=(1, 2, 3, 4), required=True)
parser.add_argument('--frames', type=int, default=600)
parser.add_argument('--window', action='store_true')
parser.add_argument('--profile', action='store_true')
parser.add_argument('--replay', type=Path)
parser.add_argument('--catalog-selection', action='store_true',
                    help='Use the saved mod selection instead of a CLI scale override')
args = parser.parse_args()
if args.frames < 1:
    parser.error('--frames must be positive')
for key in ('exe', 'rom', 'bios', 'state', 'config'):
    if not getattr(args, key).is_file():
        parser.error(f'missing {key}: {getattr(args, key)}')
args.output.mkdir(parents=True, exist_ok=True)
output = args.output.resolve()
env = {k: v for k, v in os.environ.items() if not k.startswith('GBARECOMP_')}
env.update(SDL_AUDIODRIVER='dummy', GBARECOMP_STRICT_STATIC='1',
           GBARECOMP_SELFHEAL_RECOMPILE='0', GBARECOMP_NO_VSYNC='1')
if args.profile:
    env['GBARECOMP_PHASE_PROF'] = '1'
if args.replay:
    env['GBARECOMP_INPUT_REPLAY'] = str(args.replay.resolve())
command = [str(args.exe.resolve()), '--window' if args.window else '--no-window',
           '--uncapped', '--frames', str(args.frames),
           '--bios', str(args.bios.resolve()), '--rom', str(args.rom.resolve()),
           '--load-state', str(args.state.resolve()), '--save-path', str(output / 'isolated.sav'),
           '--dump-state', str(output / 'final.state'), '--dump-png', str(output / 'final.png')]
if not args.catalog_selection:
    command.extend(('--internal-resolution', str(args.scale)))
if args.window:
    command.extend(('--resize-view', '--fullscreen=1'))
command.append(str(args.config.resolve()))
launch = {}
if os.name == 'nt':
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    launch.update(startupinfo=startup, creationflags=subprocess.CREATE_NO_WINDOW)
started = time.perf_counter()
with (output / 'run.log').open('w', encoding='utf-8') as log:
    process = subprocess.Popen(command, cwd=output, env=env, stdout=log, stderr=subprocess.STDOUT, **launch)
    print(json.dumps({'pid': process.pid, 'scale': args.scale, 'window': args.window, 'log': str(output / 'run.log')}), flush=True)
    code = process.wait()
elapsed = time.perf_counter() - started
log = (output / 'run.log').read_text(encoding='utf-8', errors='replace')
required = ['self_heal_coverage=FULLY_STATIC dispatch_misses=0 interpreted_insns=0 healed_native=0',
            'unmapped=0 io_unhandled=0']
# Windowed runs suppress some ordinary asset/backend banners. Validate the
# supplied retail inputs independently; do not mistake a quiet banner for a
# failed BIOS execution path. This harness clears all legacy HLE overrides.
if hashlib.sha1(args.bios.read_bytes()).hexdigest() != '300c20df6731a33952ded8c436f7f186d25d3492':
    raise SystemExit('Unexpected retail BIOS input')
if hashlib.sha1(args.rom.read_bytes()).hexdigest() != '9d327c030c3e2d9007990518594f70c3340ac56f':
    raise SystemExit('Unexpected AMKE0 ROM input')
if code or any(token not in log for token in required) or not re.search(r'^bounded_(?:run|window) ', log, re.M):
    raise SystemExit(f'Failed or inconclusive observation (exit {code}): {output / "run.log"}')
with (output / 'final.png').open('rb') as image:
    header = image.read(24)
    dimensions = struct.unpack('>II', header[16:24])
if args.scale > 1:
    match = re.search(r'presentation scale=(\d+) effective=(\d+)x(\d+) complete_frames=(\d+) affine_lines=(\d+) samples=(\d+)', log)
    if not match or int(match[1]) != args.scale or int(match[4]) < args.frames-2 or not int(match[6]):
        raise SystemExit('High-resolution raster did not execute over the requested race observation')
    if dimensions != (int(match[2]), int(match[3])):
        raise SystemExit('Captured image does not match effective raster dimensions')
    if args.window:
        presented = re.search(r'hd_presented=(\d+)', log)
        if not presented or int(presented[1]) < args.frames - 3:
            raise SystemExit('Completed high-resolution frames were not presented to the host window')
summary = {
    'command': command, 'exe_sha256': hashlib.sha256(args.exe.read_bytes()).hexdigest(),
    'initial_state_sha256': hashlib.sha256(args.state.read_bytes()).hexdigest(),
    'final_state_sha256': hashlib.sha256((output / 'final.state').read_bytes()).hexdigest(),
    'scale': args.scale, 'window': args.window, 'profiled': args.profile,
    'catalog_selection': args.catalog_selection,
    'process_wall_seconds_including_startup': elapsed, 'image_dimensions': dimensions,
    'bounded_run': re.search(r'^bounded_(?:run|window) .+$', log, re.M)[0],
    'presentation': re.findall(r'^presentation .+$', log, re.M),
    'phase_profile': re.findall(r'^\[phase\].+$', log, re.M),
    'scope': 'One checkpoint and bounded input sequence; not campaign or cross-build netplay qualification.'
}
(output / 'observation.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
print(json.dumps(summary, indent=2))
