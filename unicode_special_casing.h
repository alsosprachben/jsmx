#ifndef UNICODE_SPECIAL_CASING_H
#define UNICODE_SPECIAL_CASING_H
#include <stdint.h>
#include <stddef.h>
typedef struct {
    uint32_t code;
    uint8_t lower_len;
    uint8_t upper_len;
    uint32_t lower[3];
    uint32_t upper[3];
} unicode_special_case_t;
static const unicode_special_case_t unicode_special_cases[] = {
    {0x00DF, 2, 2, {0x0073, 0x0073, 0}, {0x0053, 0x0053, 0}},
    {0x0130, 2, 0, {0x0069, 0x0307, 0}, {0, 0, 0}},
    {0xFB00, 2, 2, {0x0066, 0x0066, 0}, {0x0046, 0x0046, 0}},
};
static const size_t unicode_special_cases_len = sizeof(unicode_special_cases)/sizeof(unicode_special_cases[0]);
#endif /* UNICODE_SPECIAL_CASING_H */
