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

def tiff(name, width, height, bps, data, sample_format=None, rows_per_strip='auto', extra=(), truncate=None, photometric=1, spp=1):
    entries = [(256, LONG, [width]), (257, LONG, [height]), (259, SHORT, [1]), (262, SHORT, [photometric]),
               (273, LONG, [0]), (277, SHORT, [spp]), (279, LONG, [len(data)])]
    if bps is not None:
        entries.append((258, SHORT, [bps] * spp))
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
# STG-A17: a palette image (Photometric 3) with a ColorMap of 256 entries, all reds, then all greens, then all
# blues, in 16 bits: colour k is (k, 2k mod 256, 3k mod 256), which GDAL reads back as 8 bits (v * 257 / 257)
cmap = [((k * f) % 256) * 257 for f in (1, 2, 3) for k in range(256)]
tiff('tif_u8_palette.tif', W, H, 8, u8, sample_format=1, extra=[(320, SHORT, cmap)], photometric=3)
tiff('tif_u8_truncated.tif', W, H, 8, u8, sample_format=1, truncate=None)              # placeholder, replaced below
# STG-A12: the depths that the tif read expands, in images of 5 x 3, an odd width that is no multiple of 8:
# - RGB, three samples of 8 bits, pixel k is (17k, k + 100, 255 - k), read into uint32 as r + 256 g + 65536 b
rgb = bytes(v for k in range(15) for v in (17 * k, k + 100, 255 - k))
tiff('tif_rgb24.tif', 5, 3, 8, rgb, photometric=2, spp=3)
# - a palette image of 4 bits, pixel k is 15 - k; TIFF puts the first pixel of a byte in its high nibble, and
#   pads each row of 5 pixels to 3 bytes
u4 = [15 - k for k in range(15)]
u4rows = [u4[r * 5:r * 5 + 5] + [0] for r in range(3)]
u4data = bytes((row[i] << 4) | row[i + 1] for row in u4rows for i in range(0, 6, 2))
cmap16 = [((k * f) % 16) * 4369 for f in (1, 2, 3) for k in range(16)]
tiff('tif_u4_palette.tif', 5, 3, 4, u4data, extra=[(320, SHORT, cmap16)], photometric=3)
# - a mask of 8 bits, pixel k is 0, 200 or 1 for k mod 3; read into bool, every value but 0 is true (not 255, which
#   is null in the uint8 read that the cases take as control)
mask = bytes((0, 200, 1)[k % 3] for k in range(15))
tiff('tif_u8_mask.tif', 5, 3, 8, mask)
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
null_rec = struct.pack('<I', 0)                                  # a null shape record: the shape type 0 only, 4 bytes
write_shp('shp_pts3_null', 1, [pts[0], null_rec, pts[2]], (1, 2, 5, 6))   # STG-A20: unreadable as a whole until 20.22.1
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
# values.
HEADER = '<? xml version = "1.0" ? >\n'
# exceeds MAX_TOKEN_LEN (32): a clean error; the element after it must then not exist
xml('xml_entity_long.xml', HEADER + '< TreeItem name = "xc" >\n< Descr > a &thisentitynameislongerthanthirtytwocharacters; b < / Descr >\n< / TreeItem >\n< TreeItem name = "xe" / >\n')
# unterminated entity at end of file: a clean error; the Descr never reaches the item
xml('xml_entity_eof.xml', HEADER + '< TreeItem name = "xd" >\n< Descr > a &amp')
# RTC-03 positive: a fragment that loads completely, with a nested item element and an entity decoded
# into each Descr. Not a battery case until #1254 fixed the parse-context assertion that killed such
# a load in Debug. Every entity sits at the END of its text, which is why this case was unaffected by
# #1260 (the character right after a ';' was eaten): there that character is the trailing space,
# which ReadText drops anyway. xml_entity_text.xml below is the case for #1260 itself.
xml('xml_entity.xml', HEADER + '< TreeItem name = "xb" >\n< Descr > a &amp; < / Descr >\n< TreeItem name = "xc" >\n< Descr > b &lt; < / Descr >\n< / TreeItem >\n< / TreeItem >\n')
# entity followed by text, on both sides of a decoded character: the whole point of the case is the
# character right after the ';', which used to be swallowed
xml('xml_entity_text.xml', HEADER + '< TreeItem name = "xt" >\n< Descr > a &amp; b &lt;c&gt; < / Descr >\n< / TreeItem >\n')
# INF-A05: numeric references (decimal, hex, one of two UTF-8 bytes) and an unknown entity in element text; each
# gave a '\0' byte, at which the Descr ended
xml('xml_entity_numeric.xml', HEADER + '< TreeItem name = "xn" >\n< Descr > x&#65;y&#x42;z&unknown;w&#169;v < / Descr >\n< / TreeItem >\n')
# INF-A01 of doc/code-audit-2026-09-27.md: entities in an ATTRIBUTE value, which since #1261 is decoded
# too, a known one and an unknown one; every such value used to hang the load. Only properties fixed at
# construction are attributes (name, ValuesUnit, DomainUnit, ValueType), and no valid value of those
# holds an entity, so the value is an unknown values unit: the load must end with that error instead of
# hanging. Written compact, as the XML writer does
xml('xml_attr_entity.xml', '<?xml version="1.0" encoding="UTF-8"?>\n<TreeItem name="xae"><DATAITEM name="xap" ValuesUnit="str&amp;ing&unknown;"><CalcRule>&apos;v&apos;</CalcRule></DATAITEM></TreeItem>\n')

