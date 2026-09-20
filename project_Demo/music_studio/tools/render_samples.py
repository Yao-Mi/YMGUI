#!/usr/bin/env python3
"""Offline preparation: render every built-in instrument and drum from GeneralUser GS.
Requires Python 3 and libfluidsynth; the music application does not link FluidSynth.
"""
import argparse
import array
import ctypes as C
import ctypes.util
import hashlib
from pathlib import Path
import struct
import sys
import json
import wave

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('soundfont', type=Path)
parser.add_argument('output', type=Path)
parser.add_argument('--library', default=ctypes.util.find_library('fluidsynth'))
parser.add_argument('--drums-only', action='store_true')
args = parser.parse_args()
if not args.library:
    parser.error('libfluidsynth not found; pass --library /path/to/library')
f = C.CDLL(args.library)
def api(name, result, *types):
    fn = getattr(f, name)
    fn.restype, fn.argtypes = result, list(types)
    return fn
ptr, integer, string = C.c_void_p, C.c_int, C.c_char_p
new_settings = api('new_fluid_settings', ptr)
setnum = api('fluid_settings_setnum', integer, ptr, string, C.c_double)
setint = api('fluid_settings_setint', integer, ptr, string, integer)
new_synth = api('new_fluid_synth', ptr, ptr)
sfload = api('fluid_synth_sfload', integer, ptr, string, integer)
program = api('fluid_synth_program_select', integer, ptr, integer, integer, integer, integer)
on = api('fluid_synth_noteon', integer, ptr, integer, integer, integer)
kill = api('fluid_synth_all_sounds_off', integer, ptr, integer)
render = api('fluid_synth_write_float', integer, ptr, integer, ptr, integer, integer, ptr, integer, integer)
del_synth = api('delete_fluid_synth', None, ptr)
del_settings = api('delete_fluid_settings', None, ptr)
rate, frames = 24000, 144000
settings = new_settings()
assert settings
setnum(settings, b'synth.sample-rate', rate)
setnum(settings, b'synth.gain', .6)
setint(settings, b'synth.reverb.active', 0)
setint(settings, b'synth.chorus.active', 0)
setint(settings, b'synth.cpu-cores', 1)
synth = new_synth(settings)
assert synth
voices = [('piano', 0), ('electric_piano', 4), ('bass', 33), ('guitar', 24),
          ('strings', 48), ('organ', 19), ('flute', 73), ('lead', 80),
          ('horn', 60), ('brass', 61), ('timpani', 47), ('voice', 53), ('choir', 52)]
drums = [('kick', 36, .8), ('snare', 38, .8), ('closed_hat', 42, .3),
         ('open_hat', 46, 2), ('low_tom', 45, 1.2), ('high_tom', 50, 1.2), ('crash', 49, 3.5)]
args.output.mkdir(parents=True, exist_ok=True)
assert hashlib.sha256(args.soundfont.read_bytes()).hexdigest() == '9575028c7a1f589f5770fccc8cff2734566af40cd26ed836944e9a5152688cfe'
manifest = {}
if args.drums_only:
    for row in (args.output / 'samples.sha256').read_text().splitlines():
        sha, file = row.split(); manifest[file] = sha
def mono(count):
    left, right = (C.c_float * count)(), (C.c_float * count)()
    assert render(synth, count, left, 0, 1, right, 0, 1) == 0
    return array.array('f', ((left[i] + right[i]) * .5 for i in range(count)))
def encode(samples, gain, fade=0):
    count = len(samples)
    output = array.array('h', (round(v * gain * min(1, (count - 1 - i) / fade if fade else 1)) for i, v in enumerate(samples)))
    if sys.byteorder != 'little': output.byteswap()
    return output.tobytes()
def record(path):
    manifest[path.name] = hashlib.sha256(path.read_bytes()).hexdigest()
    print(path.name, manifest[path.name], flush=True)
try:
    sf = sfload(synth, str(args.soundfont.resolve()).encode(), 0)
    assert sf >= 0
    for slug, preset in ([] if args.drums_only else voices):
        assert program(synth, 0, sf, 0, preset) == 0
        zones = []
        for pitch in range(36, 85, 4):
            kill(synth, 0)
            assert on(synth, 0, pitch, 100) == 0
            zones.append(mono(frames))
        peak = max(abs(v) for zone in zones for v in zone)
        assert peak > .001
        path = args.output / (slug + '.bank')
        with path.open('wb') as stream:
            stream.write(b'MSCHOIR1' + struct.pack('<IIIII', rate, len(zones), frames, 36, 4))
            for zone in zones:
                stream.write(encode(zone, 24575 / peak))
        record(path)
    # Percussion is rendered at 24 kHz then resampled offline to the engine's 48 kHz.
    kill(synth, 0)
    assert program(synth, 9, sf, 128, 0) == 0
    for slug, pitch, duration in drums:
        kill(synth, 9)
        assert on(synth, 9, pitch, 110) == 0
        samples = mono(round(duration * rate))
        peak = max(abs(v) for v in samples)
        assert peak > .001
        # Linear interpolation; output tail explicitly returns to zero.
        doubled = array.array('f')
        for i, v in enumerate(samples):
            doubled.append(v)
            doubled.append((v + samples[min(i + 1, len(samples) - 1)]) * .5)
        path = args.output / (slug + '.wav')
        with wave.open(str(path), 'wb') as stream:
            stream.setparams((1, 2, 48000, len(doubled), 'NONE', 'not compressed'))
            stream.writeframes(encode(doubled, 24575 / peak, 480))
        record(path)
    (args.output / 'samples.sha256').write_text(''.join(sha + '  ' + file + '\n' for file, sha in manifest.items()))
finally:
    del_synth(synth)
    del_settings(settings)
