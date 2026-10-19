"""Check that AMKE0's affine course fly-in renders at the selected 4x density."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import socket
import struct
import subprocess
import time
import zlib


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def png(path, rgb, width, height):
    def chunk(tag, data):
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data) & 0xffffffff)
    rows = b''.join(b'\0' + rgb[y*width*3:(y+1)*width*3] for y in range(height))
    path.write_bytes(b'\x89PNG\r\n\x1a\n' +
                     chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0)) +
                     chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))


parser = argparse.ArgumentParser(description=__doc__)
for key in ('exe', 'rom', 'bios', 'config', 'output'):
    parser.add_argument('--' + key, type=Path, required=True)
parser.add_argument('--state', type=Path, help='Optional checkpoint at or before frame 8200 from this input route')
parser.add_argument('--replay', type=Path, default=Path(__file__).resolve().parents[1] / 'tests/mksc-gameplay-fuzz.input')
args = parser.parse_args()
for key in ('exe', 'rom', 'bios', 'config', 'replay', 'state'):
    path = getattr(args, key)
    if path is not None and not path.is_file():
        parser.error(f'missing {key}: {path}')
if hashlib.sha1(args.rom.read_bytes()).hexdigest() != '9d327c030c3e2d9007990518594f70c3340ac56f':
    parser.error('expected AMKE USA revision 0')
if hashlib.sha1(args.bios.read_bytes()).hexdigest() != '300c20df6731a33952ded8c436f7f186d25d3492':
    parser.error('expected retail GBA BIOS')
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)
if any(out.iterdir()):
    parser.error('--output must be empty to preserve earlier evidence')
with socket.socket() as reservation:
    reservation.bind(('127.0.0.1', 0))
    port = reservation.getsockname()[1]
env = {k: v for k, v in os.environ.items() if not k.startswith('GBARECOMP_')}
env.update(SDL_AUDIODRIVER='dummy', GBARECOMP_STRICT_STATIC='1', GBARECOMP_SELFHEAL_RECOMPILE='0')
command = [str(args.exe.resolve()), '--no-window', '--internal-resolution', '4', '--tcp', str(port),
           '--rom', str(args.rom.resolve()), '--bios', str(args.bios.resolve()),
           '--save-path', str(out / 'isolated.sav'), str(args.config.resolve())]
inputs = []
for line in args.replay.read_text().splitlines():
    if line and not line.startswith('#'):
        frame, keys = line.split(',')
        inputs.append((int(frame), int(keys, 0)))
shots = {8200, 8300, *range(8340, 8701, 20), 8800, 9000}
events = sorted({0, 9000, *shots, *(f for f, _ in inputs if f <= 9000)})
observations = []
with (out / 'run.log').open('w', encoding='utf-8') as log:
    process = subprocess.Popen(command, cwd=out, env=env, stdout=log, stderr=subprocess.STDOUT,
                               creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
    try:
        print(json.dumps({'pid': process.pid, 'port': port, 'output': str(out)}), flush=True)
        deadline = time.monotonic() + 90
        while True:
            try:
                conn = socket.create_connection(('127.0.0.1', port), timeout=1)
                break
            except OSError:
                if process.poll() is not None or time.monotonic() >= deadline:
                    raise RuntimeError('TCP startup failed; inspect run.log')
                time.sleep(.2)
        # Buffered reads matter: an unbuffered readline performs millions of
        # single-byte socket reads for a high-resolution screenshot.
        with conn, conn.makefile('rwb') as stream:
            def call(cmd, **fields):
                stream.write((json.dumps(dict(cmd=cmd, **fields)) + '\n').encode())
                stream.flush()
                conn.settimeout(300)
                reply = json.loads(stream.readline())
                if not reply.get('ok'):
                    raise RuntimeError(reply)
                return reply

            frame, keys = 0, 1023
            if args.state:
                call('savestate_load', path=str(args.state.resolve()))
                frame = int(call('frame')['frame'])
                if frame > 8200:
                    raise RuntimeError('checkpoint starts after the course-intro trigger')
                for f, value in inputs:
                    if f <= frame:
                        keys = value
            try:
                for target in events:
                    if target < frame:
                        continue
                    if target > frame:
                        call('run_frames', n=target-frame, keyinput=keys)
                        frame = target
                    for f, value in inputs:
                        if f == frame:
                            keys = value
                    if frame not in shots:
                        continue
                    image = call('presentation_screenshot')
                    rgb = bytes.fromhex(image.pop('data'))
                    png(out / f'{frame}.png', rgb, image['w'], image['h'])
                    native = call('screenshot')
                    native_rgb = bytes.fromhex(native.pop('data'))
                    png(out / f'{frame}-native.png', native_rgb, native['w'], native['h'])
                    io = call('read_io', addr=0x04000000, len=96)
                    (out / f'{frame}.io').write_bytes(bytes.fromhex(io['data']))
                    record = dict(frame=frame, presentation=image, native_sha256=hashlib.sha256(native_rgb).hexdigest())
                    observations.append(record)
                    print(json.dumps(record), flush=True)
                    if frame in (8200, 8400, 8600, 9000):
                        call('savestate_save', path=str(out / f'{frame}.state'))
            finally:
                call('quit')
    finally:
        if process.poll() is None:
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.terminate()
                process.wait(timeout=10)
    code = process.wait(timeout=30)
text = (out / 'run.log').read_text(encoding='utf-8', errors='replace')
last_samples, failures = 0, []
for obs in observations:
    image, frame = obs['presentation'], obs['frame']
    if frame >= 8360 and (image.get('scale') != 4 or (image['w'], image['h']) != (960, 640)):
        failures.append(f'{frame}: missing completed 4x surface')
    if 8360 <= frame <= 8640 and image.get('samples', 0) <= last_samples:
        failures.append(f'{frame}: no additional affine samples during the visible fly-in')
    last_samples = image.get('samples', 0)
if code or 'dispatch_misses=0 interpreted_insns=0 healed_native=0' not in text:
    failures.append('nonzero exit or incomplete native coverage')
report = dict(command=command, exe_sha256=sha(args.exe), replay_sha256=sha(args.replay),
              state_sha256=sha(args.state) if args.state else None, observations=observations, failures=failures,
              scope='AMKE0 input-driven course intro; native-aspect headless raster test, not window performance or campaign coverage.')
(out / 'observations.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
if failures:
    raise SystemExit('\n'.join(failures))
print('PASS: course-intro affine sampling stays active before the race HUD')
