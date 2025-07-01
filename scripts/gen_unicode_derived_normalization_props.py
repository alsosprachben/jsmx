"""Generate a header for derived Unicode normalisation properties.

The ``DerivedNormalizationProps.txt`` file contains several properties
used when normalising Unicode strings.  This script parses that file and
emits a C header describing the relevant tables.
"""

from __future__ import annotations

import sys
from typing import Dict, List, Tuple

QC_PROPS = ["NFD_QC", "NFC_QC", "NFKD_QC", "NFKC_QC"]


def parse(path: str):
    """Parse ``DerivedNormalizationProps.txt``.

    Returns ``(qc, nfkc_cf, nfkc_scf, cwkcf)`` where ``qc`` maps property
    names to lists of ``(start, end, value)`` tuples. ``nfkc_cf`` and
    ``nfkc_scf`` are lists of ``(code, sequence)`` mappings and
    ``cwkcf`` is a list of ``(start, end)`` ranges.
    """

    qc: Dict[str, List[Tuple[int, int, str]]] = {p: [] for p in QC_PROPS}
    nfkc_cf: List[Tuple[int, List[int]]] = []
    nfkc_scf: List[Tuple[int, List[int]]] = []
    cwkcf: List[Tuple[int, int]] = []

    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.split("#", 1)[0].strip()
            if not line or ";" not in line:
                continue
            fields = [x.strip() for x in line.split(";")]
            if len(fields) < 3:
                continue
            code_str, prop, value = fields[:3]

            if prop in QC_PROPS:
                if ".." in code_str:
                    lo, hi = code_str.split("..", 1)
                    start, end = int(lo, 16), int(hi, 16)
                else:
                    start = end = int(code_str, 16)
                qc[prop].append((start, end, value))

            elif prop == "NFKC_CF":
                seq = [int(x, 16) for x in value.split()] if value else []
                if ".." in code_str:
                    lo, hi = code_str.split("..", 1)
                    for cp in range(int(lo, 16), int(hi, 16) + 1):
                        nfkc_cf.append((cp, seq))
                else:
                    nfkc_cf.append((int(code_str, 16), seq))

            elif prop == "NFKC_SCF":
                seq = [int(x, 16) for x in value.split()] if value else []
                if ".." in code_str:
                    lo, hi = code_str.split("..", 1)
                    for cp in range(int(lo, 16), int(hi, 16) + 1):
                        nfkc_scf.append((cp, seq))
                else:
                    nfkc_scf.append((int(code_str, 16), seq))

            elif prop == "Changes_When_NFKC_Casefolded":
                if ".." in code_str:
                    lo, hi = code_str.split("..", 1)
                    cwkcf.append((int(lo, 16), int(hi, 16)))
                else:
                    cp = int(code_str, 16)
                    cwkcf.append((cp, cp))

    return qc, nfkc_cf, nfkc_scf, cwkcf


def merge_qc_ranges(ranges: List[Tuple[int, int, str]]):
    ranges.sort(key=lambda r: (r[0], r[1]))
    merged: List[Tuple[int, int, str]] = []
    for start, end, val in ranges:
        if merged and merged[-1][1] + 1 == start and merged[-1][2] == val:
            merged[-1] = (merged[-1][0], end, val)
        else:
            merged.append((start, end, val))
    return merged


def merge_ranges(ranges: List[Tuple[int, int]]):
    ranges.sort()
    merged: List[Tuple[int, int]] = []
    for start, end in ranges:
        if merged and merged[-1][1] + 1 >= start:
            if end > merged[-1][1]:
                merged[-1] = (merged[-1][0], end)
        else:
            merged.append((start, end))
    return merged


def flatten_mappings(maps: List[Tuple[int, List[int]]]):
    maps.sort(key=lambda m: m[0])
    data: List[int] = []
    table: List[Tuple[int, int, int]] = []
    for cp, seq in maps:
        idx = len(data)
        data.extend(seq)
        table.append((cp, idx, len(seq)))
    return table, data


