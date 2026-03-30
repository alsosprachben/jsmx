#include "jsmethod.h"
#include "jsnum.h"

#include <errno.h>
#include <math.h>
#include <string.h>

static void
jsmethod_error_set(jsmethod_error_t *error, jsmethod_error_kind_t kind,
		const char *message)
{
	if (error == NULL) {
		return;
	}
	error->kind = kind;
	error->message = message;
}

void
jsmethod_error_clear(jsmethod_error_t *error)
{
	if (error == NULL) {
		return;
	}
	error->kind = JSMETHOD_ERROR_NONE;
	error->message = NULL;
}

jsmethod_value_t
jsmethod_value_undefined(void)
{
	jsmethod_value_t value = {0};
	value.kind = JSMETHOD_VALUE_UNDEFINED;
	return value;
}

jsmethod_value_t
jsmethod_value_null(void)
{
	jsmethod_value_t value = {0};
	value.kind = JSMETHOD_VALUE_NULL;
	return value;
}

jsmethod_value_t
jsmethod_value_bool(int boolean)
{
	jsmethod_value_t value = {0};
	value.kind = JSMETHOD_VALUE_BOOL;
	value.as.boolean = boolean != 0;
	return value;
}

jsmethod_value_t
jsmethod_value_number(double number)
{
	jsmethod_value_t value = {0};
	value.kind = JSMETHOD_VALUE_NUMBER;
	value.as.number = number;
	return value;
}

jsmethod_value_t
jsmethod_value_string_utf8(const uint8_t *bytes, size_t len)
{
	jsmethod_value_t value = {0};
	value.kind = JSMETHOD_VALUE_STRING_UTF8;
	value.as.utf8.bytes = bytes;
	value.as.utf8.len = len;
	return value;
}

jsmethod_value_t
jsmethod_value_string_utf16(const uint16_t *codeunits, size_t len)
{
	jsmethod_value_t value = {0};
	value.kind = JSMETHOD_VALUE_STRING_UTF16;
	value.as.utf16.codeunits = codeunits;
	value.as.utf16.len = len;
	return value;
}

jsmethod_value_t
jsmethod_value_symbol(void)
{
	jsmethod_value_t value = {0};
	value.kind = JSMETHOD_VALUE_SYMBOL;
	return value;
}

jsmethod_value_t
jsmethod_value_coercible(void *ctx, jsmethod_to_string_fn fn)
{
	jsmethod_value_t value = {0};
	value.kind = JSMETHOD_VALUE_COERCIBLE;
	value.as.coercible.ctx = ctx;
	value.as.coercible.fn = fn;
	return value;
}

