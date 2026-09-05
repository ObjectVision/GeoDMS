# Fixture generator for the storage-reader regression cases (doc/code-fixes.md Phase 0/1: STG-13, STG-14,
# STG-N02, STG-N04, STG-N01; RTC-01/03/04 XML entities). Pure struct-based writers: every file is a few
# hundred bytes and carries exactly the tags the case needs, which no library-based writer guarantees.
# Run: python testcases\data\make_fixtures.py   (rewrites the fixtures beside this script)
import struct, os, math

OUT = os.path.dirname(os.path.abspath(__file__))
os.makedirs(OUT, exist_ok=True)

# ---------------------------------------------------------------- TIFF (little-endian, one strip)
SHORT, LONG, DOUBLE = 3, 4, 12
SIZES = {SHORT: 2, LONG: 4, DOUBLE: 8}
FMTS = {SHORT: 'H', LONG: 'I', DOUBLE: 'd'}

def tiff(name, width, height, bps, data, sample_format=None, rows_per_strip='auto', extra=(), truncate=None, photometric=1):
    entries = [(256, LONG, [width]), (257, LONG, [height]), (259, SHORT, [1]), (262, SHORT, [photometric]),
               (273, LONG, [0]), (277, SHORT, [1]), (279, LONG, [len(data)])]
    if bps is not None:
        entries.append((258, SHORT, [bps]))
    if rows_per_strip == 'auto':
        entries.append((278, LONG, [height]))
    elif rows_per_strip is not None:
        entries.append((278, LONG, [rows_per_strip]))
    if sample_format is not None:
        entries.append((339, SHORT, [sample_format]))
    entries += list(extra)
    entries.sort(key=lambda e: e[0])
    n = len(entries)
    ifd_off = 8
    blob_off = ifd_off + 2 + 12 * n + 4
    blobs = b''
    laid = []
    for tag, typ, vals in entries:
        packed = struct.pack('<' + FMTS[typ] * len(vals), *vals)
        if len(packed) <= 4:
            laid.append((tag, typ, len(vals), packed.ljust(4, b'\0')))
        else:
            laid.append((tag, typ, len(vals), struct.pack('<I', blob_off + len(blobs))))
            blobs += packed
    data_off = blob_off + len(blobs)
    laid = [(t, ty, c, struct.pack('<I', data_off) if t == 273 else v) for (t, ty, c, v) in laid]
    body = b'II*\0' + struct.pack('<I', ifd_off) + struct.pack('<H', n)
    for t, ty, c, v in laid:
        body += struct.pack('<HHI', t, ty, c) + v
    body += struct.pack('<I', 0) + blobs + data
    if truncate is not None:
        body = body[:truncate]
    with open(os.path.join(OUT, name), 'wb') as f:
        f.write(body)
    print(name, len(body), 'bytes')

W, H = 8, 4
u8 = bytes(range(W * H))                                   # 0..31, sum 496
tiff('tif_u8_nosf_norps.tif', W, H, 8, u8, rows_per_strip=None)          # STG-13 (no RowsPerStrip) + STG-N04 (no SampleFormat)
bits = b'\xF0\xF0' + b'\x0F\x0F' + b'\xFF\x00' + b'\x00\xFF'             # 16x4, 32 of 64 bits set
tiff('tif_1bit_nobps.tif', 16, 4, None, bits, rows_per_strip=None)       # STG-13 (no BitsPerSample -> default 1)
i16 = struct.pack('<32h', *range(-16, 16))                                # sum -16
tiff('tif_i16_nosf.tif', W, H, 16, i16)                                   # STG-N04: int16 category taken from the configured type
f32 = struct.pack('<32f', *[i * 0.5 for i in range(32)])                  # sum 248
tiff('tif_f32_nosf.tif', W, H, 32, f32)                                   # STG-N04: float category taken from the configured type
tiff('tif_u8_sf.tif', W, H, 8, u8, sample_format=1)                       # control: SampleFormat present
c, s = 10 * math.cos(math.radians(30)), 10 * math.sin(math.radians(30))
mt = [c, s, 0, 1000.0, s, -c, 0, 2000.0, 0, 0, 0, 0, 0, 0, 0, 1]
tiff('tif_u8_rotated.tif', W, H, 8, u8, sample_format=1, extra=[(34264, DOUBLE, mt)])   # STG-N02: ModelTransformation, 16 doubles
tiff('tif_u8_truncated.tif', W, H, 8, u8, sample_format=1, truncate=None)              # placeholder, replaced below
# truncated strip: StripByteCounts says 32, the file ends after 20 data bytes
full = open(os.path.join(OUT, 'tif_u8_truncated.tif'), 'rb').read()
open(os.path.join(OUT, 'tif_u8_truncated.tif'), 'wb').write(full[:len(full) - 12])
print('tif_u8_truncated.tif', len(full) - 12, 'bytes (12 short)')