def emit(qc, nfkc_cf, nfkc_scf, cwkcf, out) -> None:
    out.write("#ifndef UNICODE_DERIVED_NORMALIZATION_PROPS_H\n")
    out.write("#define UNICODE_DERIVED_NORMALIZATION_PROPS_H\n")
    out.write("#include <stdint.h>\n")
    out.write("#include <stddef.h>\n")

    out.write(
        "typedef struct { uint32_t start; uint32_t end; uint8_t qc; } unicode_qc_range_t;\n"
    )
    out.write("#define UNICODE_QC_NO 0\n")
    out.write("#define UNICODE_QC_MAYBE 1\n")

    for prop in QC_PROPS:
        arr = f"unicode_{prop.lower()}"
        out.write(f"static const unicode_qc_range_t {arr}[] = {{\n")
        for start, end, val in qc[prop]:
            flag = "UNICODE_QC_NO" if val == "N" else "UNICODE_QC_MAYBE"
            out.write(f"    {{0x{start:04X}, 0x{end:04X}, {flag}}},\n")
        out.write("};\n")
        out.write(
            f"static const size_t {arr}_len = sizeof({arr})/sizeof({arr}[0]);\n"
        )

    # NFKC_CF mappings
    cf_table, cf_data = flatten_mappings(nfkc_cf)
    out.write(
        "typedef struct { uint32_t code; uint16_t index; uint8_t len; } unicode_nfkc_cf_map_t;\n"
    )
    out.write("static const uint32_t unicode_nfkc_cf_data[] = {\n")
    for cp in cf_data:
        out.write(f"    0x{cp:04X},\n")
    out.write("};\n")
    out.write("static const unicode_nfkc_cf_map_t unicode_nfkc_cf_map[] = {\n")
    for cp, idx, ln in cf_table:
        out.write(f"    {{0x{cp:04X}, {idx}, {ln}}},\n")
    out.write("};\n")
    out.write(
        "static const size_t unicode_nfkc_cf_map_len = sizeof(unicode_nfkc_cf_map)/sizeof(unicode_nfkc_cf_map[0]);\n"
    )

    # NFKC_SCF mappings
    scf_table, scf_data = flatten_mappings(nfkc_scf)
    out.write(
        "typedef struct { uint32_t code; uint16_t index; uint8_t len; } unicode_nfkc_scf_map_t;\n"
    )
    out.write("static const uint32_t unicode_nfkc_scf_data[] = {\n")
    for cp in scf_data:
        out.write(f"    0x{cp:04X},\n")
    out.write("};\n")
    out.write("static const unicode_nfkc_scf_map_t unicode_nfkc_scf_map[] = {\n")
    for cp, idx, ln in scf_table:
        out.write(f"    {{0x{cp:04X}, {idx}, {ln}}},\n")
    out.write("};\n")
    out.write(
        "static const size_t unicode_nfkc_scf_map_len = sizeof(unicode_nfkc_scf_map)/sizeof(unicode_nfkc_scf_map[0]);\n"
    )

    # CWKCF ranges
    cwkcf = merge_ranges(cwkcf)
    out.write(
        "typedef struct { uint32_t start; uint32_t end; } unicode_range_t;\n"
    )
    out.write("static const unicode_range_t unicode_cwkcf[] = {\n")
    for start, end in cwkcf:
        out.write(f"    {{0x{start:04X}, 0x{end:04X}}},\n")
    out.write("};\n")
    out.write(
        "static const size_t unicode_cwkcf_len = sizeof(unicode_cwkcf)/sizeof(unicode_cwkcf[0]);\n"
    )

    out.write("#endif /* UNICODE_DERIVED_NORMALIZATION_PROPS_H */\n")


def main(argv: List[str]) -> int:
    if len(argv) != 3:
        print(
            "usage: gen_unicode_derived_normalization_props.py DerivedNormalizationProps.txt output.h"
        )
        return 1

    qc, nfkc_cf, nfkc_scf, cwkcf = parse(argv[1])

    for prop in QC_PROPS:
        qc[prop] = merge_qc_ranges(qc[prop])

    with open(argv[2], "w", encoding="utf-8") as out:
        emit(qc, nfkc_cf, nfkc_scf, cwkcf, out)

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
