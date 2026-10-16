"""Rebuild current executable Lua sources into the validated five-resource archive.

Run from repository root: python tools/build_release.py
This preserves resource IDs and inert payloads, recalculating offsets and lengths
so source edits have no fixed slot-size limit.
Update source version strings, VERSION, documentation and release notes first.
The readable executable core is canonical. Named shared factory blocks are
assembled automatically before packing; standalone factory files own their
embedded content. No duplicate readable entry or compaction step is needed.
"""
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1]
RELEASE = ROOT / "release" if (ROOT / "release").is_dir() else ROOT
ARCHIVE = RELEASE / "Core/9ba626afa44a3aa3.patch_0"
SOURCES = {
    "mods/local/utility_support_slot": "utility_support_slot.lua",
    "mods/local/utility_support_slot_resupply": "utility_support_slot_resupply.lua",
}


def resource_hash(name):
    data = name.encode()
    mask, mix = (1 << 64) - 1, 0xC6A4A7935BD1E995
    value = len(data) * mix & mask
    end = len(data) // 8 * 8
    for (word,) in struct.iter_unpack("<Q", data[:end]):
        word = word * mix & mask
        word ^= word >> 47
        value = (value ^ (word * mix & mask)) * mix & mask
    if end < len(data):
        value = (value ^ int.from_bytes(data[end:], "little")) * mix & mask
    value ^= value >> 47
    value = value * mix & mask
    return value ^ (value >> 47)


def lua_source(path):
    """Canonical LF text makes archives independent of Git's Windows checkout EOLs.

    Only our owned Lua source passes here. Inert resource payloads remain binary
    copies, and packing never rewrites the source file or changes resource IDs.
    """
    return path.read_bytes().replace(b"\r\n", b"\n")


def pack(original):
    magic, _, count = struct.unpack_from("<III", original)
    assert magic == 0xF0000011 and count == 5, "Unexpected archive template"
    directory_end = 104 + count * 80
    patch = bytearray(original[:directory_end])
    patch.extend(b"\0" * (-len(patch) % 16))
    found = set()
    for index in range(count):
        entry = 104 + index * 80
        identity, kind, offset = struct.unpack_from("<QQQ", original, entry)
        length, version = struct.unpack_from("<II", original, offset)
        assert kind == 0xA14E8DFA2CD117E2 and version == 2
        previous = original[offset + 8:offset + 8 + length]
        marker = previous.splitlines()[0].decode().removeprefix("-- HD2-Addon: ")
        assert identity == resource_hash(marker), "Archive identity mismatch"
        if marker in SOURCES:
            data = lua_source(ROOT / "Source" / SOURCES[marker])
            assert data.splitlines()[0] == previous.splitlines()[0], "Keep runtime identifiers"
            found.add(marker)
        else:
            data = previous  # Preserve each inert resource byte for byte.
        resource = struct.pack("<II", len(data), 2) + data
        struct.pack_into("<Q", patch, entry + 16, len(patch))
        struct.pack_into("<I", patch, entry + 56, len(resource))
        patch.extend(resource)
        patch.extend(b"\0" * (-len(patch) % 16))
    assert found == set(SOURCES)
    struct.pack_into("<Q", patch, 32, len(patch))
    return bytes(patch)


def mode_pack(original, mode):
    """One resource, unique from Core; both choices replace the same config ID."""
    marker = "mods/local/utility_support_slot_binding_mode"
    data = lua_source(ROOT / "Source" / ("utility_binding_" + mode + ".lua"))
    assert data.splitlines()[0] == ("-- HD2-Addon: " + marker).encode()
    patch = bytearray(original[:104] + original[104:184])
    struct.pack_into("<I", patch, 8, 1)
    struct.pack_into("<I", patch, 88, 1)  # Lua resource type-group count.
    patch.extend(b"\0" * (-len(patch) % 16))
    resource = struct.pack("<II", len(data), 2) + data
    struct.pack_into("<Q", patch, 104, resource_hash(marker))
    struct.pack_into("<Q", patch, 120, len(patch))
    struct.pack_into("<I", patch, 160, len(resource))
    patch.extend(resource)
    patch.extend(b"\0" * (-len(patch) % 16))
    struct.pack_into("<Q", patch, 32, len(patch))
    return bytes(patch)


def build():
    import runpy
    runpy.run_path(str(ROOT/'tools/assemble_sources.py'))['assemble']()
    core = pack(ARCHIVE.read_bytes())
    ARCHIVE.write_bytes(core)
    for folder, mode in (("DefaultBindings", "default"), ("DedicatedKey", "dedicated")):
        target = RELEASE / folder / "9ba626afa44a3aa3.patch_1"
        target.write_bytes(mode_pack(core, mode))
        Path(str(target) + ".stream").write_bytes(b"")
        Path(str(target) + ".gpu_resources").write_bytes(b"")
    print("Rebuilt shared Core and configuration-only binding choices")


if __name__ == "__main__":
    build()
