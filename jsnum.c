#include "jsnum.h"

#include <errno.h>
#include <math.h>
#include <stdint.h>

#ifdef USE_LIBC
#include <stdio.h>
#include <stdlib.h>
#endif

#define JSNUM_TEXT_PRECISION 17

typedef struct jsnum_parse_options_s {
	int allow_plus;
	int allow_leading_dot;
	int allow_trailing_dot;
	int strict_json_integer;
} jsnum_parse_options_t;

static int jsnum_is_ascii_digit(uint8_t ch)
{
	return ch >= '0' && ch <= '9';
}

static int jsnum_is_ascii_alpha(uint8_t ch)
{
	return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
}

static int jsnum_is_ascii_space(uint8_t ch)
{
	switch (ch) {
	case 0x09:
	case 0x0A:
	case 0x0B:
	case 0x0C:
	case 0x0D:
	case 0x20:
		return 1;
	default:
		return 0;
	}
}

static int jsnum_exact_ascii(const uint8_t *src, size_t len, const char *text)
{
	size_t i = 0;

	while (i < len && text[i] != '\0') {
		if (src[i] != (uint8_t)text[i]) {
			return 0;
		}
		i++;
	}
	return i == len && text[i] == '\0';
}

#ifndef USE_LIBC
static long double jsnum_apply_pow10(long double value, int exponent)
{
	static const struct {
		int exponent;
		long double value;
	} powers[] = {
		{ 256, 1e256L },
		{ 128, 1e128L },
		{ 64,  1e64L  },
		{ 32,  1e32L  },
		{ 16,  1e16L  },
		{ 8,   1e8L   },
		{ 4,   1e4L   },
		{ 2,   1e2L   },
		{ 1,   1e1L   },
	};
	size_t i;
	int remaining;

	if (value == 0.0L || exponent == 0) {
		return value;
	}
	if (exponent > 4096) {
		return INFINITY;
	}
	if (exponent < -4096) {
		return 0.0L;
	}

	remaining = exponent < 0 ? -exponent : exponent;
	for (i = 0; i < sizeof(powers) / sizeof(powers[0]) && remaining > 0; i++) {
		while (remaining >= powers[i].exponent) {
			if (exponent > 0) {
				value *= powers[i].value;
			} else {
				value /= powers[i].value;
			}
			remaining -= powers[i].exponent;
		}
	}

	return value;
}

static void jsnum_normalize_decimal(long double *value_ptr, int *exponent_ptr)
{
	static const struct {
		int exponent;
		long double value;
	} powers[] = {
		{ 256, 1e256L },
		{ 128, 1e128L },
		{ 64,  1e64L  },
		{ 32,  1e32L  },
		{ 16,  1e16L  },
		{ 8,   1e8L   },
		{ 4,   1e4L   },
		{ 2,   1e2L   },
		{ 1,   1e1L   },
	};
	long double value = *value_ptr;
	int exponent = 0;
	size_t i;

	if (!(value > 0.0L)) {
		*value_ptr = value;
		*exponent_ptr = 0;
		return;
	}

	if (value >= 10.0L) {
		for (i = 0; i < sizeof(powers) / sizeof(powers[0]); i++) {
			while (value >= powers[i].value) {
				value /= powers[i].value;
				exponent += powers[i].exponent;
			}
		}
		while (value >= 10.0L) {
			value /= 10.0L;
			exponent++;
		}
	} else if (value < 1.0L) {
		for (i = 0; i < sizeof(powers) / sizeof(powers[0]); i++) {
			while (value * powers[i].value < 1.0L) {
				value *= powers[i].value;
				exponent -= powers[i].exponent;
			}
		}
		while (value < 1.0L) {
			value *= 10.0L;
			exponent--;
		}
	}

	if (value >= 10.0L) {
		value /= 10.0L;
		exponent++;
	} else if (value < 1.0L && value > 0.0L) {
		value *= 10.0L;
		exponent--;
	}

	*value_ptr = value;
	*exponent_ptr = exponent;
}

static int jsnum_append_char(char *buf, size_t cap, size_t *len_ptr, char ch)
{
	if (*len_ptr + 1 >= cap) {
		errno = ENOBUFS;
		return -1;
	}
	buf[*len_ptr] = ch;
	(*len_ptr)++;
	buf[*len_ptr] = '\0';
	return 0;
}

