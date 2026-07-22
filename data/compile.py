import argparse
import os
import sys

import re
import struct

MAGIC = b"STDK"
VERSION = 1
HEADER_SIZE = 128
HASH_ENTRIES = 27

PHONEME_NAMES = {
    0: "IY", 1: "IH", 2: "EH", 3: "AE", 4: "IX", 5: "AX", 6: "ER", 7: "AH",
    8: "AA", 9: "AO", 10: "UH", 11: "UW", 12: "EY", 13: "AY", 14: "OY",
    15: "AW", 16: "OW", 17: "YU", 18: "IR", 19: "XR", 20: "AR", 21: "OR",
    22: "UR", 23: "SIL", 24: "M", 25: "N", 26: "NG", 27: "W", 28: "Y",
    29: "R", 30: "L", 31: "RX", 32: "LX", 33: "EL", 34: "EN", 35: "F",
    36: "V", 37: "TH", 38: "DH", 39: "S", 40: "Z", 41: "SH", 42: "ZH",
    43: "HH", 44: "P", 45: "B", 46: "T", 47: "D", 48: "K", 49: "G",
    50: "CH", 51: "JH", 52: "TX", 53: "DX", 54: "QX", 55: "DD",
}
NAME_TO_ID = {name: pid for pid, name in PHONEME_NAMES.items()}

VOWEL_IDS = set(range(0, 23))
STRESS_PRIMARY = 56
STRESS_SECONDARY = 57


def phons_to_tokens(phons):
    tokens = []
    pending = ""
    for b in phons:
        if b == STRESS_PRIMARY:
            pending = "1"
            continue
        if b == STRESS_SECONDARY:
            pending = "2"
            continue
        name = PHONEME_NAMES.get(b)
        if name is None:
            raise ValueError("unknown phoneme byte %d" % b)
        if pending:
            if b not in VOWEL_IDS:
                raise ValueError("stress marker before non-vowel %s" % name)
            name += pending
            pending = ""
        tokens.append(name)
    if pending:
        raise ValueError("trailing stress marker with no vowel")
    return tokens


def tokens_to_phons(tokens):
    phons = []
    for tok in tokens:
        base, stress = tok, ""
        if tok and tok[-1] in "012" and tok[:-1] in NAME_TO_ID:
            base, stress = tok[:-1], tok[-1]
        pid = NAME_TO_ID.get(base)
        if pid is None:
            raise ValueError("unknown phoneme token %r" % tok)
        if stress in ("1", "2"):
            if pid not in VOWEL_IDS:
                raise ValueError("stress on non-vowel token %r" % tok)
            phons.append(STRESS_PRIMARY if stress == "1" else STRESS_SECONDARY)
        phons.append(pid)
    return phons


def parse_blob(blob):
    if blob[:4] != MAGIC:
        raise ValueError("bad STDICT magic")
    word_count = struct.unpack_from("<I", blob, 8)[0]
    index_off = struct.unpack_from("<I", blob, 16)[0]
    entries = []
    for i in range(word_count):
        off = struct.unpack_from("<I", blob, index_off + i * 4)[0]
        d_len = blob[off]
        word = blob[off + 1:off + 1 + d_len].decode("ascii")
        p_off = off + 1 + d_len
        p_len = blob[p_off]
        phons = list(blob[p_off + 1:p_off + 1 + p_len])
        entries.append((word, phons))
    return entries


def build_blob(entries):
    ordered = sorted(entries, key=lambda e: e[0].encode("ascii"))
    word_count = len(ordered)

    data = bytearray()
    offsets = []
    for word, phons in ordered:
        wb = word.encode("ascii")
        if len(wb) > 255 or len(phons) > 255:
            raise ValueError("word or phon stream too long: %r" % word)
        offsets.append(HEADER_SIZE + len(data))
        data += bytes([len(wb)]) + wb + bytes([len(phons)]) + bytes(phons)

    index_off = HEADER_SIZE + len(data)

    first_letters = [w[0] for w, _ in ordered]
    starts = []
    for li in range(26):
        ch = chr(ord("A") + li)
        lo, hi = 0, word_count
        while lo < hi:
            mid = (lo + hi) // 2
            if first_letters[mid] < ch:
                lo = mid + 1
            else:
                hi = mid
        starts.append(lo)
    starts.append(word_count)

    header = bytearray(HEADER_SIZE)
    header[0:4] = MAGIC
    struct.pack_into("<H", header, 4, VERSION)
    struct.pack_into("<H", header, 6, 0)
    struct.pack_into("<I", header, 8, word_count)
    struct.pack_into("<I", header, 12, HEADER_SIZE)
    struct.pack_into("<I", header, 16, index_off)
    for i, s in enumerate(starts):
        struct.pack_into("<I", header, 20 + i * 4, s)

    index = bytearray()
    for off in offsets:
        index += struct.pack("<I", off)

    return bytes(header) + bytes(data) + bytes(index)


CPP_HEADER = (
    '#include "LibraryData.h"\n\n'
    "namespace SharpVox {\n\n"
    "const uint8_t LibraryData::dictionary[] =\n"
    "{\n"
)
CPP_FOOTER = (
    "};\n\n"
    "const int32_t LibraryData::dictionarySize = sizeof(LibraryData::dictionary);\n\n"
    "}  // namespace SharpVox\n"
)


def blob_to_cpp(blob):
    lines = [CPP_HEADER, "// %d bytes\n" % len(blob)]
    for i in range(0, len(blob), 16):
        chunk = blob[i:i + 16]
        lines.append(", ".join("0x%02X" % b for b in chunk) + ",\n")
    lines.append(CPP_FOOTER)
    return "".join(lines)


def cpp_to_blob(text):
    m = re.search(r"dictionary\[\]\s*=\s*\{(.*?)\};", text, re.S)
    if not m:
        raise ValueError("dictionary array not found")
    return bytes(int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]{2})", m.group(1)))

def load_source(path):
    entries = []
    seen = set()
    with open(path, "r") as f:
        for lineno, line in enumerate(f, 1):
            line = line.rstrip("\n")
            if not line or line.lstrip().startswith("#"):
                continue
            if "\t" in line:
                word, rest = line.split("\t", 1)
            else:
                parts = line.split(None, 1)
                word, rest = parts[0], (parts[1] if len(parts) > 1 else "")
            word = word.strip().upper()
            if not word:
                continue
            if word in seen:
                raise ValueError("line %d: duplicate word %r" % (lineno, word))
            seen.add(word)
            try:
                phons = tokens_to_phons(rest.split())
            except ValueError as e:
                raise ValueError("line %d (%s): %s" % (lineno, word, e))
            entries.append((word, phons))
    return entries


entries = load_source("english.dict")
blob = build_blob(entries)

with open("../src/LibraryDataDictionary.cpp", "w") as f:
    f.write(blob_to_cpp(blob))

print("compiled %d entries, %d byte blob into the C++" % (len(entries), len(blob)))
