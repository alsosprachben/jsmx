#!/usr/bin/env python3
import sys
import re

WEIGHT_RE = re.compile(r"\[([0-9A-Fa-f.]+)\]")


def parse(path):
    records = []
    with open(path, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.split('#', 1)[0].strip()
            if not line:
                continue
            if ';' not in line:
                continue
            left, right = line.split(';', 1)
            cps = [int(cp, 16) for cp in left.strip().split()]
            # only handle single codepoint entries for now
            if len(cps) != 1:
                continue
            m = WEIGHT_RE.search(right)
            if not m:
                continue
            weights = [int(x, 16) for x in m.group(1).split('.')]
            while len(weights) < 3:
                weights.append(0)
            records.append((cps[0], weights[0], weights[1], weights[2]))
    records.sort(key=lambda r: r[0])
    return records


def emit(records, out):
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
    out.write('#endif /* UNICODE_COLLATION_H */\n')


def main(argv):
    if len(argv) != 3:
        print('usage: gen_unicode_collation.py allkeys.txt output.h')
        return 1
    recs = parse(argv[1])
    with open(argv[2], 'w', encoding='utf-8') as out:
        emit(recs, out)


if __name__ == '__main__':
    sys.exit(main(sys.argv))
