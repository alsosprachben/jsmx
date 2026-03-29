#!/usr/bin/env python3
import sys

UNICODE_LOCALE_TURKIC = 0x01
UNICODE_LOCALE_LITHUANIAN = 0x02
LOCALE_MASKS = {
    'tr': UNICODE_LOCALE_TURKIC,
    'az': UNICODE_LOCALE_TURKIC,
    'lt': UNICODE_LOCALE_LITHUANIAN,
}


def parse(path):
    records = []
    locale_records = []
    seen_locale = set()
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
            if not condition:
                records.append((code, lower, upper))
                continue

            mask = 0
            supported = True
            for token in condition.split():
                locale_mask = LOCALE_MASKS.get(token.lower())
                if locale_mask is None:
                    supported = False
                    break
                mask |= locale_mask
            if not supported or mask == 0:
                continue

            key = (code, mask, tuple(lower), tuple(upper))
            if key in seen_locale:
                continue
            seen_locale.add(key)
            locale_records.append((code, mask, lower, upper))
    records.sort(key=lambda r: r[0])
    locale_records.sort(key=lambda r: (r[0], r[1]))
    return records, locale_records


def emit_records(records, out):
    for code, *rest in records:
        lower = rest[-2]
        upper = rest[-1]
        fields = [f'0x{code:04X}']
        if len(rest) == 3:
            fields.append(str(rest[0]))
        fields.extend([str(len(lower)), str(len(upper))])
        lower_vals = ', '.join(f'0x{x:04X}' for x in lower)
        upper_vals = ', '.join(f'0x{x:04X}' for x in upper)
        if lower_vals:
            lower_vals += ', '
        if upper_vals:
            upper_vals += ', '
        lower_vals += ', '.join(['0'] * (3 - len(lower)))
        upper_vals += ', '.join(['0'] * (3 - len(upper)))
        fields.append(f'{{{lower_vals}}}')
        fields.append(f'{{{upper_vals}}}')
        out.write(f'    {{{", ".join(fields)}}},\n')


def emit(records, locale_records, out):
    out.write('#ifndef UNICODE_SPECIAL_CASING_H\n')
    out.write('#define UNICODE_SPECIAL_CASING_H\n')
    out.write('#include <stdint.h>\n')
    out.write('#include <stddef.h>\n')
    out.write('#define UNICODE_LOCALE_TURKIC 0x01\n')
    out.write('#define UNICODE_LOCALE_LITHUANIAN 0x02\n')
    out.write('typedef struct {\n')
    out.write('    uint32_t code;\n')
    out.write('    uint8_t lower_len;\n')
    out.write('    uint8_t upper_len;\n')
    out.write('    uint32_t lower[3];\n')
    out.write('    uint32_t upper[3];\n')
    out.write('} unicode_special_case_t;\n')
    out.write('typedef struct {\n')
    out.write('    uint32_t code;\n')
    out.write('    uint8_t locale;\n')
    out.write('    uint8_t lower_len;\n')
    out.write('    uint8_t upper_len;\n')
    out.write('    uint32_t lower[3];\n')
    out.write('    uint32_t upper[3];\n')
    out.write('} unicode_special_case_locale_t;\n')
    out.write('static const unicode_special_case_t unicode_special_cases[] = {\n')
    emit_records(records, out)
    out.write('};\n')
    out.write('static const size_t unicode_special_cases_len = sizeof(unicode_special_cases) / sizeof(unicode_special_cases[0]);\n')
    out.write('static const unicode_special_case_locale_t unicode_special_cases_locale[] = {\n')
    if locale_records:
        emit_records(locale_records, out)
    else:
        out.write('    {0, 0, 0, 0, {0, 0, 0}, {0, 0, 0}},\n')
    out.write('};\n')
    out.write(f'static const size_t unicode_special_cases_locale_len = {len(locale_records)};\n')
    out.write('#endif /* UNICODE_SPECIAL_CASING_H */\n')


def main(argv):
    if len(argv) != 3:
        print('usage: gen_unicode_special_casing.py SpecialCasing.txt output.h')
        return 1
    recs, locale_recs = parse(argv[1])
    with open(argv[2], 'w', encoding='utf-8') as out:
        emit(recs, locale_recs, out)


if __name__ == '__main__':
    sys.exit(main(sys.argv))
