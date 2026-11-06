"""Prepare QuickLoot IE's LootMenuIE.swf for Skyrim LE's Scaleform.

    py -3.11 -I prepare_swf.py <in.swf> <out.swf> [report.json]

The only change is the SWF header version (15 -> 10), the same header-only conversion
TrueHUD LE ships. Every tag is checked first: the movie must be AVM1 only (no DoABC /
DoABC2, FileAttributes without the AS3 bit), and every tag body is copied byte for byte.
The output is re-read and its decompressed body compared with the input's, so the
report proves that nothing but the version byte changed. Playback is NOT proven.
"""
import hashlib, json, struct, sys, zlib

def body(raw):
    sig = raw[:3]
    if sig == b'CWS':
        return raw[:8], zlib.decompress(raw[8:])
    if sig == b'FWS':
        return raw[:8], raw[8:]
    raise SystemExit('not a SWF: %r' % sig)

def tags(data):
    nbits = data[0] >> 3
    pos = (5 + 4 * nbits + 7) // 8 + 4          # RECT + frame rate + frame count
    out = []
    while pos < len(data):
        code_len = struct.unpack_from('<H', data, pos)[0]; pos += 2
        code, length = code_len >> 6, code_len & 0x3F
        if length == 0x3F:
            length = struct.unpack_from('<I', data, pos)[0]; pos += 4
        if pos + length > len(data):
            raise SystemExit('tag %d overruns the movie' % code)
        out.append((code, data[pos:pos + length]))
        pos += length
        if code == 0:
            break
    return out, pos

def main():
    src, dst = sys.argv[1], sys.argv[2]
    raw = open(src, 'rb').read()
    head, data = body(raw)
    version = raw[3]
    declared = struct.unpack_from('<I', raw, 4)[0]
    if declared != len(data) + 8:
        raise SystemExit('declared length %d != %d' % (declared, len(data) + 8))
    tlist, end = tags(data)
    codes = sorted({c for c, _ in tlist})
    if 72 in codes or 82 in codes:
        raise SystemExit('AVM2 code present (DoABC) - not an AS2 movie')
    for c, b in tlist:
        if c == 69 and b and (b[0] & 0x08):
            raise SystemExit('FileAttributes declares ActionScript 3')
    if end != len(data):
        raise SystemExit('trailing bytes after the End tag')
    new = bytearray(head); new[3] = 10
    out = bytes(new) + zlib.compress(data, 9) if raw[:3] == b'CWS' else bytes(new) + data
    open(dst, 'wb').write(out)
    back = open(dst, 'rb').read()
    h2, d2 = body(back)
    assert back[3] == 10 and d2 == data and struct.unpack_from('<I', back, 4)[0] == declared
    rep = dict(input=src, input_sha256=hashlib.sha256(raw).hexdigest(), input_version=version,
               output=dst, output_sha256=hashlib.sha256(back).hexdigest(), output_version=10,
               tags=len(tlist), tag_codes=codes, body_sha256=hashlib.sha256(data).hexdigest(),
               body_identical=True, avm2=False,
               limitation='header-only conversion; LE Scaleform playback is not proven by this script')
    if len(sys.argv) > 3:
        open(sys.argv[3], 'w').write(json.dumps(rep, indent=2) + '\n')
    print(json.dumps(rep))

if __name__ == '__main__':
    main()
