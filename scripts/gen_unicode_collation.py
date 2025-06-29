#!/usr/bin/env python3
import sys
import re

WEIGHT_RE = re.compile(r"\[([0-9A-Fa-f.]+)\]")


def parse(path):
    records = []
    implicit_rules = []   # list of (start, end, [p, s, t])

    with open(path, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.split('#', 1)[0].strip()
            if not line or ';' not in line:
                continue

            left, right = line.split(';', 1)
            left = left.strip()

            # 1) capture implicit-weight metadata
            if left.startswith('@implicitweights'):
                # left looks like '@implicitweights 3400..4DB5'
                rng = left[len('@implicitweights'):].strip()
                if '..' in rng:
                    lo, hi = rng.split('..', 1)
                else:
                    lo = hi = rng
                lo = int(lo, 16)
                hi = int(hi, 16)

                m = WEIGHT_RE.search(right)
                if not m:
                    continue
                w = [int(x, 16) for x in m.group(1).split('.')]
                while len(w) < 3:
                    w.append(0)

                implicit_rules.append((lo, hi, tuple(w)))
                continue

            if not left or not right:
                continue

            # 2) explicit single-code-point entries
            cps = [int(cp, 16) for cp in left.split() if len(cp) > 0]
            if len(cps) != 1:
                continue
            m = WEIGHT_RE.search(right)
            if not m:
                continue
            w = [int(x, 16) for x in m.group(1).split('.') if len(x) > 0]
            while len(w) < 3:
                w.append(0)
            records.append((cps[0], w[0], w[1], w[2]))

    records.sort(key=lambda r: r[0])

    # 3) fill in missing points from implicit_rules
    filled = []
    it = iter(records)
    next_rec = next(it, None)

    # pick whatever universe of code points you need, e.g. 0..0x10FFFF
    for cp in range(0x110000):
        if next_rec and cp == next_rec[0]:
            filled.append(next_rec)
            next_rec = next(it, None)
            continue

        # find a matching implicit range
        for lo, hi, (p, s, t) in implicit_rules:
            if lo <= cp <= hi:
                # you may want to increment p/s/t per cp – spec has details
                filled.append((cp, p, s, t))
                break
        # otherwise skip entirely

    return filled

def compute_skip_list(records):
    skips = [0]
    for i in range(1, len(records)):
        if records[i][0] != records[i-1][0] + 1:
            skips.append(i)
    return skips


def emit(records, skip_list, out):
    out.write('#ifndef UNICODE_COLLATION_H\n')
    out.write('#define UNICODE_COLLATION_H\n')
    out.write('#include <stdint.h>\n')
    out.write('#include <stddef.h>\n')
    out.write('typedef struct { uint32_t code; uint16_t primary; uint16_t secondary; uint16_t tertiary; } unicode_collation_t;\n')
    out.write('static const unicode_collation_t unicode_collation_db[] = {\n')
    for code, p, s, t in records:
        out.write(f'    {{0x{code:04X}, 0x{p:04X}, 0x{s:04X}, 0x{t:04X}}},\n')
    out.write('};\n')
    out.write('static const size_t unicode_collation_db_len = sizeof(unicode_collation_db)/sizeof(unicode_collation_db[0]);\n')
    out.write('static const size_t unicode_collation_skip[] = {\n')
    for idx in skip_list:
        out.write(f'    {idx},\n')
    out.write('};\n')
    out.write('static const size_t unicode_collation_skip_len = sizeof(unicode_collation_skip)/sizeof(unicode_collation_skip[0]);\n')
    out.write('#endif /* UNICODE_COLLATION_H */\n')


def main(argv):
    if len(argv) != 3:
        print('usage: gen_unicode_collation.py allkeys.txt output.h')
        return 1
    recs = parse(argv[1])
    skip_list = compute_skip_list(recs)
    with open(argv[2], 'w', encoding='utf-8') as out:
        emit(recs, skip_list, out)


if __name__ == '__main__':
    sys.exit(main(sys.argv))
