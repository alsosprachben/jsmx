#include "jsmethod.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
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
	if (number != number) {
		return "NaN";
	}
	if (isinf(number)) {
		return number < 0 ? "-Infinity" : "Infinity";
	}
	snprintf(buf, 64, "%.17g", number);
	return buf;
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
jsmethod_string_normalize(jsstr16_t *out, jsmethod_value_t this_value,
		int have_form, jsmethod_value_t form_value,
		uint16_t *form_storage, size_t form_storage_cap,
		uint32_t *workspace, size_t workspace_cap,
		jsmethod_error_t *error)
{
	unicode_normalization_form_t form = UNICODE_NORMALIZE_NFC;
	jsstr16_t form_str;

	if (out == NULL || workspace == NULL) {
		errno = EINVAL;
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
