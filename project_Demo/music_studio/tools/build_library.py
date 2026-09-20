#!/usr/bin/env python3
"""Convert the checked-in Mutopia selection to editable YMUSIC02 projects (stdlib only).

This is a curated library builder, not a general MIDI importer. Channel programs,
controllers and tempo maps are reported; instruments and a fixed tempo are chosen
in catalog.json. Notes remain notes, with no embedded recording.
"""
import argparse
from collections import Counter, defaultdict, deque
import hashlib
import json
from pathlib import Path
import struct

LIBRARY = Path(__file__).resolve().parents[1] / 'library'
PPQ, PERIOD = 96, 768


def read_midi(path):
    data = path.read_bytes()
    if data[:4] != b'MThd' or len(data) < 14:
        raise ValueError('Not a MIDI file')
    size, fmt, count, division = struct.unpack_from('>IHHH', data, 4)
    if size < 6 or fmt not in (0, 1) or not division or division & 0x8000:
        raise ValueError('Only simultaneous type 0/1 MIDI with PPQ is supported')
    offset = 8 + size
    notes, tempos, signatures, names, ignored = [], [], [], {}, Counter()
    for track in range(count):
        if data[offset:offset+4] != b'MTrk':
            raise ValueError('Missing MIDI track')
        length = struct.unpack_from('>I', data, offset+4)[0]
        raw = data[offset+8:offset+8+length]
        if len(raw) != length:
            raise ValueError('Truncated MIDI track')
        offset += 8 + length
        pos = tick = running = 0
        active = defaultdict(deque)

        def vlq():
            nonlocal pos
            value = 0
            for _ in range(4):
                byte = raw[pos]
                pos += 1
                value = (value << 7) | (byte & 127)
                if byte < 128:
                    return value
            raise ValueError('Invalid MIDI VLQ')

        while pos < len(raw):
            tick += vlq()
            status = raw[pos]
            if status >= 128:
                pos += 1
                running = status if status < 240 else 0
            else:
                status = running
            if status == 255:
                kind = raw[pos]
                pos += 1
                length = vlq()
                payload = raw[pos:pos+length]
                if len(payload) != length:
                    raise ValueError('Truncated MIDI meta event')
                pos += length
                if kind == 81:
                    if length != 3:
                        raise ValueError('Invalid MIDI tempo')
                    tempos.append((tick, int.from_bytes(payload, 'big')))
                elif kind == 88:
                    signatures.append((tick, payload[0], 2 ** payload[1]))
                elif kind == 3:
                    names[track] = payload.decode('utf-8', errors='replace')
                elif kind == 47:
                    break
                continue
            if status in (240, 247):
                length = vlq()
                pos += length
                if pos > len(raw):
                    raise ValueError('Truncated SysEx')
                ignored['sysex'] += 1
                continue
            if not 128 <= status < 240:
                raise ValueError('Unsupported MIDI status')
            kind, channel = status >> 4, status & 15
            length = 1 if kind in (12, 13) else 2
            payload = raw[pos:pos+length]
            if len(payload) != length or any(v >= 128 for v in payload):
                raise ValueError('Invalid MIDI channel event')
            pos += length
            if kind == 9 and payload[1]:
                active[channel, payload[0]].append((tick, payload[1]))
            elif kind == 8 or kind == 9:
                key = channel, payload[0]
                if not active[key]:
                    raise ValueError('Unpaired note-off')
                start, velocity = active[key].popleft()
                if tick > start:
                    notes.append((track, channel, start, tick, payload[0], velocity))
            else:
                ignored[f'channel_event_{kind:x}'] += 1
        if any(active.values()):
            raise ValueError('Unterminated MIDI notes')
    if offset != len(data) or not notes:
        raise ValueError('Trailing data or empty MIDI')
    return dict(division=division, notes=sorted(notes), tempos=sorted(set(tempos)),
                signatures=sorted(set(signatures)), names=names, ignored=dict(ignored))


def source_notes(midi, entry):
    """Map quarter-note time to project ticks; optional 3/8 -> 3/4 display scaling."""
    scale = entry.get('tick_scale', 1)
    start = round(entry.get('start_quarters', 0) * PPQ * scale)
    end = round(entry['end_quarters'] * PPQ * scale)
    result = []
    for track_spec in entry['tracks']:
        notes = []
        for track, channel, lo, hi, pitch, velocity in midi['notes']:
            if track not in track_spec['midi_tracks']:
                continue
            if channel == 9:
                raise ValueError('Percussion mapping is not supported by this selection builder')
            lo = round(lo * PPQ * scale / midi['division'])
            hi = round(hi * PPQ * scale / midi['division'])
            # Excerpts are explicit. A note crossing an excerpt edge is clipped,
            # but internal phrase boundaries must never split/retrigger a note.
            if lo >= end or hi <= start:
                continue
            lo, hi = max(start, lo)-start, min(end, hi)-start
            if hi <= lo:
                raise ValueError('Note shorter than project tick resolution')
            notes.append((lo, hi-lo, pitch, velocity))
        if not notes:
            raise ValueError('Empty selected track')
        result.append(sorted(notes))
    return result, end-start