static int jsnum_append_repeated(char *buf, size_t cap, size_t *len_ptr, char ch,
		size_t count)
{
	size_t i;

	for (i = 0; i < count; i++) {
		if (jsnum_append_char(buf, cap, len_ptr, ch) < 0) {
			return -1;
		}
	}
	return 0;
}

static int jsnum_append_unsigned(char *buf, size_t cap, size_t *len_ptr,
		unsigned int value, size_t min_digits)
{
	char tmp[16];
	size_t digits = 0;
	size_t i;

	do {
		tmp[digits++] = (char)('0' + (value % 10));
		value /= 10;
	} while (value != 0 && digits < sizeof(tmp));

	while (digits < min_digits && digits < sizeof(tmp)) {
		tmp[digits++] = '0';
	}

	for (i = 0; i < digits; i++) {
		if (jsnum_append_char(buf, cap, len_ptr, tmp[digits - 1 - i]) < 0) {
			return -1;
		}
	}
	return 0;
}

static int jsnum_format_nonlibc(double number, char *buf, size_t cap, size_t *len_ptr)
{
	long double value;
	int digits[JSNUM_TEXT_PRECISION + 1];
	size_t ndigits = JSNUM_TEXT_PRECISION;
	size_t len = 0;
	int exponent = 0;
	size_t i;
	int use_exponent;
	int negative;

	if (buf == NULL || cap == 0) {
		errno = EINVAL;
		return -1;
	}
	buf[0] = '\0';

	if (number != number) {
		if (cap < 4) {
			errno = ENOBUFS;
			return -1;
		}
		buf[0] = 'N';
		buf[1] = 'a';
		buf[2] = 'N';
		buf[3] = '\0';
		if (len_ptr != NULL) {
			*len_ptr = 3;
		}
		return 0;
	}
	if (number == INFINITY) {
		if (cap < 9) {
			errno = ENOBUFS;
			return -1;
		}
		buf[0] = 'I';
		buf[1] = 'n';
		buf[2] = 'f';
		buf[3] = 'i';
		buf[4] = 'n';
		buf[5] = 'i';
		buf[6] = 't';
		buf[7] = 'y';
		buf[8] = '\0';
		if (len_ptr != NULL) {
			*len_ptr = 8;
		}
		return 0;
	}
	if (number == -INFINITY) {
		if (cap < 10) {
			errno = ENOBUFS;
			return -1;
		}
		buf[0] = '-';
		buf[1] = 'I';
		buf[2] = 'n';
		buf[3] = 'f';
		buf[4] = 'i';
		buf[5] = 'n';
		buf[6] = 'i';
		buf[7] = 't';
		buf[8] = 'y';
		buf[9] = '\0';
		if (len_ptr != NULL) {
			*len_ptr = 9;
		}
		return 0;
	}

	negative = signbit(number) != 0;
	if (number == 0.0) {
		if (negative) {
			if (cap < 3) {
				errno = ENOBUFS;
				return -1;
			}
			buf[0] = '-';
			buf[1] = '0';
			buf[2] = '\0';
			if (len_ptr != NULL) {
				*len_ptr = 2;
			}
			return 0;
		}
		if (cap < 2) {
			errno = ENOBUFS;
			return -1;
		}
		buf[0] = '0';
		buf[1] = '\0';
		if (len_ptr != NULL) {
			*len_ptr = 1;
		}
		return 0;
	}

	value = negative ? -(long double)number : (long double)number;
	jsnum_normalize_decimal(&value, &exponent);
	for (i = 0; i < JSNUM_TEXT_PRECISION + 1; i++) {
		int digit = (int)value;

		if (digit < 0) {
			digit = 0;
		} else if (digit > 9) {
			digit = 9;
		}
		digits[i] = digit;
		value = (value - (long double)digit) * 10.0L;
		if (value < 0.0L) {
			value = 0.0L;
		}
	}

	if (digits[JSNUM_TEXT_PRECISION] >= 5) {
		for (i = JSNUM_TEXT_PRECISION; i > 0; i--) {
			if (digits[i - 1] < 9) {
				digits[i - 1]++;
				break;
			}
			digits[i - 1] = 0;
		}
		if (i == 0) {
			digits[0] = 1;
			for (i = 1; i < JSNUM_TEXT_PRECISION; i++) {
				digits[i] = 0;
			}
			exponent++;
		}
	}

	while (ndigits > 1 && digits[ndigits - 1] == 0) {
		ndigits--;
	}

	if (negative) {
		if (jsnum_append_char(buf, cap, &len, '-') < 0) {
			return -1;
		}
	}

	use_exponent = exponent < -4 || exponent >= JSNUM_TEXT_PRECISION;
	if (!use_exponent) {
		if (exponent >= 0) {
			size_t int_digits = (size_t)exponent + 1;

			for (i = 0; i < int_digits; i++) {
				char ch = (i < ndigits) ? (char)('0' + digits[i]) : '0';
				if (jsnum_append_char(buf, cap, &len, ch) < 0) {
					return -1;
				}
			}
			if (int_digits < ndigits) {
				if (jsnum_append_char(buf, cap, &len, '.') < 0) {
					return -1;
				}
				for (i = int_digits; i < ndigits; i++) {
					if (jsnum_append_char(buf, cap, &len, (char)('0' + digits[i])) < 0) {
						return -1;
					}
				}
			}
		} else {
			if (jsnum_append_char(buf, cap, &len, '0') < 0) {
				return -1;
			}
			if (jsnum_append_char(buf, cap, &len, '.') < 0) {
				return -1;
			}
			if (jsnum_append_repeated(buf, cap, &len, '0',
					(size_t)(-exponent - 1)) < 0) {
				return -1;
			}
			for (i = 0; i < ndigits; i++) {
				if (jsnum_append_char(buf, cap, &len, (char)('0' + digits[i])) < 0) {
					return -1;
				}
			}
		}
	} else {
		if (jsnum_append_char(buf, cap, &len, (char)('0' + digits[0])) < 0) {
			return -1;
		}
		if (ndigits > 1) {
			if (jsnum_append_char(buf, cap, &len, '.') < 0) {
				return -1;
			}
			for (i = 1; i < ndigits; i++) {
				if (jsnum_append_char(buf, cap, &len, (char)('0' + digits[i])) < 0) {
					return -1;
				}
			}
		}
		if (jsnum_append_char(buf, cap, &len, 'e') < 0) {
			return -1;
		}
		if (exponent < 0) {
			if (jsnum_append_char(buf, cap, &len, '-') < 0) {
				return -1;
			}
		} else {
			if (jsnum_append_char(buf, cap, &len, '+') < 0) {
				return -1;
			}
		}
		if (jsnum_append_unsigned(buf, cap, &len,
				(unsigned int)(exponent < 0 ? -exponent : exponent), 2) < 0) {
			return -1;
		}
	}

	if (len_ptr != NULL) {
		*len_ptr = len;
	}
	return 0;
}

