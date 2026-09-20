"""Curated MIDI conversion regressions; run with python3 tests/test_library.py."""
import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location('build_library', Path(__file__).resolve().parents[1]/'tools/build_library.py')
builder = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(builder)


def midi_file(events):
    return b'MThd'+struct.pack('>IHHH', 6, 0, 1, 96)+b'MTrk'+struct.pack('>I', len(events))+events


class LibraryTest(unittest.TestCase):
    def parse(self, data):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d)/'source.mid'
            p.write_bytes(data)
            return builder.read_midi(p)

    def test_running_status_and_zero_velocity_note_off(self):
        # C4 at tick 0, E4 at tick 48; both end at tick 96, running status 0x90.
        data = midi_file(bytes.fromhex('00 90 3c 64 30 40 50 30 3c 00 00 40 00 00 ff 2f 00'))
        self.assertEqual(self.parse(data)['notes'], [(0, 0, 0, 96, 60, 100), (0, 0, 48, 96, 64, 80)])

    def test_malformed_input_rejected(self):
        for data in [b'not-midi', midi_file(bytes.fromhex('00 90 3c 64 00 ff 2f 00')),
                     midi_file(bytes.fromhex('00 80 3c 00 00 ff 2f 00')),
                     midi_file(bytes.fromhex('00 90 3c 64'))[:-1]]:
            with self.subTest(data=data), self.assertRaises((ValueError, IndexError, struct.error)):
                self.parse(data)

    def test_whole_notes_crossing_nominal_phrase_boundary(self):
        notes = [[(0, 96, 60, 100), (700, 200, 64, 90), (720, 20, 67, 80), (1400, 180, 69, 90)]]
        patterns, clips = builder.pack_patterns(notes)
        restored = sorted((start+t, length, pitch, v) for _, p, start, _ in clips
                          for t, length, pitch, v in patterns[p])
        self.assertEqual(restored, notes[0])
        self.assertTrue(all(t+length <= builder.PERIOD for p in patterns for t, length, _, _ in p))

    def test_repeated_phrase_shares_pattern(self):
        notes = [[(0, 96, 60, 100), (768, 96, 60, 100)]]
        patterns, clips = builder.pack_patterns(notes)
        self.assertEqual(len(patterns), 1)
        self.assertEqual([c[2] for c in clips], [0, 768])
        with self.assertRaises(ValueError):
            builder.pack_patterns([[(0, 769, 60, 100)]])
        with self.assertRaises(ValueError):
            builder.pack_patterns([[(i*768, 96, 40+i, 100) for i in range(17)]])

    def test_catalog_regenerates_and_never_overwrites_edits(self):
        with tempfile.TemporaryDirectory() as d:
            output = Path(d)
            builder.build(output)
            projects = sorted(output.glob('*.ymmusic'))
            self.assertEqual(len(projects), 7)
            for p in projects:
                self.assertEqual(p.read_bytes(), (builder.LIBRARY/'projects'/p.name).read_bytes())
                self.assertEqual(p.read_bytes()[:8], b'YMUSIC02')
                self.assertEqual(p.read_bytes()[-4:], bytes.fromhex('cd ab 34 12'))
            projects[0].write_bytes(b'user edited project')
            with self.assertRaises(ValueError):
                builder.build(output)
            self.assertEqual(projects[0].read_bytes(), b'user edited project')

    def test_timing_scale_preserves_seconds(self):
        rows = json.loads((builder.LIBRARY/'catalog.json').read_text())
        entry = rows[0]
        midi = builder.read_midi(builder.LIBRARY/'sources'/entry['source'])
        tracks, end = builder.source_notes(midi, entry)
        self.assertAlmostEqual(end/96*60/entry['bpm'], 28.125)
        self.assertEqual(tracks[0][0][:3], (0, 48, 76))


if __name__ == '__main__':
    unittest.main()