def pack_patterns(tracks):
    """Pack whole notes into <= 8-beat patterns; overlapping clips preserve ties."""
    patterns, clips, lookup = [], [], {}
    for track, notes in enumerate(tracks):
        index = 0
        while index < len(notes):
            start = notes[index][0]
            group = []
            while index < len(notes) and len(group) < 128:
                tick, length, pitch, velocity = notes[index]
                if length > PERIOD:
                    raise ValueError('A note exceeds the eight-beat pattern limit')
                if tick + length > start + PERIOD:
                    break
                group.append((tick-start, length, pitch, velocity))
                index += 1
            key = tuple(group)
            if key not in lookup:
                lookup[key] = len(patterns)
                patterns.append(group)
            pattern = lookup[key]
            duration = max(n[0] + n[1] for n in group)
            clips.append((track, pattern, start, duration))
    if len(patterns) > 16 or len(clips) > 128:
        raise ValueError(f'Project capacity exceeded: {len(patterns)} patterns, {len(clips)} clips')
    # Round-trip through clip scheduling, not merely through the binary writer.
    restored = [[] for _ in tracks]
    for track, pattern, start, duration in clips:
        for tick, length, pitch, velocity in patterns[pattern]:
            if tick + length > duration:
                raise ValueError('Truncated packed note')
            restored[track].append((start+tick, length, pitch, velocity))
    if [sorted(t) for t in restored] != tracks:
        raise ValueError('Packing changed notes')
    return patterns, clips


def encode(entry, patterns, clips, end):
    def name(value):
        value = value.encode('utf-8')
        if len(value) >= 64:
            raise ValueError('Name exceeds project byte limit')
        return value.ljust(64, b'\0')
    def ints(*values):
        return struct.pack('<'+'i'*len(values), *values)
    def floats(*values):
        return struct.pack('<'+'f'*len(values), *values)
    tracks = entry['tracks']
    out = bytearray(b'YMUSIC02')
    out += name(entry['title']) + ints(entry['bpm'], entry['beats'], 0, end)
    out += floats(entry.get('master', .8)) + ints(len(tracks), len(patterns), len(clips))
    for i, track in enumerate(tracks):
        out += name(track['name']) + ints(1, 0, 0, track['instrument'])
        out += floats(track.get('volume', .85), track.get('pan', 0))
        out += ints([0x68b7a5, 0x778ed0, 0xc685a8, 0xd5a35d][i % 4])
    for _ in range(9):
        out += floats(.8, 0) + ints(0, 0, -1)
    for i, notes in enumerate(patterns):
        out += name(f'乐句 {i+1:02d}') + ints(1, 32, len(notes)) + bytes(9*32)
        for note in notes:
            out += ints(*note)
    for track, pattern, start, length in clips:
        out += ints(track, pattern, start, length, 0) + floats(1, 0, 0)
    out += ints(0, 0x1234abcd)  # no embedded PCM assets; format footer
    return bytes(out)


def build(output):
    manifest = json.loads((LIBRARY/'sources/manifest.json').read_text())
    for item in manifest:
        data = (LIBRARY/'sources'/item['file']).read_bytes()
        if hashlib.sha256(data).hexdigest() != item['sha256']:
            raise ValueError('Source checksum mismatch: '+item['file'])
    entries = json.loads((LIBRARY/'catalog.json').read_text())
    output.mkdir(parents=True, exist_ok=True)
    report = []
    for entry in entries:
        midi = read_midi(LIBRARY/'sources'/entry['source'])
        tracks, end = source_notes(midi, entry)
        patterns, clips = pack_patterns(tracks)
        data = encode(entry, patterns, clips, end)
        path = output/(entry['title']+'.ymmusic')
        # Never overwrite edits made to a supplied project in place.
        if path.exists() and path.read_bytes() != data:
            raise ValueError('Existing project differs; use a new output directory: '+str(path))
        path.write_bytes(data)
        row = dict(file=path.name, seconds=round(end/PPQ*60/entry['bpm'], 3),
                   notes=sum(map(len, tracks)), tracks=len(tracks), patterns=len(patterns), clips=len(clips),
                   sha256=hashlib.sha256(data).hexdigest(), source=entry['source'],
                   source_tempos=midi['tempos'], source_signatures=midi['signatures'],
                   source_events_not_rendered=midi['ignored'])
        report.append(row)
        print(f"{path.name}: {row['seconds']} s, {row['notes']} notes, {row['patterns']} patterns")
    (output/'conversion.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=LIBRARY/'projects')
    args = parser.parse_args()
    build(args.output)