static int jsnum_parse_decimal_nonlibc(const uint8_t *src, size_t len,
		const jsnum_parse_options_t *options, double *number_ptr, int *valid_ptr)
{
	size_t pos = 0;
	int negative = 0;
	size_t int_start;
	size_t int_len;
	size_t frac_start = 0;
	size_t frac_len = 0;
	int saw_dot = 0;
	int exponent_negative = 0;
	int exponent = 0;
	int scale;
	long double value = 0.0L;

	if (number_ptr == NULL || valid_ptr == NULL || options == NULL) {
		errno = EINVAL;
		return -1;
	}

	*valid_ptr = 0;
	if (len == 0) {
		return 0;
	}

	if (src[pos] == '-') {
		negative = 1;
		pos++;
	} else if (src[pos] == '+') {
		if (!options->allow_plus) {
			return 0;
		}
		pos++;
	}
	if (pos == len) {
		return 0;
	}

	int_start = pos;
	while (pos < len && jsnum_is_ascii_digit(src[pos])) {
		pos++;
	}
	int_len = pos - int_start;

	if (pos < len && src[pos] == '.') {
		saw_dot = 1;
		pos++;
		frac_start = pos;
		while (pos < len && jsnum_is_ascii_digit(src[pos])) {
			pos++;
		}
		frac_len = pos - frac_start;
	}

	if (int_len == 0 && !(options->allow_leading_dot && frac_len > 0)) {
		return 0;
	}
	if (saw_dot && frac_len == 0 && !options->allow_trailing_dot) {
		return 0;
	}
	if (options->strict_json_integer && int_len > 1 && src[int_start] == '0') {
		return 0;
	}

	if (pos < len && (src[pos] == 'e' || src[pos] == 'E')) {
		size_t exp_start;

		pos++;
		if (pos == len) {
			return 0;
		}
		if (src[pos] == '+' || src[pos] == '-') {
			exponent_negative = src[pos] == '-';
			pos++;
		}
		exp_start = pos;
		while (pos < len && jsnum_is_ascii_digit(src[pos])) {
			if (exponent < 100000) {
				exponent = exponent * 10 + (int)(src[pos] - '0');
			}
			pos++;
		}
		if (pos == exp_start) {
			return 0;
		}
	}

	if (pos != len) {
		return 0;
	}

	for (pos = int_start; pos < int_start + int_len; pos++) {
		value = value * 10.0L + (long double)(src[pos] - '0');
	}
	for (pos = frac_start; pos < frac_start + frac_len; pos++) {
		value = value * 10.0L + (long double)(src[pos] - '0');
	}

	scale = exponent_negative ? -exponent : exponent;
	if (frac_len > 0) {
		scale -= (int)frac_len;
	}
	value = jsnum_apply_pow10(value, scale);
	if (negative) {
		value = -value;
	}

	*number_ptr = (double)value;
	*valid_ptr = 1;
	return 0;
}

