#!/usr/bin/env python3
"""Validate the versioned external dictionaries used by chinese_ime."""

import argparse
import struct
from pathlib import Path


HEADER_SIZE = 28 + 27 * 4


def read_layout(path, magic, count, record_size):
    data = Path(path).read_bytes()
    if len(data) < HEADER_SIZE:
        raise ValueError(f"{path}: shorter than its indexed header")
    actual = struct.unpack_from("<8sIIIII", data)
    actual_magic, actual_count, blob_size, header_size, actual_record_size, bucket_count = actual
    records_end = header_size + actual_count * actual_record_size
    if (actual_magic != magic or actual_count != count or header_size != HEADER_SIZE or
            actual_record_size != record_size or bucket_count != 26 or
            records_end + blob_size != len(data)):
        raise ValueError(f"{path}: header/count/size mismatch")
    buckets = struct.unpack_from("<27I", data, 28)
    if (buckets[0] != 0 or buckets[-1] != count or
            any(buckets[i] > buckets[i + 1] for i in range(26))):
        raise ValueError(f"{path}: bucket offsets are invalid")
    return data, buckets, data[records_end:], header_size


def read_string(blob, offset, path, index):
    if offset >= len(blob):
        raise ValueError(f"{path}: invalid string offset at record {index}")
    end = blob.find(b"\0", offset)
    if end < 0:
        raise ValueError(f"{path}: unterminated string at record {index}")
    return blob[offset:end]


def verify_phrases(path, count):
    data, buckets, blob, header_size = read_layout(path, b"YMIMEP2\0", count, 16)
    found_gada = False
    found_nima = False
    for index in range(count):
        offsets = struct.unpack_from("<III", data, header_size + index * 16)
        pinyin, _, word = (read_string(blob, offset, path, index) for offset in offsets)
        letter = pinyin[0] - ord("a")
        if letter < 0 or letter >= 26 or not buckets[letter] <= index < buckets[letter + 1]:
            raise ValueError(f"{path}: record {index} is outside its letter bucket")
        found_gada |= pinyin == b"ga'da" and word == "嘎达".encode()
        found_nima |= pinyin == b"ni'ma" and word == "尼玛".encode()
    if not found_gada:
        raise ValueError(f"{path}: missing gada -> 嘎达 user dictionary entry")
    if not found_nima:
        raise ValueError(f"{path}: missing nima -> 尼玛 user dictionary entry")
    print(f"IME phrase BIN: OK ({count} phrases, 26 buckets, {len(data)} bytes)")


def verify_chars(path, count):
    data, buckets, blob, header_size = read_layout(path, b"YMIMEC1\0", count, 12)
    found_zeng = False
    for index in range(count):
        py_offset, char_offset, _ = struct.unpack_from("<III", data, header_size + index * 12)
        pinyin = read_string(blob, py_offset, path, index)
        char = read_string(blob, char_offset, path, index)
        letter = pinyin[0] - ord("a")
        if letter < 0 or letter >= 26 or not buckets[letter] <= index < buckets[letter + 1]:
            raise ValueError(f"{path}: record {index} is outside its letter bucket")
        found_zeng |= pinyin == b"zeng" and char == "曾".encode()
    if not found_zeng:
        raise ValueError(f"{path}: missing zeng -> 曾 regression entry")
    print(f"IME character BIN: OK ({count} pronunciations, 26 buckets, {len(data)} bytes)")


def verify_english(path, count):
    data, buckets, blob, header_size = read_layout(path, b"YMIMEE1\0", count, 8)
    first_th = None
    for index in range(count):
        offset, _ = struct.unpack_from("<II", data, header_size + index * 8)
        word = read_string(blob, offset, path, index)
        if not word.isalpha() or not word.islower() or not word.isascii():
            raise ValueError(f"{path}: non-lowercase-ASCII word at record {index}")
        letter = word[0] - ord("a")
        if letter < 0 or letter >= 26 or not buckets[letter] <= index < buckets[letter + 1]:
            raise ValueError(f"{path}: record {index} is outside its letter bucket")
        if first_th is None and word.startswith(b"th"):
            first_th = word
    if first_th != b"the":
        raise ValueError(f"{path}: frequency order regression, first 'th' word is {first_th!r}")
    print(f"IME English BIN: OK ({count} words, 26 buckets, {len(data)} bytes)")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--phrase", default="project_Demo/chinese_ime/phrases_rime.bin")
    parser.add_argument("--phrase-count", type=int, default=47280)
    parser.add_argument("--chars", default="project_Demo/chinese_ime/pinyin_gb2312.bin")
    parser.add_argument("--char-count", type=int, default=7291)
    parser.add_argument("--english", default="project_Demo/chinese_ime/english_words.bin")
    parser.add_argument("--english-count", type=int, default=288996)
    args = parser.parse_args()
    try:
        verify_phrases(args.phrase, args.phrase_count)
        verify_chars(args.chars, args.char_count)
        verify_english(args.english, args.english_count)
    except ValueError as error:
        raise SystemExit(str(error)) from error


if __name__ == "__main__":
    main()
