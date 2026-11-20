"""Build the optional USS Lua Menu Companion; never install or publish.
Archive layout adapted from CowboyBingus Vanilla Plus Megapack lua_patch.py (0BSD).
The upstream notice is shipped in THIRD_PARTY_LICENSES.txt.
"""
import argparse, hashlib, io, json, re, struct, sys, uuid, zipfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'Companion/uss_lua_menu_bridge.lua'
OUT = ROOT / 'build/lua-menu-companion'
ARCHIVE = '9ba626afa44a3aa3.patch_0'   # the archive name Bingus Shared Loader packs use (same as the Megapack's Lua part); HellForge renumbers patches when it stages them
LUA = 0xA14E8DFA2CD117E2               # resource type hash of "lua"
DECLARATION = re.compile(rb'^-- HD2-Addon: (mods/[A-Za-z0-9_]+(?:/[A-Za-z0-9_]+)+)\n')


def resource_hash(name):
    """MurmurHash64A, seed zero (the game's resource-name hash)."""
    data = name.encode('ascii'); mask, mix = (1 << 64) - 1, 0xC6A4A7935BD1E995
    value = len(data) * mix & mask; end = len(data) // 8 * 8
    for (word,) in struct.iter_unpack('<Q', data[:end]):
        word = word * mix & mask; word ^= word >> 47; value = (value ^ (word * mix & mask)) * mix & mask
    if data[end:]: value = (value ^ int.from_bytes(data[end:], 'little')) * mix & mask
    value ^= value >> 47; value = value * mix & mask
    return value ^ (value >> 47)


def archive(name, resource):
    offset = (104 + 80 + 15) & ~15; body = bytearray(offset)
    entry = struct.pack('<7Q6I', resource_hash(name), LUA, offset, 0, 0, 0, 0, len(resource), 0, 0, 16, 16, 0)
    body += resource; body += b'\0' * (-len(body) % 16)
    header = struct.pack('<III20sQQ24s', 0xF0000011, 1, 1, b'', len(body), 0, b'')
    types = struct.pack('<IIQIIII', 0, 0, LUA, 1, 0, 16, 16)
    body[:104 + len(entry)] = header + types + entry
    return bytes(body)


def source_resource(path):
    source = Path(path).read_bytes(); match = DECLARATION.match(source)
    if not match: raise SystemExit(f'{path}: the first line must be "-- HD2-Addon: mods/<name>" (LF, no byte-order mark)')
    if b'\r' in source: raise SystemExit(f'{path}: LF line endings only')
    source.decode('utf-8')
    return match.group(1).decode('ascii'), struct.pack('<II', len(source), 2) + source


def parse_back(blob):
    """Independent reader of what archive() wrote: returns (name_hash, type, resource_bytes) so a round trip proves the layout, not just equality with the writer."""
    magic, version, count = struct.unpack_from('<III', blob, 0); assert (magic, version, count) == (0xF0000011, 1, 1)
    fields = struct.unpack_from('<7Q6I', blob, 104); name_hash, type_hash, offset, length = fields[0], fields[1], fields[2], fields[7]
    return name_hash, type_hash, blob[offset:offset + length]


def main():
    global OUT
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, default=OUT)
    parser.add_argument('--package', action='store_true')
    args = parser.parse_args()
    OUT = args.out.resolve()
    name, resource = source_resource(SOURCE); blob = archive(name, resource)
    OUT.mkdir(parents=True, exist_ok=True); data = OUT / 'data'; data.mkdir(exist_ok=True)
    (data / ARCHIVE).write_bytes(blob)
    for suffix in ('.stream', '.gpu_resources'): (data / (ARCHIVE + suffix)).write_bytes(b'')
    guid = str(uuid.uuid5(uuid.NAMESPACE_URL, 'https://github.com/MajorXAce/Utility-Support-Slot#lua-menu-companion'))   # Preserve the identity used by the accepted companion candidate.
    manifest = {'Version': 1, 'Guid': guid, 'Name': 'Utility Support Slot - Lua Menu Companion',
                'Description': 'Optional Lua menu support for Utility Support Slot. Requires the matching USS version, Lua menu providers and Bingus Shared Loader.',
                'Options': [{'Name': 'Utility Support Slot - Lua Menu Companion', 'Description': 'Registers USS options with the Lua menus.', 'Include': ['data']}]}
    (OUT / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    got_hash, got_type, got_resource = parse_back(blob)
    assert got_hash == resource_hash(name) and got_type == LUA and got_resource == resource
    inner = got_resource[8:]; assert struct.unpack_from('<II', got_resource)[0] == len(inner) and inner == SOURCE.read_bytes()
    if args.package:
        version=(ROOT/'VERSION').read_text().strip();dist=ROOT/'dist';dist.mkdir(exist_ok=True)
        package=dist/('Utility-Support-Slot-Lua-Menu-Companion-v'+version+'.zip')
        files={'manifest.json':OUT/'manifest.json',**{p.relative_to(OUT).as_posix():p for p in data.iterdir()},
               'LICENSE':ROOT/'LICENSE','THIRD_PARTY_LICENSES.txt':ROOT/'THIRD_PARTY_LICENSES.txt','README.md':ROOT/'COMPANION.md'}
        buffer=io.BytesIO()
        with zipfile.ZipFile(buffer,'w',zipfile.ZIP_DEFLATED) as z:
            for key,path in sorted(files.items()):
                info=zipfile.ZipInfo(key,(1980,1,1,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED;info.external_attr=0o100644<<16
                z.writestr(info,path.read_bytes())
        payload=buffer.getvalue()
        if package.exists() and package.read_bytes()!=payload:raise RuntimeError('Preserve immutable ZIP: '+str(package))
        package.write_bytes(payload)
    print(json.dumps({'resource_name': name, 'archive_bytes': len(blob), 'archive_sha256': hashlib.sha256(blob).hexdigest(), 'source_sha256': hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
                      'manifest_sha256': hashlib.sha256((OUT / 'manifest.json').read_bytes()).hexdigest(), 'guid_proposal': guid, 'round_trip': 'PASS'}))


if __name__ == '__main__': main()
