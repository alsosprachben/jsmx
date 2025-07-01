"""Generate a header for Unicode composition exclusions.

The file ``CompositionExclusions.txt`` distributed with the Unicode
Character Database lists the set of characters that must be excluded
from canonical composition when constructing NFC/NFKC tables.  The
format of that file is fairly small: each significant line begins with
either a single code point or a range of code points followed by a
comment.  This script parses the file and emits a simple C header
containing a sorted list of all excluded code points.

Only the code points themselves are stored – no ranges are preserved –
so that a binary search can be used by the normalisation routines.
"""

import sys
from typing import List


def parse(path: str) -> List[int]:
    """Return a sorted list of excluded code points."""

    cps: List[int] = []
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            # Strip comments and whitespace
            line = line.split("#", 1)[0].strip()
            if not line:
                continue

            field = line.split()[0]
            if ".." in field:
                lo, hi = field.split("..", 1)
                start = int(lo, 16)
                end = int(hi, 16)
                cps.extend(range(start, end + 1))
            else:
                cps.append(int(field, 16))

    cps.sort()
    return cps


def emit(cps: List[int], out) -> None:
    out.write("#ifndef UNICODE_EXCLUSIONS_H\n")
    out.write("#define UNICODE_EXCLUSIONS_H\n")
    out.write("#include <stdint.h>\n")
    out.write("#include <stddef.h>\n")
    out.write("static const uint32_t unicode_exclusions[] = {\n")
    for cp in cps:
        out.write(f"    0x{cp:04X},\n")
    out.write("};\n")
    out.write(
        "static const size_t unicode_exclusions_len = "
        "sizeof(unicode_exclusions)/sizeof(unicode_exclusions[0]);\n"
    )
    out.write("#endif /* UNICODE_EXCLUSIONS_H */\n")


def main(argv: List[str]) -> int:
    if len(argv) != 3:
        print("usage: gen_unicode_exclusions.py CompositionExclusions.txt output.h")
        return 1

    cps = parse(argv[1])
    with open(argv[2], "w", encoding="utf-8") as out:
        emit(cps, out)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
