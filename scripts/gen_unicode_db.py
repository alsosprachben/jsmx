#!/usr/bin/env python3
import sys

MAX_FIELDS = 15


def compute_max_lengths(path):
    max_len = [0]*MAX_FIELDS
    with open(path, 'r', encoding='utf-8') as f:
        for line in f:
            fields = line.rstrip('\n').split(';')
            for i, field in enumerate(fields):
                if len(field) > max_len[i]:
                    max_len[i] = len(field)
    return max_len


def parse_records(path):
    records = []
    with open(path, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.rstrip('\n')
            if not line:
                continue
            fields = line.split(';')
            if len(fields) < MAX_FIELDS:
                fields += [''] * (MAX_FIELDS - len(fields))
            code = int(fields[0], 16)
            # parse simple case mappings as ints (hex) or default to 0
            try:
                upper = int(fields[12], 16) if fields[12] else 0
            except ValueError:
                upper = 0
            try:
                lower = int(fields[13], 16) if fields[13] else 0
            except ValueError:
                lower = 0
            try:
                title = int(fields[14], 16) if fields[14] else 0
            except ValueError:
                title = 0
            # other fields remain as strings
            records.append((code,) + tuple(fields[1:12]) + (upper, lower, title))
    return records


def emit(records, max_len, out):
    out.write('#ifndef UNICODE_DB_H\n')
    out.write('#define UNICODE_DB_H\n')
    out.write('#include <stdint.h>\n')
    out.write('#include <stddef.h>\n')
    out.write('/* maximum field lengths including null terminator */\n')
    macros = [
        ("UNICODE_NAME_MAX", max_len[1] + 1),
        ("UNICODE_GC_MAX", max_len[2] + 1),
        ("UNICODE_CC_MAX", max_len[3] + 1),
        ("UNICODE_BIDI_MAX", max_len[4] + 1),
        ("UNICODE_DECOMP_MAX", max_len[5] + 1),
        ("UNICODE_DECIMAL_MAX", max_len[6] + 1),
        ("UNICODE_DIGIT_MAX", max_len[7] + 1),
        ("UNICODE_NUMERIC_MAX", max_len[8] + 1),
        ("UNICODE_MIRRORED_MAX", max_len[9] + 1),
        ("UNICODE_UNICODE1_NAME_MAX", max_len[10] + 1),
        ("UNICODE_ISO_COMMENT_MAX", max_len[11] + 1),
    ]
    for name, val in macros:
        out.write(f'#define {name} {val}\n')
    out.write('typedef struct {\n')
    out.write('    uint32_t code;\n')
    out.write('    char name[UNICODE_NAME_MAX];\n')
    out.write('    char general_category[UNICODE_GC_MAX];\n')
    out.write('    char combining_class[UNICODE_CC_MAX];\n')
    out.write('    char bidi_class[UNICODE_BIDI_MAX];\n')
    out.write('    char decomposition[UNICODE_DECOMP_MAX];\n')
    out.write('    char decimal_value[UNICODE_DECIMAL_MAX];\n')
    out.write('    char digit_value[UNICODE_DIGIT_MAX];\n')
    out.write('    char numeric_value[UNICODE_NUMERIC_MAX];\n')
    out.write('    char mirrored[UNICODE_MIRRORED_MAX];\n')
    out.write('    char unicode1_name[UNICODE_UNICODE1_NAME_MAX];\n')
    out.write('    char iso_comment[UNICODE_ISO_COMMENT_MAX];\n')
    out.write('    uint32_t simple_upper;\n')
    out.write('    uint32_t simple_lower;\n')
    out.write('    uint32_t simple_title;\n')
    out.write('} unicode_record_t;\n')
    out.write('static const unicode_record_t unicode_db[] = {\n')
    for rec in records:
        fields = []
        for field in rec[1:12]:
            esc = field.replace('\\', '\\\\').replace('"', '\\"')
            fields.append(f'"{esc}"')
        out.write(f'    {{0x{rec[0]:04X}, {", ".join(fields)}, 0x{rec[12]:04X}, 0x{rec[13]:04X}, 0x{rec[14]:04X}}},\n')
    out.write('};\n')
    out.write('static const size_t unicode_db_len = sizeof(unicode_db)/sizeof(unicode_db[0]);\n')
    out.write('#endif /* UNICODE_DB_H */\n')


def main(argv):
    if len(argv) != 3:
        print('usage: gen_unicode_db.py UnicodeData.txt output.h')
        return 1
    path_in = argv[1]
    path_out = argv[2]
    max_len = compute_max_lengths(path_in)
    records = parse_records(path_in)
    with open(path_out, 'w', encoding='utf-8') as out:
        emit(records, max_len, out)


if __name__ == '__main__':
    sys.exit(main(sys.argv))
