#!/usr/bin/env python3
"""Verify that the exported ABI symbols survived into a Mach-O binary.

Why this exists: `nm` on the CI runner is Apple's LLVM 17 build, and the
release artefacts are produced by a much newer rustc (LLVM 23).  That `nm`
chokes on the newer DWARF ("Unknown attribute kind (102)") and prints nothing
for the object that actually holds our code, so a `nm | grep` gate reports a
false failure.  This script reads only the load commands and the symbol table,
which are stable across LLVM versions, so the gate means what it says.

It understands:
  * thin Mach-O objects, dylibs and executables,
  * universal (fat) binaries -- each slice,
  * static archives (`!<arch>`), including BSD `#1/len` long member names.

Usage:
    scripts/check_symbols.py libhrt_core.a [more binaries ...]
    scripts/check_symbols.py --header include/hrt_core.h libhrt_core.a

Exit status is 0 only when every symbol promised by the header is present as a
defined external symbol in every file given.
"""

from __future__ import annotations

import argparse
import os
import re
import struct
import sys

THIN_MAGICS = {
    0xFEEDFACE,  # MH_MAGIC,   32-bit, host endian
    0xFEEDFACF,  # MH_MAGIC_64, 64-bit, host endian
    0xCEFAEDFE,  # MH_CIGAM
    0xCFFAEDFE,  # MH_CIGAM_64
}
FAT_MAGICS = {0xCAFEBABE, 0xCAFEBABF}
LC_SYMTAB = 0x2
N_STAB = 0xE0
N_TYPE = 0x0E
N_EXT = 0x01
N_SECT = 0x0E


def _thin_symbols(data: bytes, out: set[str]) -> None:
    """Collect defined external symbols from one Mach-O image."""
    if len(data) < 32:
        return
    magic = struct.unpack_from("<I", data, 0)[0]
    if magic not in (0xFEEDFACE, 0xFEEDFACF):
        # Big-endian images are not produced for any platform we ship.
        return
    is64 = magic == 0xFEEDFACF
    ncmds, _ = struct.unpack_from("<II", data, 16)
    off = 32 if is64 else 28

    for _ in range(ncmds):
        if off + 8 > len(data):
            return
        cmd, cmdsize = struct.unpack_from("<II", data, off)
        if cmd == LC_SYMTAB and off + 24 <= len(data):
            symoff, nsyms, stroff, _ = struct.unpack_from("<IIII", data, off + 8)
            entsize = 16 if is64 else 12
            for i in range(nsyms):
                p = symoff + i * entsize
                if p + entsize > len(data):
                    break
                if is64:
                    strx, ntype, _nsect, _ndesc, nvalue = struct.unpack_from("<IBBhQ", data, p)
                else:
                    strx, ntype, _nsect, _ndesc, nvalue = struct.unpack_from("<IBBhI", data, p)
                if ntype & N_STAB:
                    continue
                if not ntype & N_EXT or ntype & N_TYPE != N_SECT or nvalue == 0:
                    continue
                start = stroff + strx
                if start >= len(data):
                    continue
                end = data.find(b"\0", start)
                if end < 0:
                    continue
                out.add(data[start:end].decode("utf-8", "replace").lstrip("_"))
        if cmdsize == 0:
            return
        off += cmdsize


def _fat_symbols(data: bytes, out: set[str]) -> None:
    magic = struct.unpack_from(">I", data, 0)[0]
    count = struct.unpack_from(">I", data, 4)[0]
    wide = magic == 0xCAFEBABF
    for i in range(count):
        if wide:
            _, _, offset, size, _ = struct.unpack_from(">iiQQI", data, 8 + i * 32)
        else:
            _, _, offset, size = struct.unpack_from(">iiII", data, 8 + i * 20)
        slice_data = data[offset:offset + size]
        # `lipo -create` on static libraries produces a fat file whose slices
        # are themselves archives, so a universal .a needs both readers.
        if slice_data.startswith(b"!<arch>\n"):
            _archive_symbols(slice_data, out)
        else:
            _thin_symbols(slice_data, out)


def _archive_symbols(data: bytes, out: set[str]) -> None:
    if not data.startswith(b"!<arch>\n"):
        return
    pos = 8
    gnu_names: list[bytes] = []
    while pos + 60 <= len(data):
        header = data[pos:pos + 60]
        raw_name = header[0:16].decode("ascii", "replace").strip()
        try:
            size = int(header[48:58].decode("ascii").strip())
        except ValueError:
            return
        body = data[pos + 60:pos + 60 + size]

        if raw_name in ("//", "/"):
            # GNU long-name table (used by llvm-ar on non-Darwin hosts).
            gnu_names = [n for n in body.split(b"/\n") if n]
            pos += 60 + size + (size & 1)
            continue
        if raw_name.startswith("#1/"):
            namelen = int(raw_name[3:])
            for skip in (namelen, (namelen + 3) & ~3, (namelen + 7) & ~7):
                if struct.unpack_from("<I", body + b"\0\0\0\0", skip)[0] in THIN_MAGICS:
                    body = body[skip:]
                    break
        elif raw_name.startswith("/") and raw_name[1:].rstrip().isdigit():
            index = int(raw_name[1:].rstrip())
            if index < len(gnu_names):
                pass  # name only; the payload still starts at body[0]

        if body[:4] == b"!<ar" or size == 0:
            pos += 60 + size + (size & 1)
            continue
        if struct.unpack_from("<I", body + b"\0\0\0\0", 0)[0] in THIN_MAGICS:
            _thin_symbols(body, out)
        pos += 60 + size + (size & 1)


def symbols_in(path: str) -> set[str]:
    data = open(path, "rb").read()
    out: set[str] = set()
    if data.startswith(b"!<arch>\n"):
        _archive_symbols(data, out)
    elif data[:4] in (b"\xcf\xfa\xed\xfe", b"\xce\xfa\xed\xfe"):
        _thin_symbols(data, out)
    elif data[:4] in (b"\xca\xfe\xba\xbe", b"\xca\xfe\xba\xbf"):
        _fat_symbols(data, out)
    return out


def expected_from_header(path: str) -> list[str]:
    text = open(path, encoding="utf-8").read()
    names = re.findall(r"HRT_API\b[^;]*?\b(hrt_[a-z0-9_]+)\s*\(", text, re.S)
    return sorted(set(names))


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("binaries", nargs="+", help="Mach-O files to inspect")
    parser.add_argument(
        "--header",
        default=os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "include", "hrt_core.h"),
        help="header that declares the ABI (default: include/hrt_core.h)",
    )
    args = parser.parse_args(argv[1:])

    expected = expected_from_header(args.header)
    if not expected:
        print(f"no HRT_API declarations found in {args.header}", file=sys.stderr)
        return 2

    failed = False
    for path in args.binaries:
        found = symbols_in(path)
        missing = [name for name in expected if name not in found]
        status = "ok" if not missing else "MISSING"
        print(f"{status:8} {path}: {len(expected) - len(missing)}/{len(expected)} symbols")
        if missing:
            failed = True
            for name in missing:
                print(f"         - {name}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
