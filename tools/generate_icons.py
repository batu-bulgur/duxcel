#!/usr/bin/env python3
"""Offline SVG -> shared C mesh, RGBA PNG, ICNS, ICO and hicolor assets.

Python standard library only. No tracing service, installed rasterizer or network.
The deliberately small SVG subset is validated rather than silently approximated.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import xml.etree.ElementTree as ET
import zlib

ROOT = Path(__file__).resolve().parents[1]
SOURCE = Path("assets/icons/duxcel.svg")
MANIFEST = Path("assets/icons/manifest.json")
SIZES = (16, 20, 24, 30, 32, 36, 40, 48, 60, 64, 72, 80, 96, 128, 160, 192, 256, 320, 384, 512, 640, 768, 1024)
MODES = {None: 0, "x": 1, "y": 2, "eye": 3, "neck": 4, "body": 5}


def cross(a, b, c):
    return (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])


def polygon_path(text):
    tokens = re.findall(r"[MLHVCQZ]|[-+]?(?:\d*\.\d+|\d+)(?:[eE][-+]?\d+)?", text)
    if re.sub(r"[\s,]", "", text) != "".join(tokens):
        raise ValueError("Only absolute M/L/H/V/C/Q/Z paths are supported")
    points, pos, command = [], 0, None
    while pos < len(tokens):
        if tokens[pos].isalpha():
            command = tokens[pos]
            pos += 1
        if command == "Z":
            if pos != len(tokens):
                raise ValueError("One closed contour per path is required")
            break
        count = {"M": 2, "L": 2, "H": 1, "V": 1, "C": 6, "Q": 4}[command]
        values = list(map(float, tokens[pos:pos+count]))
        if len(values) != count:
            raise ValueError("Incomplete path")
        pos += count
        if command in ("M", "L"):
            points.append(tuple(values))
            command = "L"
        elif command == "H":
            points.append((values[0], points[-1][1]))
        elif command == "V":
            points.append((points[-1][0], values[0]))
        else:
            start = points[-1]
            for step in range(1, 33):
                t, u = step/32, 1-step/32
                if command == "Q":
                    points.append(tuple(u*u*start[d]+2*u*t*values[d]+t*t*values[d+2] for d in range(2)))
                else:
                    points.append(tuple(u**3*start[d]+3*u*u*t*values[d]+3*u*t*t*values[d+2]+t**3*values[d+4] for d in range(2)))
    return points


def clean(points):
    result = []
    for point in points:
        if not result or math.dist(point, result[-1]) > 1e-7:
            result.append(point)
    if len(result) > 1 and math.dist(result[0], result[-1]) < 1e-7:
        result.pop()
    changed = True
    while changed and len(result) > 3:
        changed = False
        for i, point in enumerate(result):
            if abs(cross(result[i-1], point, result[(i+1) % len(result)])) < 1e-9:
                result.pop(i)
                changed = True
                break
    return result


def triangulate(points):
    area = sum(a[0]*b[1]-b[0]*a[1] for a, b in zip(points, points[1:]+points[:1]))
    remaining = list(range(len(points)))
    if area < 0:
        remaining.reverse()
    triangles = []
    while len(remaining) > 3:
        for at, middle in enumerate(remaining):
            before, after = remaining[at-1], remaining[(at+1) % len(remaining)]
            a, b, c = points[before], points[middle], points[after]
            if cross(a, b, c) <= 1e-9:
                continue
            if any(cross(a, b, points[i]) >= -1e-9 and cross(b, c, points[i]) >= -1e-9 and cross(c, a, points[i]) >= -1e-9 for i in remaining if i not in (before, middle, after)):
                continue
            triangles.append((before, middle, after))
            remaining.pop(at)
            break
        else:
            raise ValueError("Degenerate or intersecting contour")
    triangles.append(tuple(remaining))
    return triangles


def read_shapes():
    root = ET.parse(ROOT/SOURCE).getroot()
    if root.attrib.get("viewBox") != "0 0 256 256":
        raise ValueError("Expected the normalized 256-unit viewBox")
    shapes = []
    for element in root:
        tag = element.tag.rsplit("}", 1)[-1]
        attrs = element.attrib
        if tag in ("title", "desc"):
            continue
        if set(attrs)-{"fill", "d", "x", "y", "width", "height", "rx", "cx", "cy", "r", "data-optical"}:
            raise ValueError("Unsupported SVG attribute")
        if tag == "path":
            points = polygon_path(attrs["d"])
        elif tag == "circle":
            x, y, radius = (float(attrs[key]) for key in ("cx", "cy", "r"))
            points = [(x+radius*math.cos(t*math.tau/64), y+radius*math.sin(t*math.tau/64)) for t in range(64)]
        elif tag == "rect":
            x, y, w, h = (float(attrs[key]) for key in ("x", "y", "width", "height"))
            radius = float(attrs.get("rx", 0))
            if not radius:
                points = [(x, y), (x+w, y), (x+w, y+h), (x, y+h)]
            else:
                points = []
                for cx, cy, start in ((x+w-radius, y+radius, -90), (x+w-radius, y+h-radius, 0), (x+radius, y+h-radius, 90), (x+radius, y+radius, 180)):
                    for step in range(17):
                        angle = math.radians(start+90*step/16)
                        points.append((cx+radius*math.cos(angle), cy+radius*math.sin(angle)))
        else:
            raise ValueError(f"Unsupported vector element: {tag}")
        points = clean(points)
        if any(not math.isfinite(v) or v < 0 or v > 256 for point in points for v in point):
            raise ValueError("Geometry outside the icon viewBox")
        color = attrs["fill"]
        if not re.fullmatch(r"#[0-9a-fA-F]{6}", color):
            raise ValueError("Use explicit opaque RGB fills")
        bounds = (min(p[0] for p in points), min(p[1] for p in points), max(p[0] for p in points), max(p[1] for p in points))
        shapes.append(dict(points=points, triangles=triangulate(points), color=tuple(bytes.fromhex(color[1:]))+(255,), bounds=bounds, mode=MODES[attrs.get("data-optical")]))
    return shapes


def transform(shape, pixels):
    x0, y0, x1, y1 = shape["bounds"]
    sx = sy = 1.0
    tx = ty = 0.0
    mode = shape["mode"]
    if mode in (1, 2, 3):
        minimum = (0.9 if mode == 3 else 0.8)*256/pixels
        if mode in (1, 3):
            sx = max(1.0, minimum/(x1-x0))
            tx = (x0+x1)*0.5*(1-sx)
        if mode in (2, 3):
            sy = max(1.0, minimum/(y1-y0))
            ty = (y0+y1)*0.5*(1-sy)
    if mode in (4, 5):
        extra = max(0.0, 0.7*256/pixels-4)
        top, bottom = (extra, extra*0.5) if mode == 4 else (extra*0.5, 0)
        sy = (y1-y0-top-bottom)/(y1-y0)
        ty = y0+top-y0*sy
    return sx, sy, tx, ty


def raster(shapes, size):
    samples = 8 if size <= 64 else 4
    side = size*samples
    scale = side/256
    canvas = bytearray(side*side*4)
    for shape in shapes:
        sx, sy, tx, ty = transform(shape, size)
        points = [((x*sx+tx)*scale, (y*sy+ty)*scale) for x, y in shape["points"]]
        first = max(0, math.ceil(min(y for _, y in points)-0.5))
        last = min(side, math.ceil(max(y for _, y in points)-0.5))
        edges = list(zip(points, points[1:]+points[:1]))
        pixel = bytes(shape["color"])
        for row in range(first, last):
            y = row+0.5
            cuts = sorted(a[0]+(y-a[1])*(b[0]-a[0])/(b[1]-a[1]) for a, b in edges if (a[1] <= y < b[1]) or (b[1] <= y < a[1]))
            if len(cuts) % 2:
                raise ValueError("Unpaired scanline intersections")
            for left, right in zip(cuts[::2], cuts[1::2]):
                lo, hi = max(0, math.ceil(left-0.5)), min(side, math.ceil(right-0.5))
                if hi > lo:
                    start = (row*side+lo)*4
                    canvas[start:start+(hi-lo)*4] = pixel*(hi-lo)
    result = bytearray(size*size*4)
    for row in range(size):
        for col in range(size):
            sums = [0, 0, 0, 0]
            for dy in range(samples):
                at = ((row*samples+dy)*side+col*samples)*4
                for dx in range(samples):
                    pixel = at+dx*4
                    if canvas[pixel+3]:
                        for c in range(4):
                            sums[c] += canvas[pixel+c]
            at = (row*size+col)*4
            alpha = sums[3]
            if alpha:
                # Straight-alpha output: average premultiplied coverage, then unpremultiply.
                result[at:at+4] = bytes([round(sums[c]*255/alpha) for c in range(3)]+[round(alpha/(samples*samples))])
    return result


def png(size, rgba):
    def chunk(kind, data):
        return struct.pack(">I", len(data))+kind+data+struct.pack(">I", zlib.crc32(kind+data))
    rows = b"".join(b"\0"+rgba[y*size*4:(y+1)*size*4] for y in range(size))
    return b"\x89PNG\r\n\x1a\n"+chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0))+chunk(b"sRGB", b"\0")+chunk(b"IDAT", zlib.compress(rows, 9))+chunk(b"IEND", b"")


def c_float(value):
    return f"{value:.7f}f"


def geometry_header(shapes):
    lines = ["/* Generated by tools/generate_icons.py from assets/icons/duxcel.svg. */", "#ifndef DUXCEL_BRAND_GEOMETRY_H", "#define DUXCEL_BRAND_GEOMETRY_H", "#include <math.h>", "#include <stddef.h>", f"#define BRAND_MAX_POINTS {max(len(s['points']) for s in shapes)}", "typedef struct { float x,y; } BrandPoint;", "typedef struct { size_t vertex,count,index,indices; unsigned char color[4],mode; float x0,y0,x1,y1; } BrandShape;", "typedef struct { float sx,sy,tx,ty; } BrandTransform;", "static const BrandPoint brand_vertices[]={"]
    vertices, indices, records = [], [], []
    for shape in shapes:
        records.append((len(vertices), len(shape["points"]), len(indices), len(shape["triangles"])*3, shape))
        vertices.extend(shape["points"])
        indices.extend(v for tri in shape["triangles"] for v in tri)
    lines.extend("    {"+c_float(x)+","+c_float(y)+"}," for x, y in vertices)
    lines.append("};\nstatic const unsigned short brand_indices[]={")
    lines.extend("    "+",".join(map(str, indices[i:i+24]))+"," for i in range(0, len(indices), 24))
    lines.append("};\nstatic const BrandShape brand_shapes[]={")
    for first, count, index, length, shape in records:
        lines.append("    {"+f"{first},{count},{index},{length},"+"{"+",".join(map(str, shape["color"]))+"},"+str(shape["mode"])+","+",".join(map(c_float, shape["bounds"]))+"},")
    lines.append("};")
    lines.append(r"""static inline BrandTransform brand_transform(const BrandShape *s,float pixels) {
    BrandTransform t={1,1,0,0}; pixels=fmaxf(8,pixels);
    if (s->mode>=1 && s->mode<=3) {
        float minimum=(s->mode==3 ? .9f : .8f)*256/pixels;
        if (s->mode==1 || s->mode==3) { t.sx=fmaxf(1,minimum/(s->x1-s->x0)); t.tx=(s->x0+s->x1)*.5f*(1-t.sx); }
        if (s->mode==2 || s->mode==3) { t.sy=fmaxf(1,minimum/(s->y1-s->y0)); t.ty=(s->y0+s->y1)*.5f*(1-t.sy); }
    }
    if (s->mode==4 || s->mode==5) {
        float extra=fmaxf(0,.7f*256/pixels-4),top=s->mode==4 ? extra : extra*.5f,bottom=s->mode==4 ? extra*.5f : 0;
        t.sy=(s->y1-s->y0-top-bottom)/(s->y1-s->y0); t.ty=s->y0+top-s->y0*t.sy;
    }
    return t;
}
#endif
""")
    return "\n".join(lines).encode()


def build():
    shapes = read_shapes()
    output = {Path("src/ui/brand_geometry.h"): geometry_header(shapes)}
    images = {}
    for size in SIZES:
        print(f"Rendering {size}px", flush=True)
        images[size] = png(size, raster(shapes, size))
        output[Path(f"assets/icons/png/duxcel-{size}.png")] = images[size]
    icns = b""
    for kind, size in ((b"icp4",16),(b"icp5",32),(b"icp6",64),(b"ic07",128),(b"ic08",256),(b"ic09",512),(b"ic10",1024),(b"ic11",32),(b"ic12",64),(b"ic13",256),(b"ic14",512)):
        icns += kind+struct.pack(">I", len(images[size])+8)+images[size]
    output[Path("assets/icons/duxcel.icns")] = b"icns"+struct.pack(">I", len(icns)+8)+icns
    sizes = [s for s in SIZES if s <= 256]
    offset = 6+16*len(sizes)
    directory, payload = b"", b""
    for size in sizes:
        data = images[size]
        directory += struct.pack("<BBBBHHII", size % 256, size % 256, 0, 0, 1, 32, len(data), offset)
        payload += data
        offset += len(data)
    output[Path("assets/icons/duxcel.ico")] = struct.pack("<HHH", 0, 1, len(sizes))+directory+payload
    embedded = ["/* Generated OS icon assets; decoded once at startup, never per frame. */", "#ifndef DUXCEL_BRAND_IMAGES_H", "#define DUXCEL_BRAND_IMAGES_H", "#ifdef __APPLE__", "static const unsigned char brand_icns[]={"]
    data = output[Path("assets/icons/duxcel.icns")]
    embedded.extend("    "+",".join(str(b) for b in data[i:i+24])+"," for i in range(0, len(data), 24))
    embedded.append("};\n#else")
    for size in (16, 32, 64, 128, 256, 512):
        data = images[size]
        embedded.append(f"static const unsigned char brand_png_{size}[]={{")
        embedded.extend("    "+",".join(str(b) for b in data[i:i+24])+"," for i in range(0, len(data), 24))
        embedded.append("};")
    embedded.append("#endif\n#endif\n")
    output[Path("src/platform/brand_images.h")] = "\n".join(embedded).encode()
    data = images[512]
    bitmap = ["/* Generated display cache; the canonical source remains vector. */", "#ifndef DUXCEL_BRAND_BITMAP_H", "#define DUXCEL_BRAND_BITMAP_H", "static const unsigned char brand_bitmap[]={"]
    bitmap.extend("    "+",".join(str(b) for b in data[i:i+24])+"," for i in range(0, len(data), 24))
    bitmap.append("};\n#endif\n")
    output[Path("src/ui/brand_bitmap.h")] = "\n".join(bitmap).encode()
    for size in (16, 24, 32, 48, 64, 96, 128, 256, 512):
        output[Path(f"packaging/linux/icons/hicolor/{size}x{size}/apps/dev.duxcel.desktop.png")] = images[size]
    for size in (16, 24, 32, 48, 64, 128, 256, 512):
        output[Path(f"packaging/linux/icons/hicolor/{size}x{size}@2/apps/dev.duxcel.desktop.png")] = images[size*2]
    output[Path("packaging/linux/icons/hicolor/scalable/apps/dev.duxcel.desktop.svg")] = (ROOT/SOURCE).read_bytes()
    for size in (16, 32, 128, 256, 512):
        output[Path(f"assets/icons/duxcel.iconset/icon_{size}x{size}.png")] = images[size]
        output[Path(f"assets/icons/duxcel.iconset/icon_{size}x{size}@2x.png")] = images[size*2]
    return output


def digest(data):
    return hashlib.sha256(data).hexdigest()


def validate_png(data, size):
    if not data.startswith(b"\x89PNG\r\n\x1a\n"):
        raise ValueError("Invalid PNG signature")
    at, compressed, srgb = 8, b"", False
    while at < len(data):
        length = struct.unpack_from(">I", data, at)[0]
        kind, content = data[at+4:at+8], data[at+8:at+8+length]
        checksum = struct.unpack_from(">I", data, at+8+length)[0]
        if len(content) != length or zlib.crc32(kind+content) != checksum:
            raise ValueError("Corrupt PNG chunk")
        if kind == b"IHDR" and content != struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0):
            raise ValueError("Incorrect PNG dimensions or RGBA format")
        if kind == b"IDAT":
            compressed += content
        if kind == b"sRGB":
            srgb = content == b"\0"
        at += length+12
    raw = zlib.decompress(compressed)
    if at != len(data) or len(raw) != size*(1+size*4) or not srgb:
        raise ValueError("Invalid PNG payload or color space")
    rows = [raw[y*(1+size*4)+1:(y+1)*(1+size*4)] for y in range(size)]
    if any(raw[y*(1+size*4)] for y in range(size)):
        raise ValueError("Unexpected PNG filter")
    alpha = b"".join(row[3::4] for row in rows)
    if min(alpha) != 0 or max(alpha) != 255 or sum(a == 255 for a in alpha) < size*size*.4:
        raise ValueError("Missing transparent surround or opaque symbol")
    if any(rows[y][x*4:x*4+4] != b"\0\0\0\0" for y in (0, size-1) for x in (0, size-1)):
        raise ValueError("Icon surround contains a presentation background")


def validate_containers():
    data = (ROOT/"assets/icons/duxcel.ico").read_bytes()
    reserved, kind, count = struct.unpack_from("<HHH", data)
    if reserved != 0 or kind != 1 or count != len([s for s in SIZES if s <= 256]):
        raise ValueError("Invalid multiresolution ICO directory")
    for i in range(count):
        w, h, colors, reserved, planes, bits, length, offset = struct.unpack_from("<BBBBHHII", data, 6+i*16)
        size = w or 256
        if (h or 256) != size or colors or reserved or planes != 1 or bits != 32 or data[offset:offset+length] != (ROOT/f"assets/icons/png/duxcel-{size}.png").read_bytes():
            raise ValueError("ICO representation differs from the shared PNG")
    data = (ROOT/"assets/icons/duxcel.icns").read_bytes()
    if data[:4] != b"icns" or struct.unpack_from(">I", data, 4)[0] != len(data):
        raise ValueError("Invalid ICNS header")
    expected = {b"icp4":16,b"icp5":32,b"icp6":64,b"ic07":128,b"ic08":256,b"ic09":512,b"ic10":1024,b"ic11":32,b"ic12":64,b"ic13":256,b"ic14":512}
    at, found = 8, set()
    while at < len(data):
        kind, length = data[at:at+4], struct.unpack_from(">I", data, at+4)[0]
        if kind in found or kind not in expected or length <= 8 or data[at+8:at+length] != (ROOT/f"assets/icons/png/duxcel-{expected[kind]}.png").read_bytes():
            raise ValueError("ICNS representation differs from the shared PNG")
        found.add(kind)
        at += length
    if at != len(data) or found != set(expected):
        raise ValueError("Incomplete ICNS size/Retina representations")


def check():
    metadata = json.loads((ROOT/MANIFEST).read_text())
    for name, expected in metadata["sha256"].items():
        path = ROOT/name
        if not path.is_file() or digest(path.read_bytes()) != expected:
            raise ValueError(f"Missing or stale icon derivative: {name}; run make icons")
    shapes = read_shapes()
    if (ROOT/"src/ui/brand_geometry.h").read_bytes() != geometry_header(shapes):
        raise ValueError("C geometry does not match the canonical SVG")
    for size in SIZES:
        validate_png((ROOT/f"assets/icons/png/duxcel-{size}.png").read_bytes(), size)
    validate_containers()
    print(f"PASS: canonical vector and {len(metadata['sha256'])-2} derivatives match their hashes")
    print(f"PASS: {len(SIZES)} sRGB RGBA sizes, transparent surround, ICNS 1x/2x and ICO representations")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--verify", action="store_true", help="Regenerate in memory and compare every derivative byte")
    args = parser.parse_args()
    if args.check:
        check()
        return
    output = build()
    if args.verify:
        for name, data in output.items():
            if (ROOT/name).read_bytes() != data:
                raise ValueError(f"Non-reproducible derivative: {name}")
        check()
        print("PASS: every derivative regenerates byte-for-byte offline")
        return
    for name, data in output.items():
        path = ROOT/name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    inputs = {SOURCE: (ROOT/SOURCE).read_bytes(), Path("tools/generate_icons.py"): Path(__file__).read_bytes()}
    metadata = {"source": str(SOURCE), "sizes": SIZES, "sha256": {str(name): digest(data) for name, data in sorted((inputs | output).items())}}
    (ROOT/MANIFEST).write_text(json.dumps(metadata, indent=2)+"\n")
    check()


if __name__ == "__main__":
    main()
