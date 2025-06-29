#include <stdio.h>
#include "unicode_collation.h"

int main() {
    size_t n = unicode_collation_db_len < 5 ? unicode_collation_db_len : 5;
    for (size_t i = 0; i < n; i++) {
        printf("U+%04X -> %u %u %u\n",
               unicode_collation_db[i].code,
               unicode_collation_db[i].primary,
               unicode_collation_db[i].secondary,
               unicode_collation_db[i].tertiary);
    }
    return 0;
}