# ---------------------------------------------------------------- shapefiles (points and polylines) with a stale .shx
def shp_header(shape_type, file_len_bytes, bbox):
    h = struct.pack('>I', 9994) + b'\0' * 20 + struct.pack('>I', file_len_bytes // 2)
    h += struct.pack('<II', 1000, shape_type) + struct.pack('<4d', *bbox) + struct.pack('<4d', 0, 0, 0, 0)
    assert len(h) == 100
    return h

def write_shp(name, shape_type, records, bbox, shx_extra=0):
    contents = [rec for rec in records]
    body = b''
    index = b''
    off = 100
    for i, content in enumerate(contents):
        body += struct.pack('>II', i + 1, len(content) // 2) + content
        index += struct.pack('>II', off // 2, len(content) // 2)
        off += 8 + len(content)
    for j in range(shx_extra):                                   # entries the .shp does not hold
        index += struct.pack('>II', off // 2, len(contents[-1]) // 2)
        off += 8 + len(contents[-1])
    shp = shp_header(shape_type, 100 + len(body), bbox) + body
    shx = shp_header(shape_type, 100 + len(index), bbox) + index
    open(os.path.join(OUT, name + '.shp'), 'wb').write(shp)
    open(os.path.join(OUT, name + '.shx'), 'wb').write(shx)
    print(name, len(shp), 'shp bytes,', len(shx), 'shx bytes, records', len(contents), '+', shx_extra, 'stale')

def point_rec(x, y):
    return struct.pack('<I', 1) + struct.pack('<2d', x, y)

def arc_rec(pts):
    xs = [p[0] for p in pts]; ys = [p[1] for p in pts]
    r = struct.pack('<I', 3) + struct.pack('<4d', min(xs), min(ys), max(xs), max(ys))
    r += struct.pack('<II', 1, len(pts)) + struct.pack('<I', 0)
    for x, y in pts:
        r += struct.pack('<2d', x, y)
    return r

pts = [point_rec(1.0, 2.0), point_rec(3.0, 4.0), point_rec(5.0, 6.0)]
write_shp('shp_pts3', 1, pts, (1, 2, 5, 6))                       # control
write_shp('shp_pts3_stale', 1, pts, (1, 2, 5, 6), shx_extra=2)    # STG-N01: .shx declares 5, .shp holds 3
arcs = [arc_rec([(0, 0), (10, 0)]), arc_rec([(0, 5), (10, 5), (10, 15)])]
write_shp('shp_arc2', 3, arcs, (0, 0, 10, 15))                    # control
write_shp('shp_arc2_stale', 3, arcs, (0, 0, 10, 15), shx_extra=2) # STG-N01: .shx declares 4, .shp holds 2

# ---------------------------------------------------------------- XML configuration fragments (RTC-01/03/04)
def xml(name, text):
    with open(os.path.join(OUT, name), 'wb') as f:
        f.write(text.encode('latin-1'))
    print(name, len(text), 'bytes')

# The XML configuration grammar (rtc/dll/src/xml/XmlParser.cpp ReadAttr, FormattedInpStream::NextWord) is
# whitespace-tokenised: '<', '/', '?', '>', names, '=' and quoted values must be separated by spaces, a
# version header comes first, and entities are decoded in element TEXT only (ReadText), not in attribute
# values. A fragment that loads leaks its items at exit in a Debug build (see doc/code-fixes.md, XML
# finding of 2026-09-05), so only the two negatives are battery cases.
HEADER = '<? xml version = "1.0" ? >\n'
# exceeds MAX_TOKEN_LEN (32): a clean error; the element after it must then not exist
xml('xml_entity_long.xml', HEADER + '< TreeItem name = "xc" >\n< Descr > a &thisentitynameislongerthanthirtytwocharacters; b < / Descr >\n< / TreeItem >\n< TreeItem name = "xe" / >\n')
# unterminated entity at end of file: a clean error; the Descr never reaches the item
xml('xml_entity_eof.xml', HEADER + '< TreeItem name = "xd" >\n< Descr > a &amp')