static double jsnum_remainder_nonlibc(double left, double right)
{
	union {
		double f;
		uint64_t i;
	} ux = { left }, uy = { right };
	uint64_t sign;
	uint64_t lx;
	uint64_t ly;
	uint64_t diff;
	uint64_t carry;
	int ex;
	int ey;

	ex = (int)((ux.i >> 52) & 0x7ff);
	ey = (int)((uy.i >> 52) & 0x7ff);
	sign = ux.i & (UINT64_C(1) << 63);

	if ((uy.i << 1) == 0 || right != right || ex == 0x7ff) {
		return (left * right) / (left * right);
	}
	if ((ux.i << 1) <= (uy.i << 1)) {
		if ((ux.i << 1) == (uy.i << 1)) {
			return 0.0 * left;
		}
		return left;
	}

	if (ex == 0) {
		lx = ux.i << 12;
		while ((lx & (UINT64_C(1) << 63)) == 0) {
			lx <<= 1;
			ex--;
		}
		lx = ux.i << (-ex + 1);
	} else {
		lx = (ux.i & ((UINT64_C(1) << 52) - 1)) | (UINT64_C(1) << 52);
	}

	if (ey == 0) {
		ly = uy.i << 12;
		while ((ly & (UINT64_C(1) << 63)) == 0) {
			ly <<= 1;
			ey--;
		}
		ly = uy.i << (-ey + 1);
	} else {
		ly = (uy.i & ((UINT64_C(1) << 52) - 1)) | (UINT64_C(1) << 52);
	}

	while (ex > ey) {
		diff = lx - ly;
		carry = diff >> 63;
		if (carry == 0) {
			if (diff == 0) {
				return 0.0 * left;
			}
			lx = diff;
		}
		lx <<= 1;
		ex--;
	}

	diff = lx - ly;
	carry = diff >> 63;
	if (carry == 0) {
		if (diff == 0) {
			return 0.0 * left;
		}
		lx = diff;
	}

	while ((lx >> 52) == 0) {
		lx <<= 1;
		ex--;
	}

	if (ex > 0) {
		lx -= UINT64_C(1) << 52;
		lx |= (uint64_t)ex << 52;
	} else {
		lx >>= (unsigned int)(-ex + 1);
	}

	ux.i = lx | sign;
	return ux.f;
}
#endif

static int jsnum_parse_decimal_core(const uint8_t *src, size_t len,
		const jsnum_parse_options_t *options, double *number_ptr, int *valid_ptr)
{
#ifdef USE_LIBC
	size_t pos = 0;
	char buf[128];
	char *endptr;

	if (number_ptr == NULL || valid_ptr == NULL || options == NULL) {
		errno = EINVAL;
		return -1;
	}
	*valid_ptr = 0;
	if (len == 0 || len >= sizeof(buf)) {
		return 0;
	}
	if (!options->allow_plus && src[pos] == '+') {
		return 0;
	}
	if (options->strict_json_integer) {
		size_t check = 0;

		if (src[check] == '-') {
			check++;
		}
		if (check < len && src[check] == '0' && check + 1 < len
				&& jsnum_is_ascii_digit(src[check + 1])) {
			return 0;
		}
	}
	if (!options->allow_leading_dot && src[pos] == '.') {
		return 0;
	}
	if (!options->allow_trailing_dot && len > 0 && src[len - 1] == '.') {
		return 0;
	}

	while (pos < len) {
		uint8_t ch = src[pos];

		if (!(jsnum_is_ascii_digit(ch) || ch == '.' || ch == 'e' || ch == 'E'
				|| ch == '+' || ch == '-')) {
			return 0;
		}
		buf[pos] = (char)ch;
		pos++;
	}
	buf[len] = '\0';

	*number_ptr = strtod(buf, &endptr);
	if ((size_t)(endptr - buf) != len) {
		return 0;
	}
	*valid_ptr = 1;
	return 0;
#else
	return jsnum_parse_decimal_nonlibc(src, len, options, number_ptr, valid_ptr);
#endif
}

