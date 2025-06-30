#!/usr/bin/env python3
import sys


def parse(path):
    records = []
    with open(path, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.split('#', 1)[0].strip()
            if not line:
                continue
            fields = [field.strip() for field in line.split(';')]
            if len(fields) < 5:
                continue
            code = int(fields[0], 16)
            lower = [int(x, 16) for x in fields[1].split()] if fields[1] else []
            title = [int(x, 16) for x in fields[2].split()] if fields[2] else []
            upper = [int(x, 16) for x in fields[3].split()] if fields[3] else []
            condition = fields[4]
            if condition:
                continue  # skip conditional rules
            records.append((code, lower, upper))
    records.sort(key=lambda r: r[0])
    return records


def emit(records, out):
    out.write('#ifndef UNICODE_SPECIAL_CASING_H\n')
    out.write('#define UNICODE_SPECIAL_CASING_H\n')
    out.write('#include <stdint.h>\n')
    out.write('#include <stddef.h>\n')
    out.write('typedef struct {\n')
    out.write('    uint32_t code;\n')
    out.write('    uint8_t lower_len;\n')
    out.write('    uint8_t upper_len;\n')
    out.write('    uint32_t lower[3];\n')
    out.write('    uint32_t upper[3];\n')
    out.write('} unicode_special_case_t;\n')
    out.write('static const unicode_special_case_t unicode_special_cases[] = {\n')
    for code, lower, upper in records:
        lower_vals = ', '.join(f'0x{x:04X}' for x in lower)
        upper_vals = ', '.join(f'0x{x:04X}' for x in upper)
        if lower_vals:
            lower_vals += ', '
        if upper_vals:
            upper_vals += ', '
        lower_vals += ', '.join(['0'] * (3 - len(lower)))
        upper_vals += ', '.join(['0'] * (3 - len(upper)))
        out.write(f'    {{0x{code:04X}, {len(lower)}, {len(upper)}, {{{lower_vals}}}, {{{upper_vals}}}}},\n')
    out.write('};\n')
    out.write('static const size_t unicode_special_cases_len = sizeof(unicode_special_cases)/sizeof(unicode_special_cases[0]);\n')
    out.write('#endif /* UNICODE_SPECIAL_CASING_H */\n')


def main(argv):
    if len(argv) != 3:
        print('usage: gen_unicode_special_casing.py SpecialCasing.txt output.h')
        return 1
    recs = parse(argv[1])
    with open(argv[2], 'w', encoding='utf-8') as out:
        emit(recs, out)


if __name__ == '__main__':
    sys.exit(main(sys.argv))
