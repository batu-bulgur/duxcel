#!/usr/bin/env python3
"""Verify frozen C pixel fixtures against existing captures; no renderer launch."""
import argparse
import hashlib
import json
import re
import struct
import zlib
from pathlib import Path

REFERENCE_SHA = "1c8f3a6ff4dc532598a580abe44f369848cd00107a2a93a69e9289071440f876"
CASES = (("narrow-100", 1, 220), ("narrow-150", 1.5, 304))


def decode_png(path):
    data = path.read_bytes()
    assert data[:8] == b"\x89PNG\r\n\x1a\n"
    at, compressed, dimensions = 8, bytearray(), None
    while at < len(data):
        length = struct.unpack_from(">I", data, at)[0]
        tag, part = data[at + 4:at + 8], data[at + 8:at + 8 + length]
        assert at + length + 12 <= len(data)
        crc = struct.unpack_from(">I", data, at + 8 + length)[0]
        assert zlib.crc32(tag + part) & 0xFFFFFFFF == crc
        at += length + 12
        if tag == b"IHDR":
            width, height, depth, color, compression, filtering, interlace = struct.unpack(">IIBBBBB", part)
            assert depth == 8 and color in (2, 6) and compression == filtering == interlace == 0
            channels = 4 if color == 6 else 3
            assert 0 < width <= 8192 and 0 < height <= 8192 and width * height * channels <= 64 * 1024 * 1024
            dimensions = width, height, channels
        elif tag == b"IDAT":
            compressed.extend(part)
        elif tag == b"IEND":
            assert not part and at == len(data)
            break
    assert dimensions is not None
    width, height, channels = dimensions
    stride = width * channels
    raw = zlib.decompress(compressed)
    assert len(raw) == height * (stride + 1)
    prior, rows, at = bytearray(stride), [], 0
    for _ in range(height):
        filtering, row = raw[at], bytearray(raw[at + 1:at + 1 + stride])
        at += stride + 1
        assert filtering in range(5)
        for x in range(stride):
            a, b, c = row[x - channels] if x >= channels else 0, prior[x], prior[x - channels] if x >= channels else 0
            if filtering == 1:
                row[x] = (row[x] + a) & 255
            elif filtering == 2:
                row[x] = (row[x] + b) & 255
            elif filtering == 3:
                row[x] = (row[x] + (a + b) // 2) & 255
            elif filtering == 4:
                prediction = a + b - c
                da, db, dc = abs(prediction - a), abs(prediction - b), abs(prediction - c)
                nearest = a if da <= db and da <= dc else b if db <= dc else c
                row[x] = (row[x] + nearest) & 255
        rows.append(row)
        prior = row

    def pixel(x, y):
        assert 0 <= x < width and 0 <= y < height
        value = list(rows[y][x * channels:(x + 1) * channels])
        return value if channels == 4 else value + [255]

    return width, height, pixel


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference-dir", type=Path, required=True, help="Existing 20261018T030846-reference-rail-1269 capture directory")
    args = parser.parse_args()
    reference_bytes = (args.reference_dir / "reference.json").read_bytes()
    assert hashlib.sha256(reference_bytes).hexdigest() == REFERENCE_SHA
    reference = json.loads(reference_bytes)
    assert reference["status"] == "passed" and reference["checks"] == 427 and reference["dpr"] == 2
    header = Path(__file__).with_name("shadow_reference.h").read_text()
    assert REFERENCE_SHA in header
    points = 0
    for name, scale, logical_y in CASES:
        records = [next(item for item in reference["states"] if item["case"] == name and item["name"] == phase) for phase in ("expanded", "dismissed")]
        images = []
        for record in records:
            capture = record["capture"]
            png = args.reference_dir / capture["file"]
            assert png.stat().st_size == capture["bytes"]
            assert hashlib.sha256(png.read_bytes()).hexdigest() == capture["sha256"]
            assert capture["sha256"] in header and record["validated"]
            width, height, pixel = decode_png(png)
            assert width == 1280 and height == (1440 if scale == 1 else 1280)
            images.append(pixel)
        panel = records[0]["state"]["wrapper"]["rect"]
        assert records[0]["state"]["scale"] == scale
        first, last, y = int((panel["x"] - 48 * scale) * 2), int(panel["x"] * 2), logical_y * 2
        expected = [[images[0](x, y), images[1](x, y)] for x in range(first, last)]
        match = re.search(r"static const ShadowReferencePixel " + name.replace("-", "_") + r"\[\] = \{(.*?)\n\};", header, re.S)
        assert match, name
        actual = [[list(map(int, a.split(","))), list(map(int, b.split(",")))] for a, b in re.findall(r"\{\{([0-9,]+)\},\{([0-9,]+)\}\}", match[1])]
        assert actual == expected, name + " fixture differs from frozen RGBA samples"
        points += len(actual)
    assert points == 240
    print(f"PASS: {points} exact expanded/dismissed sample pairs; 4 PNG hashes and original capture SHA256 verified; no browser/GPU launch.")


if __name__ == "__main__":
    main()