# ---------------------------------------------------------------- dBase III table (STG-A03, STG-A04 of doc/code-audit-2026-09-27.md)
def dbf(name, fields, records):
    # fields: (name, type 'N' or 'C', length, decimals); records: tuples of the raw field texts.
    # A numeric field is right-aligned and blank-filled, as dBase writes it; an empty text is a NULL.
    rec_len = 1 + sum(f[2] for f in fields)
    hdr_len = 32 + 32 * len(fields) + 1
    head = struct.pack('<BBBBIHH20x', 3, 126, 9, 27, len(records), hdr_len, rec_len)
    descr = b''
    for fname, ftype, flen, fdec in fields:
        descr += fname.encode('ascii').ljust(11, b'\0') + ftype.encode('ascii') + b'\0' * 4 + struct.pack('<BB', flen, fdec) + b'\0' * 14
    body = b''
    for rec in records:
        body += b' ' + b''.join(v.encode('ascii').rjust(f[2]) if f[1] == 'N' else v.encode('ascii').ljust(f[2]) for v, f in zip(rec, fields))
    data = head + descr + b'\r' + body + b'\x1a'
    with open(os.path.join(OUT, name), 'wb') as f:
        f.write(data)
    print(name, len(data), 'bytes')

# an 18-digit id beyond 2^53 (int64 used to read as all zeros), NULL numerics written as blanks and as
# '*' fill (one used to fail the whole column), and an ESRI long integer N(10,0)
dbf('dbf_int64_null.dbf',
    [('ID', 'N', 18, 0), ('VAL', 'N', 10, 0), ('NAME', 'C', 4, 0)],
    [('123456789012345678', '42', 'a'),
     ('', '**********', 'b'),
     ('-5', '-7', 'c')])

# read through the ODBC storage manager with the Access dBASE driver (stor_odbc_dbf.dms): a folder of its own,
# since the driver's database is the folder (DBQ) and each .dbf in it a table. Numerics, a NULL numeric written
# as blanks, and strings of different lengths, an empty one included, read row by row.
os.makedirs(os.path.join(OUT, 'odbc'), exist_ok=True)
dbf(os.path.join('odbc', 'odbc_t1.dbf'),
    [('ID', 'N', 4, 0), ('I', 'N', 6, 0), ('F', 'N', 10, 3), ('S', 'C', 12, 0)],
    [('1', '7',   '1.500',  'alpha'),
     ('2', '',    '2.250',  'be'),
     ('3', '-3',  '',       ''),
     ('4', '120', '-0.125', 'gamma delta'),
     ('5', '0',   '10.000', 'e')])