int jsnum_parse_json(const uint8_t *src, size_t len, double *number_ptr)
{
	static const jsnum_parse_options_t options = {
		0, 0, 0, 1
	};
	int valid = 0;

	if (src == NULL || number_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsnum_parse_decimal_core(src, len, &options, number_ptr, &valid) < 0) {
		return -1;
	}
	if (!valid) {
		errno = EINVAL;
		return -1;
	}
	return 0;
}

int jsnum_parse_string(const uint8_t *src, size_t len, double *number_ptr)
{
	static const jsnum_parse_options_t options = {
		1, 1, 1, 0
	};
	size_t start = 0;
	size_t end = len;
	size_t trimmed_len;
	size_t sign = 0;
	int valid = 0;

	if (src == NULL || number_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}

	while (start < len && jsnum_is_ascii_space(src[start])) {
		start++;
	}
	while (end > start && jsnum_is_ascii_space(src[end - 1])) {
		end--;
	}

	trimmed_len = end - start;
	if (trimmed_len == 0) {
		*number_ptr = 0.0;
		return 0;
	}

	if (jsnum_exact_ascii(src + start, trimmed_len, "Infinity")
			|| jsnum_exact_ascii(src + start, trimmed_len, "+Infinity")) {
		*number_ptr = INFINITY;
		return 0;
	}
	if (jsnum_exact_ascii(src + start, trimmed_len, "-Infinity")) {
		*number_ptr = -INFINITY;
		return 0;
	}

	if (src[start] == '+' || src[start] == '-') {
		sign = 1;
	}
	if (sign < trimmed_len && jsnum_is_ascii_alpha(src[start + sign])) {
		*number_ptr = NAN;
		return 0;
	}
	if (trimmed_len > sign + 1 && src[start + sign] == '0'
			&& (src[start + sign + 1] == 'x'
			 || src[start + sign + 1] == 'X'
			 || src[start + sign + 1] == 'o'
			 || src[start + sign + 1] == 'O'
			 || src[start + sign + 1] == 'b'
			 || src[start + sign + 1] == 'B')) {
		*number_ptr = NAN;
		return 0;
	}

	if (jsnum_parse_decimal_core(src + start, trimmed_len, &options, number_ptr,
			&valid) < 0) {
		return -1;
	}
	if (!valid) {
		*number_ptr = NAN;
	}
	return 0;
}

int jsnum_format(double number, char *buf, size_t cap, size_t *len_ptr)
{
#ifdef USE_LIBC
	const char *text = NULL;
	size_t len = 0;
	int rc;

	if (buf == NULL || cap == 0) {
		errno = EINVAL;
		return -1;
	}
	if (number != number) {
		text = "NaN";
	} else if (number == INFINITY) {
		text = "Infinity";
	} else if (number == -INFINITY) {
		text = "-Infinity";
	}
	if (text != NULL) {
		while (text[len] != '\0') {
			len++;
		}
		if (len + 1 > cap) {
			errno = ENOBUFS;
			return -1;
		}
		for (rc = 0; rc < (int)len; rc++) {
			buf[rc] = text[rc];
		}
		buf[len] = '\0';
		if (len_ptr != NULL) {
			*len_ptr = len;
		}
		return 0;
	}

	rc = snprintf(buf, cap, "%.17g", number);
	if (rc < 0) {
		errno = EINVAL;
		return -1;
	}
	if ((size_t)rc >= cap) {
		errno = ENOBUFS;
		return -1;
	}
	if (len_ptr != NULL) {
		*len_ptr = (size_t)rc;
	}
	return 0;
#else
	return jsnum_format_nonlibc(number, buf, cap, len_ptr);
#endif
}

double jsnum_remainder(double left, double right)
{
#ifdef USE_LIBC
	return fmod(left, right);
#else
	return jsnum_remainder_nonlibc(left, right);
#endif
}
