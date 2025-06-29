#!/usr/bin/env python3
import sys

def parse(path):
    lower=[]
    upper=[]
    with open(path, 'r', encoding='utf-8') as f:
        for line in f:
            fields=line.strip().split(';')
            if len(fields) < 13:
                continue
            code=int(fields[0],16)
            upper_f=fields[12]
            lower_f=fields[13]
            if upper_f and upper_f != "":
                up=int(upper_f,16)
                if up!=code:
                    upper.append((code,up))
            if lower_f and lower_f != "":
                lo=int(lower_f,16)
                if lo!=code:
                    lower.append((code,lo))
    return upper,lower

def emit(upper, lower, out):
    out.write('#ifndef UNICODE_CASE_DATA_H\n')
    out.write('#define UNICODE_CASE_DATA_H\n')
    out.write('#include <stdint.h>\n')
    out.write('#include <stddef.h>\n')
    out.write('typedef struct { uint32_t from; uint32_t to; } unicode_case_map_t;\n')
    out.write('static const unicode_case_map_t unicode_tolower_map[] = {\n')
    for f,t in lower:
        out.write(f'    {{0x{f:04X}, 0x{t:04X}}},\n')
    out.write('};\n')
    out.write('static const size_t unicode_tolower_map_len = sizeof(unicode_tolower_map)/sizeof(unicode_tolower_map[0]);\n')
    out.write('static const unicode_case_map_t unicode_toupper_map[] = {\n')
    for f,t in upper:
        out.write(f'    {{0x{f:04X}, 0x{t:04X}}},\n')
    out.write('};\n')
    out.write('static const size_t unicode_toupper_map_len = sizeof(unicode_toupper_map)/sizeof(unicode_toupper_map[0]);\n')
    out.write('#endif /* UNICODE_CASE_DATA_H */\n')

def main(argv):
    if len(argv)!=3:
        print('usage: gen_unicode_case.py UnicodeData.txt output.h')
        return 1
    up,lo=parse(argv[1])
    with open(argv[2],'w',encoding='utf-8') as out:
        emit(up,lo,out)

if __name__ == '__main__':
    main(sys.argv)
