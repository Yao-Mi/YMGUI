#!/usr/bin/env python3
"""Prepare CC0 recorded clap/snap. Offline only; requires FFmpeg, no NumPy."""
import argparse
import array
import hashlib
from pathlib import Path
import subprocess
import sys
import wave

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('directory', type=Path)
args = parser.parse_args()
checks = {'clap': '03dc4fa78deabcbfa0972accf45e538b25b69526e69fad6a81b48ca508caeb63',
          'snap': 'bbcae48d531e02060a6ed6efec7bdf12f606fb32bfb44567f1e1463fc767a05a'}
manifest = []
for name, sha in checks.items():
    source = args.directory / (name + '_source.wav')
    assert hashlib.sha256(source.read_bytes()).hexdigest() == sha
    # Keep the recorded transient; remove low thud and gently brighten its upper spectrum.
    data = subprocess.check_output(['ffmpeg', '-v', 'error', '-i', str(source), '-ac', '1',
                                   '-ar', '48000', '-af', 'highpass=f=650:p=2,treble=g=6:f=2600:t=q:w=0.707',
                                   '-f', 'f32le', '-'])
    samples = array.array('f', data)
    if sys.byteorder != 'little': samples.byteswap()
    peak = max(abs(v) for v in samples)
    assert peak > .001
    first = next(i for i, v in enumerate(samples) if abs(v) >= peak * .08)
    start = max(0, first - 24)  # half-millisecond pre-roll, not a long fade-in
    last = max(i for i, v in enumerate(samples) if abs(v) >= peak * .002)
    end = min(len(samples), last + 240)
    samples = samples[start:end]
    result = array.array('h', (round(v / peak * 24575 * min(1, i / 2, (len(samples) - 1 - i) / 240))
                              for i, v in enumerate(samples)))
    if sys.byteorder != 'little': result.byteswap()
    output = args.directory / (name + '.wav')
    with wave.open(str(output), 'wb') as stream:
        stream.setparams((1, 2, 48000, len(result), 'NONE', 'not compressed'))
        stream.writeframes(result.tobytes())
    checksum = hashlib.sha256(output.read_bytes()).hexdigest()
    manifest.append(checksum + '  ' + output.name + '\n')
    print(output.name, len(result) / 48000, checksum)
(args.directory / 'samples.sha256').write_text(''.join(manifest))
