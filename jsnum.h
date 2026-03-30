#ifndef JSNUM_H
#define JSNUM_H

#include <stddef.h>
#include <stdint.h>

int jsnum_parse_json(const uint8_t *src, size_t len, double *number_ptr);
int jsnum_parse_string(const uint8_t *src, size_t len, double *number_ptr);
int jsnum_format(double number, char *buf, size_t cap, size_t *len_ptr);
double jsnum_remainder(double left, double right);

#endif