static int
jsmethod_ascii_to_utf16(jsstr16_t *out, const char *text)
{
	size_t len = strlen(text);
	size_t i;

	if (out == NULL || (out->cap > 0 && out->codeunits == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if (out->cap < len) {
		errno = ENOBUFS;
		return -1;
	}
	for (i = 0; i < len; i++) {
		out->codeunits[i] = (uint8_t)text[i];
	}
	out->len = len;
	return 0;
}

static const char *
jsmethod_number_text(double number, char buf[64])
{
	if (jsnum_format(number, buf, 64, NULL) < 0) {
		if (number != number) {
			return "NaN";
		}
		if (isinf(number)) {
			return number < 0 ? "-Infinity" : "Infinity";
		}
		return "0";
	}
	return buf;
}

static int
jsmethod_value_utf16_len(jsmethod_value_t value, int require_object_coercible,
		size_t *len_ptr, jsmethod_error_t *error)
{
	char numbuf[64];
	const char *text;

	if (len_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}

	switch (value.kind) {
	case JSMETHOD_VALUE_UNDEFINED:
		if (require_object_coercible) {
			jsmethod_error_set(error, JSMETHOD_ERROR_TYPE,
					"undefined is not object-coercible");
			return -1;
		}
		*len_ptr = sizeof("undefined") - 1;
		return 0;
	case JSMETHOD_VALUE_NULL:
		if (require_object_coercible) {
			jsmethod_error_set(error, JSMETHOD_ERROR_TYPE,
					"null is not object-coercible");
			return -1;
		}
		*len_ptr = sizeof("null") - 1;
		return 0;
	case JSMETHOD_VALUE_BOOL:
		*len_ptr = value.as.boolean ? sizeof("true") - 1 : sizeof("false") - 1;
		return 0;
	case JSMETHOD_VALUE_NUMBER:
		text = jsmethod_number_text(value.as.number, numbuf);
		*len_ptr = strlen(text);
		return 0;
	case JSMETHOD_VALUE_STRING_UTF8:
	{
		jsstr8_t s;
		jsstr8_init_from_buf(&s, (const char *)value.as.utf8.bytes,
				value.as.utf8.len);
		s.len = value.as.utf8.len;
		*len_ptr = jsstr8_get_utf16len(&s);
		return 0;
	}
	case JSMETHOD_VALUE_STRING_UTF16:
		*len_ptr = value.as.utf16.len;
		return 0;
	case JSMETHOD_VALUE_SYMBOL:
		jsmethod_error_set(error, JSMETHOD_ERROR_TYPE,
				"symbol cannot be converted to string");
		return -1;
	case JSMETHOD_VALUE_COERCIBLE:
		errno = ENOTSUP;
		return -1;
	default:
		errno = EINVAL;
		return -1;
	}
}

int
jsmethod_value_to_string(jsstr16_t *out, jsmethod_value_t value,
		int require_object_coercible, jsmethod_error_t *error)
{
	char numbuf[64];
	const char *text;

	if (out == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);

	switch (value.kind) {
	case JSMETHOD_VALUE_UNDEFINED:
		if (require_object_coercible) {
			jsmethod_error_set(error, JSMETHOD_ERROR_TYPE,
					"undefined is not object-coercible");
			return -1;
		}
		return jsmethod_ascii_to_utf16(out, "undefined");
	case JSMETHOD_VALUE_NULL:
		if (require_object_coercible) {
			jsmethod_error_set(error, JSMETHOD_ERROR_TYPE,
					"null is not object-coercible");
			return -1;
		}
		return jsmethod_ascii_to_utf16(out, "null");
	case JSMETHOD_VALUE_BOOL:
		return jsmethod_ascii_to_utf16(out,
				value.as.boolean ? "true" : "false");
	case JSMETHOD_VALUE_NUMBER:
		text = jsmethod_number_text(value.as.number, numbuf);
		return jsmethod_ascii_to_utf16(out, text);
	case JSMETHOD_VALUE_STRING_UTF8:
		if (jsstr16_set_from_utf8(out, value.as.utf8.bytes, value.as.utf8.len) !=
		    value.as.utf8.len) {
			errno = ENOBUFS;
			return -1;
		}
		return 0;
	case JSMETHOD_VALUE_STRING_UTF16:
		if (jsstr16_set_from_utf16(out, value.as.utf16.codeunits,
			    value.as.utf16.len) != value.as.utf16.len) {
			errno = ENOBUFS;
			return -1;
		}
		return 0;
	case JSMETHOD_VALUE_SYMBOL:
		jsmethod_error_set(error, JSMETHOD_ERROR_TYPE,
				"symbol cannot be converted to string");
		return -1;
	case JSMETHOD_VALUE_COERCIBLE:
		if (value.as.coercible.fn == NULL) {
			errno = EINVAL;
			return -1;
		}
		out->len = 0;
		if (value.as.coercible.fn(value.as.coercible.ctx, out, error) < 0) {
			if (error != NULL && error->kind == JSMETHOD_ERROR_NONE) {
				jsmethod_error_set(error, JSMETHOD_ERROR_ABRUPT,
						"coercion callback failed");
			}
			return -1;
		}
		return 0;
	default:
		errno = EINVAL;
		return -1;
	}
}

static int
jsmethod_this_to_string(jsstr16_t *out, jsmethod_value_t this_value,
		jsmethod_error_t *error)
{
	return jsmethod_value_to_string(out, this_value, 1, error);
}

static int
jsmethod_probe_coercible_to_string(jsmethod_value_t value,
		int require_object_coercible, jsmethod_error_t *error)
{
	uint16_t probe_storage[1];
	jsstr16_t probe;

	if (value.kind != JSMETHOD_VALUE_COERCIBLE) {
		errno = EINVAL;
		return -1;
	}

	jsstr16_init_from_buf(&probe, (const char *)probe_storage,
			sizeof(probe_storage));
	if (jsmethod_value_to_string(&probe, value, require_object_coercible,
			error) < 0) {
		if (error != NULL && error->kind != JSMETHOD_ERROR_NONE) {
			return -1;
		}
		if (errno == ENOBUFS) {
			errno = ENOTSUP;
		}
		return -1;
	}

	errno = ENOTSUP;
	return -1;
}

static int
jsmethod_measure_value_utf16_len(jsmethod_value_t value,
		int require_object_coercible, size_t *len_ptr,
		jsmethod_error_t *error)
{
	if (value.kind == JSMETHOD_VALUE_COERCIBLE) {
		return jsmethod_probe_coercible_to_string(value,
				require_object_coercible, error);
	}
	return jsmethod_value_utf16_len(value, require_object_coercible, len_ptr,
			error);
}

static int
jsmethod_parse_utf16_ascii_number(const uint16_t *codeunits, size_t len,
		double *number_ptr)
{
	size_t i;

	if (number_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	for (i = 0; i < len; i++) {
		if (codeunits[i] > 0x7F) {
			*number_ptr = NAN;
			return 0;
		}
	}
	{
		uint8_t bytes[len ? len : 1];

		for (i = 0; i < len; i++) {
			bytes[i] = (uint8_t)codeunits[i];
		}
		return jsnum_parse_string(bytes, len, number_ptr);
	}
}

static int
jsmethod_value_to_number(jsmethod_value_t value, double *number_ptr,
		jsmethod_error_t *error)
{
	switch (value.kind) {
	case JSMETHOD_VALUE_UNDEFINED:
		*number_ptr = NAN;
		return 0;
	case JSMETHOD_VALUE_NULL:
		*number_ptr = 0.0;
		return 0;
	case JSMETHOD_VALUE_BOOL:
		*number_ptr = value.as.boolean ? 1.0 : 0.0;
		return 0;
	case JSMETHOD_VALUE_NUMBER:
		*number_ptr = value.as.number;
		return 0;
	case JSMETHOD_VALUE_STRING_UTF8:
		return jsnum_parse_string(value.as.utf8.bytes, value.as.utf8.len,
				number_ptr);
	case JSMETHOD_VALUE_STRING_UTF16:
		return jsmethod_parse_utf16_ascii_number(value.as.utf16.codeunits,
				value.as.utf16.len, number_ptr);
	case JSMETHOD_VALUE_SYMBOL:
		jsmethod_error_set(error, JSMETHOD_ERROR_TYPE,
				"symbol cannot be converted to number");
		return -1;
	case JSMETHOD_VALUE_COERCIBLE:
		return jsmethod_probe_coercible_to_string(value, 0, error);
	default:
		errno = EINVAL;
		return -1;
	}
}

static int
jsmethod_value_to_integer_or_infinity(jsmethod_value_t value,
		int nan_is_pos_inf, double *integer_ptr, jsmethod_error_t *error)
{
	double number;

	if (integer_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsmethod_value_to_number(value, &number, error) < 0) {
		return -1;
	}
	if (number == 0.0) {
		*integer_ptr = 0.0;
		return 0;
	}
	if (isnan(number)) {
		*integer_ptr = nan_is_pos_inf ? INFINITY : 0.0;
		return 0;
	}
	if (isinf(number)) {
		*integer_ptr = number;
		return 0;
	}
	if (number > 0.0) {
		if (number >= (double)UINT64_MAX) {
			*integer_ptr = INFINITY;
			return 0;
		}
		*integer_ptr = (double)(uint64_t)number;
		return 0;
	}
	if (-number >= (double)UINT64_MAX) {
		*integer_ptr = -INFINITY;
		return 0;
	}
	*integer_ptr = -(double)(uint64_t)(-number);
	return 0;
}

static size_t
jsmethod_clamp_position(size_t len, double position)
{
	if (position <= 0.0) {
		return 0;
	}
	if (isinf(position) || position >= (double)len) {
		return len;
	}
	return (size_t)position;
}

static int
jsmethod_start_position(size_t len, int have_position,
		jsmethod_value_t position_value, size_t *start_ptr,
		jsmethod_error_t *error)
{
	double position;

	if (start_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (!have_position || position_value.kind == JSMETHOD_VALUE_UNDEFINED) {
		*start_ptr = 0;
		return 0;
	}
	if (jsmethod_value_to_integer_or_infinity(position_value, 0, &position,
			error) < 0) {
		return -1;
	}
	*start_ptr = jsmethod_clamp_position(len, position);
	return 0;
}

static int
jsmethod_last_index_position(size_t len, int have_position,
		jsmethod_value_t position_value, size_t *start_ptr,
		jsmethod_error_t *error)
{
	double position;

	if (start_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (!have_position || position_value.kind == JSMETHOD_VALUE_UNDEFINED) {
		*start_ptr = len;
		return 0;
	}
	if (jsmethod_value_to_integer_or_infinity(position_value, 1, &position,
			error) < 0) {
		return -1;
	}
	*start_ptr = jsmethod_clamp_position(len, position);
	return 0;
}

static int
jsmethod_end_position(size_t len, int have_end_position,
		jsmethod_value_t end_position_value, size_t *end_ptr,
		jsmethod_error_t *error)
{
	double position;

	if (end_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (!have_end_position ||
			end_position_value.kind == JSMETHOD_VALUE_UNDEFINED) {
		*end_ptr = len;
		return 0;
	}
	if (jsmethod_value_to_integer_or_infinity(end_position_value, 0,
			&position, error) < 0) {
		return -1;
	}
	*end_ptr = jsmethod_clamp_position(len, position);
	return 0;
}

static int
jsmethod_utf16_region_equals(const jsstr16_t *value, size_t start,
		const jsstr16_t *search)
{
	size_t i;

	for (i = 0; i < search->len; i++) {
		if (value->codeunits[start + i] != search->codeunits[i]) {
			return 0;
		}
	}
	return 1;
}

static int
jsmethod_value_to_locale_tag(char locale_tag[3], int have_locale,
		jsmethod_value_t locale_value, uint16_t *storage, size_t storage_cap,
		jsmethod_error_t *error)
{
	jsstr16_t locale_str;
	size_t i;

	if (locale_tag == NULL) {
		errno = EINVAL;
		return -1;
	}
	locale_tag[0] = '\0';
	locale_tag[1] = '\0';
	locale_tag[2] = '\0';
	if (!have_locale || locale_value.kind == JSMETHOD_VALUE_UNDEFINED) {
		return 0;
	}
	if (storage == NULL) {
		errno = EINVAL;
		return -1;
	}

	jsstr16_init_from_buf(&locale_str, (const char *)storage,
			storage_cap * sizeof(storage[0]));
	if (jsmethod_value_to_string(&locale_str, locale_value, 0, error) < 0) {
		return -1;
	}

	for (i = 0; i < locale_str.len && i < 2; i++) {
		uint16_t cu = locale_str.codeunits[i];

		locale_tag[i] = cu <= 0x7F ? (char)cu : '\0';
		if (locale_tag[i] == '\0') {
			break;
		}
	}
	return 0;
}

int
jsmethod_string_to_lower_case(jsstr16_t *out, jsmethod_value_t this_value,
		jsmethod_error_t *error)
{
	if (out == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsmethod_this_to_string(out, this_value, error) < 0) {
		return -1;
	}
	jsstr16_tolower(out);
	return 0;
}

int
jsmethod_string_to_upper_case(jsstr16_t *out, jsmethod_value_t this_value,
		jsmethod_error_t *error)
{
	if (out == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsmethod_this_to_string(out, this_value, error) < 0) {
		return -1;
	}
	jsstr16_toupper(out);
	return 0;
}

int
jsmethod_string_to_locale_lower_case(jsstr16_t *out,
		jsmethod_value_t this_value, int have_locale,
		jsmethod_value_t locale_value, uint16_t *locale_storage,
		size_t locale_storage_cap, jsmethod_error_t *error)
{
	char locale_tag[3];
	const char *locale = NULL;

	if (out == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsmethod_this_to_string(out, this_value, error) < 0) {
		return -1;
	}
	if (jsmethod_value_to_locale_tag(locale_tag, have_locale, locale_value,
			locale_storage, locale_storage_cap, error) < 0) {
		return -1;
	}
	if (locale_tag[0] != '\0') {
		locale = locale_tag;
	}
	jsstr16_tolower_locale(out, locale);
	return 0;
}

int
jsmethod_string_to_locale_upper_case(jsstr16_t *out,
		jsmethod_value_t this_value, int have_locale,
		jsmethod_value_t locale_value, uint16_t *locale_storage,
		size_t locale_storage_cap, jsmethod_error_t *error)
{
	char locale_tag[3];
	const char *locale = NULL;

	if (out == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsmethod_this_to_string(out, this_value, error) < 0) {
		return -1;
	}
	if (jsmethod_value_to_locale_tag(locale_tag, have_locale, locale_value,
			locale_storage, locale_storage_cap, error) < 0) {
		return -1;
	}
	if (locale_tag[0] != '\0') {
		locale = locale_tag;
	}
	jsstr16_toupper_locale(out, locale);
	return 0;
}

int
jsmethod_string_to_well_formed(jsstr16_t *out, jsmethod_value_t this_value,
		jsmethod_error_t *error)
{
	if (out == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsmethod_this_to_string(out, this_value, error) < 0) {
		return -1;
	}
	jsstr16_to_well_formed(out);
	return 0;
}

int
jsmethod_string_is_well_formed(int *is_well_formed,
		jsmethod_value_t this_value, uint16_t *storage, size_t storage_cap,
		jsmethod_error_t *error)
{
	jsstr16_t value;

	if (is_well_formed == NULL || storage == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsstr16_init_from_buf(&value, (const char *)storage,
			storage_cap * sizeof(storage[0]));
	if (jsmethod_this_to_string(&value, this_value, error) < 0) {
		return -1;
	}
	*is_well_formed = jsstr16_is_well_formed(&value);
	return 0;
}

int
jsmethod_string_index_of(ssize_t *index_ptr, jsmethod_value_t this_value,
		jsmethod_value_t search_value, int have_position,
		jsmethod_value_t position_value, jsmethod_error_t *error)
{
	size_t this_len;
	size_t search_len;
	size_t start;
	jsstr16_t this_str;
	jsstr16_t search_str;
	ssize_t index;

	if (index_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsmethod_measure_value_utf16_len(this_value, 1, &this_len, error) < 0) {
		return -1;
	}
	if (jsmethod_measure_value_utf16_len(search_value, 0, &search_len,
			error) < 0) {
		return -1;
	}

	{
		uint16_t this_storage[this_len ? this_len : 1];
		uint16_t search_storage[search_len ? search_len : 1];

		jsstr16_init_from_buf(&this_str, (const char *)this_storage,
				sizeof(this_storage));
		jsstr16_init_from_buf(&search_str, (const char *)search_storage,
				sizeof(search_storage));
		if (jsmethod_this_to_string(&this_str, this_value, error) < 0) {
			return -1;
		}
		if (jsmethod_value_to_string(&search_str, search_value, 0, error) < 0) {
			return -1;
		}
		if (jsmethod_start_position(this_str.len, have_position, position_value,
				&start, error) < 0) {
			return -1;
		}
		if (search_str.len == 0) {
			*index_ptr = (ssize_t)start;
			return 0;
		}
		index = jsstr16_u16_indextoken(&this_str, search_str.codeunits,
				search_str.len, start);
		*index_ptr = index >= 0 ? index : -1;
		return 0;
	}
}

int
jsmethod_string_last_index_of(ssize_t *index_ptr,
		jsmethod_value_t this_value, jsmethod_value_t search_value,
		int have_position, jsmethod_value_t position_value,
		jsmethod_error_t *error)
{
	size_t this_len;
	size_t search_len;
	size_t start;
	jsstr16_t this_str;
	jsstr16_t search_str;
	ssize_t index;

	if (index_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsmethod_measure_value_utf16_len(this_value, 1, &this_len, error) < 0) {
		return -1;
	}
	if (jsmethod_measure_value_utf16_len(search_value, 0, &search_len,
			error) < 0) {
		return -1;
	}

	{
		uint16_t this_storage[this_len ? this_len : 1];
		uint16_t search_storage[search_len ? search_len : 1];

		jsstr16_init_from_buf(&this_str, (const char *)this_storage,
				sizeof(this_storage));
		jsstr16_init_from_buf(&search_str, (const char *)search_storage,
				sizeof(search_storage));
		if (jsmethod_this_to_string(&this_str, this_value, error) < 0) {
			return -1;
		}
		if (jsmethod_value_to_string(&search_str, search_value, 0, error) < 0) {
			return -1;
		}
		if (jsmethod_last_index_position(this_str.len, have_position,
				position_value, &start, error) < 0) {
			return -1;
		}
		if (search_str.len == 0) {
			*index_ptr = (ssize_t)start;
			return 0;
		}
		if (search_str.len > this_str.len) {
			*index_ptr = -1;
			return 0;
		}
		if (start > this_str.len - search_str.len) {
			start = this_str.len - search_str.len;
		}
		index = jsstr16_u16_lastindextoken(&this_str, search_str.codeunits,
				search_str.len, start);
		*index_ptr = index >= 0 ? index : -1;
		return 0;
	}
}

int
jsmethod_string_includes(int *result_ptr, jsmethod_value_t this_value,
		jsmethod_value_t search_value, int have_position,
		jsmethod_value_t position_value, jsmethod_error_t *error)
{
	ssize_t index;

	if (result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsmethod_string_index_of(&index, this_value, search_value, have_position,
			position_value, error) < 0) {
		return -1;
	}
	*result_ptr = index >= 0;
	return 0;
}

int
jsmethod_string_starts_with(int *result_ptr, jsmethod_value_t this_value,
		jsmethod_value_t search_value, int have_position,
		jsmethod_value_t position_value, jsmethod_error_t *error)
{
	size_t this_len;
	size_t search_len;
	size_t start;
	jsstr16_t this_str;
	jsstr16_t search_str;

	if (result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsmethod_measure_value_utf16_len(this_value, 1, &this_len, error) < 0) {
		return -1;
	}
	if (jsmethod_measure_value_utf16_len(search_value, 0, &search_len,
			error) < 0) {
		return -1;
	}

	{
		uint16_t this_storage[this_len ? this_len : 1];
		uint16_t search_storage[search_len ? search_len : 1];

		jsstr16_init_from_buf(&this_str, (const char *)this_storage,
				sizeof(this_storage));
		jsstr16_init_from_buf(&search_str, (const char *)search_storage,
				sizeof(search_storage));
		if (jsmethod_this_to_string(&this_str, this_value, error) < 0) {
			return -1;
		}
		if (jsmethod_value_to_string(&search_str, search_value, 0, error) < 0) {
			return -1;
		}
		if (jsmethod_start_position(this_str.len, have_position, position_value,
				&start, error) < 0) {
			return -1;
		}
		if (search_str.len == 0) {
			*result_ptr = 1;
			return 0;
		}
		if (start > this_str.len || search_str.len > this_str.len - start) {
			*result_ptr = 0;
			return 0;
		}
		*result_ptr = jsmethod_utf16_region_equals(&this_str, start,
				&search_str);
		return 0;
	}
}

int
jsmethod_string_ends_with(int *result_ptr, jsmethod_value_t this_value,
		jsmethod_value_t search_value, int have_end_position,
		jsmethod_value_t end_position_value, jsmethod_error_t *error)
{
	size_t this_len;
	size_t search_len;
	size_t end;
	size_t start;
	jsstr16_t this_str;
	jsstr16_t search_str;

	if (result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsmethod_measure_value_utf16_len(this_value, 1, &this_len, error) < 0) {
		return -1;
	}
	if (jsmethod_measure_value_utf16_len(search_value, 0, &search_len,
			error) < 0) {
		return -1;
	}

	{
		uint16_t this_storage[this_len ? this_len : 1];
		uint16_t search_storage[search_len ? search_len : 1];

		jsstr16_init_from_buf(&this_str, (const char *)this_storage,
				sizeof(this_storage));
		jsstr16_init_from_buf(&search_str, (const char *)search_storage,
				sizeof(search_storage));
		if (jsmethod_this_to_string(&this_str, this_value, error) < 0) {
			return -1;
		}
		if (jsmethod_value_to_string(&search_str, search_value, 0, error) < 0) {
			return -1;
		}
		if (jsmethod_end_position(this_str.len, have_end_position,
				end_position_value, &end, error) < 0) {
			return -1;
		}
		if (search_str.len == 0) {
			*result_ptr = 1;
			return 0;
		}
		if (search_str.len > end) {
			*result_ptr = 0;
			return 0;
		}
		start = end - search_str.len;
		*result_ptr = jsmethod_utf16_region_equals(&this_str, start,
				&search_str);
		return 0;
	}
}

static int
jsmethod_parse_form_utf16(const jsstr16_t *form_str,
		unicode_normalization_form_t *form_out, jsmethod_error_t *error)
{
	char ascii[5];
	size_t i;

	if (form_str == NULL || form_out == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (form_str->len < 3 || form_str->len > 4) {
		jsmethod_error_set(error, JSMETHOD_ERROR_RANGE,
				"invalid normalization form");
		return -1;
	}
	for (i = 0; i < form_str->len; i++) {
		if (form_str->codeunits[i] > 0x7F) {
			jsmethod_error_set(error, JSMETHOD_ERROR_RANGE,
					"invalid normalization form");
			return -1;
		}
		ascii[i] = (char)form_str->codeunits[i];
	}
	ascii[form_str->len] = '\0';

	if (unicode_normalization_form_parse(ascii, form_str->len, form_out) < 0) {
		jsmethod_error_set(error, JSMETHOD_ERROR_RANGE,
				"invalid normalization form");
		return -1;
	}
	return 0;
}

int
jsmethod_string_normalize_measure(jsmethod_value_t this_value,
		int have_form, jsmethod_value_t form_value,
		jsmethod_string_normalize_sizes_t *sizes,
		jsmethod_error_t *error)
{
	unicode_normalization_form_t form = UNICODE_NORMALIZE_NFC;
	size_t this_storage_len;
	size_t form_storage_len = 0;

	if (sizes == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	sizes->this_storage_len = 0;
	sizes->form_storage_len = 0;
	sizes->workspace_len = 0;
	sizes->result_len = 0;

	if (jsmethod_value_utf16_len(this_value, 1, &this_storage_len, error) < 0) {
		return -1;
	}
	sizes->this_storage_len = this_storage_len;

	if (have_form && form_value.kind != JSMETHOD_VALUE_UNDEFINED) {
		jsstr16_t form_str;

		if (jsmethod_value_utf16_len(form_value, 0, &form_storage_len, error) < 0) {
			return -1;
		}
		sizes->form_storage_len = form_storage_len;
		{
			uint16_t form_buf[form_storage_len ? form_storage_len : 1];
			jsstr16_init_from_buf(&form_str, (const char *)form_buf,
					sizeof(form_buf));
			if (jsmethod_value_to_string(&form_str, form_value, 0, error) < 0) {
				return -1;
			}
			if (jsmethod_parse_form_utf16(&form_str, &form, error) < 0) {
				return -1;
			}
		}
	}

	if (this_storage_len == 0) {
		return 0;
	}
	{
		jsstr16_t this_str;
		uint16_t this_storage[this_storage_len];
		uint32_t *workspace = NULL;

		jsstr16_init_from_buf(&this_str, (const char *)this_storage,
				sizeof(this_storage));
		if (jsmethod_value_to_string(&this_str, this_value, 1, error) < 0) {
			return -1;
		}
		if (jsstr16_normalize_form_workspace_len(&this_str, form,
				&sizes->workspace_len) < 0) {
			return -1;
		}
		if (sizes->workspace_len > 0) {
			uint32_t workspace_buf[sizes->workspace_len];
			workspace = workspace_buf;
			if (jsstr16_normalize_form_needed(&this_str, form, workspace,
					sizes->workspace_len, &sizes->result_len) < 0) {
				return -1;
			}
		} else {
			sizes->result_len = 0;
		}
	}
	return 0;
}

int
jsmethod_string_normalize_into(jsstr16_t *out, jsmethod_value_t this_value,
		uint16_t *this_storage, size_t this_storage_cap,
		int have_form, jsmethod_value_t form_value,
		uint16_t *form_storage, size_t form_storage_cap,
		uint32_t *workspace, size_t workspace_cap,
		jsmethod_error_t *error)
{
	unicode_normalization_form_t form = UNICODE_NORMALIZE_NFC;
	jsstr16_t value;
	jsstr32_t decoded;
	jsstr32_t normalized;
	size_t decoded_len;
	size_t needed_len;

	if (out == NULL || (this_storage_cap > 0 && this_storage == NULL) ||
			(workspace_cap > 0 && workspace == NULL)) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);

	jsstr16_init_from_buf(&value, (const char *)this_storage,
			this_storage_cap * sizeof(this_storage[0]));
	if (jsmethod_value_to_string(&value, this_value, 1, error) < 0) {
		return -1;
	}

	if (have_form && form_value.kind != JSMETHOD_VALUE_UNDEFINED) {
		jsstr16_t form_str;

		if (form_storage == NULL) {
			errno = EINVAL;
			return -1;
		}
		jsstr16_init_from_buf(&form_str, (const char *)form_storage,
				form_storage_cap * sizeof(form_storage[0]));
		if (jsmethod_value_to_string(&form_str, form_value, 0, error) < 0) {
			return -1;
		}
		if (jsmethod_parse_form_utf16(&form_str, &form, error) < 0) {
			return -1;
		}
	}

	if (jsstr16_normalize_form_needed(&value, form, workspace, workspace_cap,
			&needed_len) < 0) {
		return -1;
	}
	if (needed_len == 0) {
		out->len = 0;
		return 0;
	}
	if (out->cap < needed_len) {
		errno = ENOBUFS;
		return -1;
	}
	decoded_len = jsstr16_get_utf32len(&value);
	if (decoded_len == 0 && value.len > 0) {
		errno = EINVAL;
		return -1;
	}
	jsstr32_init_from_buf(&decoded, (const char *)workspace,
			decoded_len * sizeof(uint32_t));
	if (jsstr32_set_from_utf16(&decoded, value.codeunits, value.len) != value.len) {
		errno = EINVAL;
		return -1;
	}
	jsstr32_init_from_buf(&normalized, (const char *)(workspace + decoded_len),
			(workspace_cap - decoded_len) * sizeof(uint32_t));
	normalized.len = unicode_normalize_into_form(decoded.codepoints, decoded.len,
			normalized.codepoints, normalized.cap, form);
	if (jsstr16_set_from_jsstr32(out, &normalized) != normalized.len) {
		errno = ENOBUFS;
		return -1;
	}
	return 0;
}

int
jsmethod_string_normalize(jsstr16_t *out, jsmethod_value_t this_value,
		int have_form, jsmethod_value_t form_value,
		uint16_t *form_storage, size_t form_storage_cap,
		uint32_t *workspace, size_t workspace_cap,
		jsmethod_error_t *error)
{
	if (out == NULL || workspace == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	{
		size_t this_storage_len;
		size_t this_storage_cap;

		if (jsmethod_value_utf16_len(this_value, 1, &this_storage_len, error) < 0) {
			unicode_normalization_form_t form = UNICODE_NORMALIZE_NFC;
			jsstr16_t form_str;

			if (!(errno == ENOTSUP && error != NULL &&
					error->kind == JSMETHOD_ERROR_NONE)) {
				return -1;
			}
			jsmethod_error_clear(error);
			if (jsmethod_value_to_string(out, this_value, 1, error) < 0) {
				return -1;
			}
			if (have_form && form_value.kind != JSMETHOD_VALUE_UNDEFINED) {
				if (form_storage == NULL) {
					errno = EINVAL;
					return -1;
				}
				jsstr16_init_from_buf(&form_str, (const char *)form_storage,
						form_storage_cap * sizeof(form_storage[0]));
				if (jsmethod_value_to_string(&form_str, form_value, 0, error) < 0) {
					return -1;
				}
				if (jsmethod_parse_form_utf16(&form_str, &form, error) < 0) {
					return -1;
				}
			}
			if (jsstr16_normalize_form_buf(out, form, workspace, workspace_cap) < 0) {
				return -1;
			}
			return 0;
		}
		if (this_storage_len == 0) {
			this_storage_cap = 1;
		} else {
			this_storage_cap = this_storage_len;
		}
		{
			uint16_t this_storage[this_storage_cap];
			return jsmethod_string_normalize_into(out, this_value,
					this_storage, this_storage_len,
					have_form, form_value,
					form_storage, form_storage_cap,
					workspace, workspace_cap, error);
		}
	}
}
