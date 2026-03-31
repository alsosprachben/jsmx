#include "jsval.h"

#include <errno.h>
#include <math.h>
#include <string.h>

#include "jsnum.h"
#include "utf8.h"

#define JSVAL_ALIGN sizeof(void *)
#define JSVAL_JSON_EMIT_MAX_DEPTH 256
#define JSVAL_METHOD_CASE_EXPANSION_MAX 3u

typedef struct jsval_native_string_s {
	size_t len;
	size_t cap;
} jsval_native_string_t;

typedef struct jsval_native_object_s {
	size_t len;
	size_t cap;
} jsval_native_object_t;

typedef struct jsval_native_array_s {
	size_t len;
	size_t cap;
} jsval_native_array_t;

typedef struct jsval_native_prop_s {
	jsval_off_t name_off;
	jsval_t value;
} jsval_native_prop_t;

typedef struct jsval_json_doc_s {
	jsval_off_t json_off;
	size_t json_len;
	jsval_off_t tokens_off;
	unsigned int tokcap;
	unsigned int tokused;
	int root_i;
} jsval_json_doc_t;

typedef struct jsval_json_emit_state_s {
	uint8_t *buf;
	size_t cap;
	size_t len;
	jsval_off_t stack[JSVAL_JSON_EMIT_MAX_DEPTH];
	size_t depth;
} jsval_json_emit_state_t;

static size_t jsval_align_up(size_t value, size_t align)
{
	size_t mask = align - 1;
	return (value + mask) & ~mask;
}

static size_t jsval_pages_head_size_aligned(void)
{
	return jsval_align_up(sizeof(jsval_pages_t), JSVAL_ALIGN);
}

static int jsval_region_valid(const jsval_region_t *region)
{
	return region != NULL &&
	       region->base != NULL &&
	       region->pages != NULL &&
	       region->pages->magic == JSVAL_PAGES_MAGIC &&
	       region->pages->version == JSVAL_PAGES_VERSION &&
	       region->pages->header_size == sizeof(jsval_pages_t);
}

static void *jsval_region_ptr(jsval_region_t *region, jsval_off_t off)
{
	if (!jsval_region_valid(region) || off > region->pages->total_len) {
		return NULL;
	}
	return region->base + off;
}

static int jsval_region_reserve(jsval_region_t *region, size_t len, size_t align, jsval_off_t *off_ptr, void **ptr_ptr)
{
	size_t start;
	size_t stop;

	if (!jsval_region_valid(region) || align == 0) {
		errno = EINVAL;
		return -1;
	}

	start = jsval_align_up(region->pages->used_len, align);
	stop = start + len;
	if (stop < start || stop > region->pages->total_len) {
		errno = ENOBUFS;
		return -1;
	}
	if (stop > UINT32_MAX) {
		errno = EOVERFLOW;
		return -1;
	}

	if (off_ptr != NULL) {
		*off_ptr = (jsval_off_t)start;
	}
	if (ptr_ptr != NULL) {
		*ptr_ptr = region->base + start;
	}
	region->pages->used_len = (uint32_t)stop;
	region->used = stop;
	return 0;
}

static int jsval_region_measure_reserve(const jsval_region_t *region,
		size_t *used_ptr, size_t len, size_t align)
{
	size_t start;
	size_t stop;

	if (!jsval_region_valid(region) || used_ptr == NULL || align == 0) {
		errno = EINVAL;
		return -1;
	}

	start = jsval_align_up(*used_ptr, align);
	stop = start + len;
	if (stop < start || stop > region->pages->total_len) {
		errno = ENOBUFS;
		return -1;
	}
	if (stop > UINT32_MAX) {
		errno = EOVERFLOW;
		return -1;
	}

	*used_ptr = stop;
	return 0;
}

static jsval_native_string_t *jsval_native_string(jsval_region_t *region, jsval_t value)
{
	if (value.repr != JSVAL_REPR_NATIVE || value.kind != JSVAL_KIND_STRING) {
		return NULL;
	}
	return (jsval_native_string_t *)jsval_region_ptr(region, value.off);
}

static uint16_t *jsval_native_string_units(jsval_native_string_t *string)
{
	return (uint16_t *)(string + 1);
}

static jsval_native_object_t *jsval_native_object(jsval_region_t *region, jsval_t value)
{
	if (value.repr != JSVAL_REPR_NATIVE || value.kind != JSVAL_KIND_OBJECT) {
		return NULL;
	}
	return (jsval_native_object_t *)jsval_region_ptr(region, value.off);
}

static jsval_native_prop_t *jsval_native_object_props(jsval_native_object_t *object)
{
	return (jsval_native_prop_t *)(object + 1);
}

static jsval_native_array_t *jsval_native_array(jsval_region_t *region, jsval_t value)
{
	if (value.repr != JSVAL_REPR_NATIVE || value.kind != JSVAL_KIND_ARRAY) {
		return NULL;
	}
	return (jsval_native_array_t *)jsval_region_ptr(region, value.off);
}

static jsval_t *jsval_native_array_values(jsval_native_array_t *array)
{
	return (jsval_t *)(array + 1);
}

static jsval_json_doc_t *jsval_json_doc(jsval_region_t *region, jsval_t value)
{
	if (value.repr != JSVAL_REPR_JSON) {
		return NULL;
	}
	return (jsval_json_doc_t *)jsval_region_ptr(region, value.off);
}

static const uint8_t *jsval_json_doc_bytes(jsval_region_t *region, jsval_json_doc_t *doc)
{
	return (const uint8_t *)jsval_region_ptr(region, doc->json_off);
}

static jsmntok_t *jsval_json_doc_tokens(jsval_region_t *region, jsval_json_doc_t *doc)
{
	return (jsmntok_t *)jsval_region_ptr(region, doc->tokens_off);
}

static size_t jsval_utf16_units_for_codepoint(uint32_t codepoint)
{
	if (codepoint < 0x10000) {
		return 1;
	}
	return 2;
}

static size_t jsval_utf8_utf16len(const uint8_t *str, size_t len)
{
	size_t out_len = 0;
	const uint8_t *cursor = str;
	const uint8_t *stop = str + len;

	while (cursor < stop) {
		uint32_t codepoint = 0;
		int seq_len = 0;

		UTF8_CHAR(cursor, stop, &codepoint, &seq_len);
		if (seq_len == 0) {
			break;
		}
		if (seq_len < 0) {
			seq_len = -seq_len;
			codepoint = 0xFFFD;
		}

		out_len += jsval_utf16_units_for_codepoint(codepoint);
		cursor += seq_len;
	}

	return out_len;
}

static size_t jsval_utf16_utf8len(const uint16_t *str, size_t len)
{
	size_t out_len = 0;
	const uint16_t *cursor = str;
	const uint16_t *stop = str + len;

	while (cursor < stop) {
		uint32_t codepoint = 0;
		int seq_len = 0;

		UTF16_CHAR(cursor, stop, &codepoint, &seq_len);
		if (seq_len == 0) {
			break;
		}
		if (seq_len < 0) {
			seq_len = -seq_len;
			codepoint = 0xFFFD;
		}

		out_len += UTF8_CLEN(codepoint);
		cursor += seq_len;
	}

	return out_len;
}

static int jsval_native_string_copy_utf8(jsval_region_t *region, jsval_t value, uint8_t *buf, size_t cap, size_t *len_ptr)
{
	jsval_native_string_t *string = jsval_native_string(region, value);
	const uint16_t *cursor;
	const uint16_t *stop;
	uint8_t *write;
	uint8_t *write_stop;
	size_t needed;

	if (string == NULL) {
		errno = EINVAL;
		return -1;
	}

	needed = jsval_utf16_utf8len(jsval_native_string_units(string), string->len);
	if (len_ptr != NULL) {
		*len_ptr = needed;
	}
	if (buf == NULL) {
		return 0;
	}
	if (cap < needed) {
		errno = ENOBUFS;
		return -1;
	}

	cursor = jsval_native_string_units(string);
	stop = cursor + string->len;
	write = buf;
	write_stop = buf + cap;
	while (cursor < stop && write < write_stop) {
		uint32_t codepoint = 0;
		int seq_len = 0;
		const uint32_t *codepoint_ptr = &codepoint;

		UTF16_CHAR(cursor, stop, &codepoint, &seq_len);
		if (seq_len == 0) {
			break;
		}
		if (seq_len < 0) {
			seq_len = -seq_len;
			codepoint = 0xFFFD;
		}

		UTF8_ENCODE(&codepoint_ptr, codepoint_ptr + 1, &write, write_stop);
		cursor += seq_len;
	}

	return 0;
}

static int jsval_native_string_copy_utf16(jsval_region_t *region, jsval_t value, uint16_t *buf, size_t cap, size_t *len_ptr)
{
	jsval_native_string_t *string = jsval_native_string(region, value);

	if (string == NULL) {
		errno = EINVAL;
		return -1;
	}

	if (len_ptr != NULL) {
		*len_ptr = string->len;
	}
	if (buf == NULL) {
		return 0;
	}
	if (cap < string->len) {
		errno = ENOBUFS;
		return -1;
	}
	if (string->len > 0) {
		memcpy(buf, jsval_native_string_units(string), string->len * sizeof(uint16_t));
	}
	return 0;
}

static int jsval_native_string_eq_utf8(jsval_region_t *region, jsval_t value, const uint8_t *key, size_t key_len)
{
	jsval_native_string_t *string = jsval_native_string(region, value);
	const uint16_t *left;
	const uint16_t *left_stop;
	const uint8_t *right;
	const uint8_t *right_stop;

	if (string == NULL) {
		return 0;
	}

	left = jsval_native_string_units(string);
	left_stop = left + string->len;
	right = key;
	right_stop = key + key_len;
	while (left < left_stop && right < right_stop) {
		uint32_t left_cp = 0;
		uint32_t right_cp = 0;
		int left_len = 0;
		int right_len = 0;

		UTF16_CHAR(left, left_stop, &left_cp, &left_len);
		UTF8_CHAR(right, right_stop, &right_cp, &right_len);
		if (left_len == 0 || right_len == 0) {
			return 0;
		}
		if (left_len < 0) {
			left_len = -left_len;
			left_cp = 0xFFFD;
		}
		if (right_len < 0) {
			right_len = -right_len;
			right_cp = 0xFFFD;
		}
		if (left_cp != right_cp) {
			return 0;
		}

		left += left_len;
		right += right_len;
	}

	return left == left_stop && right == right_stop;
}

static int jsval_json_token_range(jsval_region_t *region, jsval_json_doc_t *doc, uint32_t index, const uint8_t **start_ptr, const uint8_t **stop_ptr)
{
	const uint8_t *json;
	jsmntok_t *tokens;
	jsmntok_t *token;

	if (doc == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (index >= doc->tokused) {
		errno = EINVAL;
		return -1;
	}

	json = jsval_json_doc_bytes(region, doc);
	tokens = jsval_json_doc_tokens(region, doc);
	token = &tokens[index];
	if (token->start < 0 || token->end < token->start) {
		errno = EINVAL;
		return -1;
	}

	*start_ptr = json + token->start;
	*stop_ptr = json + token->end;
	return 0;
}

static jsval_kind_t jsval_json_token_kind(jsval_region_t *region, jsval_json_doc_t *doc, uint32_t index)
{
	const uint8_t *start;
	const uint8_t *stop;
	jsmntok_t *tokens = jsval_json_doc_tokens(region, doc);
	jsmntok_t *token = &tokens[index];

	switch (token->type) {
	case JSMN_STRING:
		return JSVAL_KIND_STRING;
	case JSMN_OBJECT:
		return JSVAL_KIND_OBJECT;
	case JSMN_ARRAY:
		return JSVAL_KIND_ARRAY;
	case JSMN_PRIMITIVE:
		if (jsval_json_token_range(region, doc, index, &start, &stop) < 0 || start >= stop) {
			return JSVAL_KIND_UNDEFINED;
		}
		if (*start == 'n') {
			return JSVAL_KIND_NULL;
		}
		if (*start == 't' || *start == 'f') {
			return JSVAL_KIND_BOOL;
		}
		return JSVAL_KIND_NUMBER;
	default:
		return JSVAL_KIND_UNDEFINED;
	}
}

static jsval_t jsval_json_value(jsval_region_t *region, jsval_t value, uint32_t index)
{
	jsval_json_doc_t *doc = jsval_json_doc(region, value);
	jsval_t out = jsval_undefined();

	out.kind = jsval_json_token_kind(region, doc, index);
	out.repr = JSVAL_REPR_JSON;
	out.off = value.off;
	out.as.index = index;
	return out;
}

static int jsval_json_next(jsval_region_t *region, jsval_json_doc_t *doc, int index)
{
	int next;
	unsigned int i;
	jsmntok_t *tokens = jsval_json_doc_tokens(region, doc);

	if (index < 0 || (unsigned int)index >= doc->tokused) {
		return -1;
	}

	next = index + 1;
	switch (tokens[index].type) {
	case JSMN_STRING:
	case JSMN_PRIMITIVE:
		return next;
	case JSMN_ARRAY:
		for (i = 0; i < (unsigned int)tokens[index].size; i++) {
			next = jsval_json_next(region, doc, next);
			if (next < 0) {
				return -1;
			}
		}
		return next;
	case JSMN_OBJECT:
		for (i = 0; i < (unsigned int)tokens[index].size; i++) {
			next = jsval_json_next(region, doc, next);
			if (next < 0) {
				return -1;
			}
			next = jsval_json_next(region, doc, next);
			if (next < 0) {
				return -1;
			}
		}
		return next;
	default:
		return -1;
	}
}

static int jsval_json_bool_value(jsval_region_t *region, jsval_json_doc_t *doc, uint32_t index, int *boolean_ptr)
{
	const uint8_t *start;
	const uint8_t *stop;
	size_t len;

	if (jsval_json_token_kind(region, doc, index) != JSVAL_KIND_BOOL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_json_token_range(region, doc, index, &start, &stop) < 0) {
		return -1;
	}

	len = (size_t)(stop - start);
	if (len == 4 && memcmp(start, "true", 4) == 0) {
		*boolean_ptr = 1;
		return 0;
	}
	if (len == 5 && memcmp(start, "false", 5) == 0) {
		*boolean_ptr = 0;
		return 0;
	}

	errno = EINVAL;
	return -1;
}

static int jsval_json_number_value(jsval_region_t *region, jsval_json_doc_t *doc, uint32_t index, double *number_ptr)
{
	const uint8_t *start;
	const uint8_t *stop;
	size_t len;

	if (jsval_json_token_kind(region, doc, index) != JSVAL_KIND_NUMBER) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_json_token_range(region, doc, index, &start, &stop) < 0) {
		return -1;
	}

	len = (size_t)(stop - start);
	return jsnum_parse_json(start, len, number_ptr);
}

static int jsval_json_string_copy_utf32(jsval_region_t *region, jsval_json_doc_t *doc, uint32_t index, uint32_t *buf, size_t cap, size_t *len_ptr)
{
	const uint8_t *pos_cursor;
	const uint8_t *pos_stop;
	uint32_t q32[128];
	uint32_t val32[128];
	uint32_t *q32_out;
	uint32_t *q32_in;
	uint32_t *val32_out;
	uint32_t *write;
	uint32_t *write_stop;
	size_t needed = 0;

	if (jsval_json_token_kind(region, doc, index) != JSVAL_KIND_STRING) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_json_token_range(region, doc, index, &pos_cursor, &pos_stop) < 0) {
		return -1;
	}

	write = buf;
	write_stop = buf == NULL ? NULL : buf + cap;
	while (pos_cursor < pos_stop) {
		q32_out = q32;
		q32_in = q32;
		val32_out = val32;

		UTF8_DECODE(&pos_cursor, pos_stop, &q32_out, q32 + 128);
		JSMN_UNQUOTE((const uint32_t **)&q32_in, (const uint32_t *)q32_out, &val32_out, val32 + 128);

		needed += (size_t)(val32_out - val32);
		if (buf != NULL) {
			size_t chunk = (size_t)(val32_out - val32);
			if ((size_t)(write_stop - write) < chunk) {
				errno = ENOBUFS;
				return -1;
			}
			memcpy(write, val32, chunk * sizeof(uint32_t));
			write += chunk;
		}
	}

	if (len_ptr != NULL) {
		*len_ptr = needed;
	}
	return 0;
}

static int jsval_json_string_copy_utf16(jsval_region_t *region, jsval_json_doc_t *doc, uint32_t index, uint16_t *buf, size_t cap, size_t *len_ptr)
{
	size_t utf32_len;
	size_t i;

	if (jsval_json_string_copy_utf32(region, doc, index, NULL, 0, &utf32_len) < 0) {
		return -1;
	}

	if (buf == NULL) {
		if (len_ptr != NULL) {
			uint32_t utf32_buf[utf32_len ? utf32_len : 1];
			if (jsval_json_string_copy_utf32(region, doc, index, utf32_buf, utf32_len, NULL) < 0) {
				return -1;
			}
			*len_ptr = 0;
			for (i = 0; i < utf32_len; i++) {
				*len_ptr += jsval_utf16_units_for_codepoint(utf32_buf[i]);
			}
		}
		return 0;
	}

	{
		uint32_t utf32_buf[utf32_len ? utf32_len : 1];
		uint16_t *write = buf;
		uint16_t *write_stop = buf + cap;
		const uint32_t *read = utf32_buf;
		const uint32_t *read_stop = utf32_buf + utf32_len;

		if (jsval_json_string_copy_utf32(region, doc, index, utf32_buf, utf32_len, NULL) < 0) {
			return -1;
		}
		for (i = 0; i < utf32_len; i++) {
			if ((size_t)(write_stop - write) < jsval_utf16_units_for_codepoint(utf32_buf[i])) {
				errno = ENOBUFS;
				return -1;
			}
		}
		UTF16_ENCODE(&read, read_stop, &write, write_stop);
		if (len_ptr != NULL) {
			*len_ptr = (size_t)(write - buf);
		}
		return 0;
	}
}

static int jsval_json_string_copy_utf8_internal(jsval_region_t *region, jsval_json_doc_t *doc, uint32_t index, uint8_t *buf, size_t cap, size_t *len_ptr)
{
	const uint8_t *pos_cursor;
	const uint8_t *pos_stop;
	uint32_t q32[128];
	uint32_t val32[128];
	uint32_t *q32_out;
	uint32_t *q32_in;
	uint32_t *val32_out;
	const uint32_t *val32_in;
	uint8_t *write;
	uint8_t *write_stop;
	size_t needed = 0;

	if (jsval_json_token_kind(region, doc, index) != JSVAL_KIND_STRING) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_json_token_range(region, doc, index, &pos_cursor, &pos_stop) < 0) {
		return -1;
	}

	write = buf;
	write_stop = buf == NULL ? NULL : buf + cap;
	while (pos_cursor < pos_stop) {
		uint8_t scratch[512];
		uint8_t *scratch_write = scratch;

		q32_out = q32;
		q32_in = q32;
		val32_out = val32;
		val32_in = val32;

		UTF8_DECODE(&pos_cursor, pos_stop, &q32_out, q32 + 128);
		JSMN_UNQUOTE((const uint32_t **)&q32_in, (const uint32_t *)q32_out, &val32_out, val32 + 128);
		UTF8_ENCODE(&val32_in, val32_out, &scratch_write, scratch + sizeof(scratch));

		needed += (size_t)(scratch_write - scratch);
		if (buf != NULL) {
			size_t chunk = (size_t)(scratch_write - scratch);
			if ((size_t)(write_stop - write) < chunk) {
				errno = ENOBUFS;
				return -1;
			}
			memcpy(write, scratch, chunk);
			write += chunk;
		}
	}

	if (len_ptr != NULL) {
		*len_ptr = needed;
	}
	return 0;
}

static int jsval_json_string_eq_utf8(jsval_region_t *region, jsval_json_doc_t *doc, uint32_t index, const uint8_t *key, size_t key_len)
{
	size_t len;

	if (jsval_json_string_copy_utf8_internal(region, doc, index, NULL, 0, &len) < 0) {
		return 0;
	}
	if (len != key_len) {
		return 0;
	}

	{
		uint8_t buf[key_len ? key_len : 1];
		if (jsval_json_string_copy_utf8_internal(region, doc, index, buf, key_len, NULL) < 0) {
			return 0;
		}
		return memcmp(buf, key, key_len) == 0;
	}
}

static int jsval_json_emit_append(jsval_json_emit_state_t *state, const uint8_t *src, size_t len)
{
	if (SIZE_MAX - state->len < len) {
		errno = EOVERFLOW;
		return -1;
	}
	if (state->buf != NULL) {
		if (state->len + len > state->cap) {
			errno = ENOBUFS;
			return -1;
		}
		if (len > 0) {
			memcpy(state->buf + state->len, src, len);
		}
	}
	state->len += len;
	return 0;
}

static int jsval_json_emit_byte(jsval_json_emit_state_t *state, uint8_t byte)
{
	return jsval_json_emit_append(state, &byte, 1);
}

static int jsval_json_emit_ascii(jsval_json_emit_state_t *state, const char *text)
{
	return jsval_json_emit_append(state, (const uint8_t *)text, strlen(text));
}

static int jsval_json_emit_u16_escape(jsval_json_emit_state_t *state, uint16_t codeunit)
{
	static const char hex[] = "0123456789ABCDEF";
	uint8_t escape[6];

	escape[0] = '\\';
	escape[1] = 'u';
	escape[2] = (uint8_t)hex[(codeunit >> 12) & 0x0F];
	escape[3] = (uint8_t)hex[(codeunit >> 8) & 0x0F];
	escape[4] = (uint8_t)hex[(codeunit >> 4) & 0x0F];
	escape[5] = (uint8_t)hex[codeunit & 0x0F];
	return jsval_json_emit_append(state, escape, sizeof(escape));
}

static int jsval_json_emit_push(jsval_json_emit_state_t *state, jsval_t value)
{
	size_t i;

	if (value.repr != JSVAL_REPR_NATIVE ||
	    (value.kind != JSVAL_KIND_OBJECT && value.kind != JSVAL_KIND_ARRAY)) {
		return 0;
	}

	for (i = 0; i < state->depth; i++) {
		if (state->stack[i] == value.off) {
			errno = ELOOP;
			return -1;
		}
	}
	if (state->depth >= JSVAL_JSON_EMIT_MAX_DEPTH) {
		errno = EOVERFLOW;
		return -1;
	}

	state->stack[state->depth++] = value.off;
	return 0;
}

static void jsval_json_emit_pop(jsval_json_emit_state_t *state, jsval_t value)
{
	if (value.repr == JSVAL_REPR_NATIVE &&
	    (value.kind == JSVAL_KIND_OBJECT || value.kind == JSVAL_KIND_ARRAY) &&
	    state->depth > 0) {
		state->depth--;
	}
}

static int jsval_json_emit_value(jsval_region_t *region, jsval_t value, jsval_json_emit_state_t *state);

static int jsval_json_emit_native_string(jsval_region_t *region, jsval_t value, jsval_json_emit_state_t *state)
{
	jsval_native_string_t *string = jsval_native_string(region, value);
	const uint16_t *cursor;
	const uint16_t *stop;

	if (string == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_json_emit_byte(state, '"') < 0) {
		return -1;
	}

	cursor = jsval_native_string_units(string);
	stop = cursor + string->len;
	while (cursor < stop) {
		uint32_t codepoint = 0;
		int seq_len = 0;

		UTF16_CHAR(cursor, stop, &codepoint, &seq_len);
		if (seq_len == 0) {
			break;
		}
		if (seq_len < 0) {
			seq_len = -seq_len;
			codepoint = 0xFFFD;
		}

		switch (codepoint) {
		case '"':
			if (jsval_json_emit_ascii(state, "\\\"") < 0) {
				return -1;
			}
			break;
		case '\\':
			if (jsval_json_emit_ascii(state, "\\\\") < 0) {
				return -1;
			}
			break;
		case '\b':
			if (jsval_json_emit_ascii(state, "\\b") < 0) {
				return -1;
			}
			break;
		case '\f':
			if (jsval_json_emit_ascii(state, "\\f") < 0) {
				return -1;
			}
			break;
		case '\n':
			if (jsval_json_emit_ascii(state, "\\n") < 0) {
				return -1;
			}
			break;
		case '\r':
			if (jsval_json_emit_ascii(state, "\\r") < 0) {
				return -1;
			}
			break;
		case '\t':
			if (jsval_json_emit_ascii(state, "\\t") < 0) {
				return -1;
			}
			break;
		default:
			if (codepoint < 0x20) {
				if (jsval_json_emit_u16_escape(state, (uint16_t)codepoint) < 0) {
					return -1;
				}
			} else {
				uint8_t utf8[4];
				uint8_t *write = utf8;
				const uint32_t *read = &codepoint;

				UTF8_ENCODE(&read, read + 1, &write, utf8 + sizeof(utf8));
				if (jsval_json_emit_append(state, utf8, (size_t)(write - utf8)) < 0) {
					return -1;
				}
			}
			break;
		}

		cursor += seq_len;
	}

	return jsval_json_emit_byte(state, '"');
}

static int jsval_json_emit_json_value(jsval_region_t *region, jsval_t value, jsval_json_emit_state_t *state)
{
	jsval_json_doc_t *doc = jsval_json_doc(region, value);
	const uint8_t *start;
	const uint8_t *stop;

	if (doc == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_json_token_range(region, doc, value.as.index, &start, &stop) < 0) {
		return -1;
	}

	if (value.kind == JSVAL_KIND_STRING) {
		if (jsval_json_emit_byte(state, '"') < 0) {
			return -1;
		}
		if (jsval_json_emit_append(state, start, (size_t)(stop - start)) < 0) {
			return -1;
		}
		return jsval_json_emit_byte(state, '"');
	}

	return jsval_json_emit_append(state, start, (size_t)(stop - start));
}

static int jsval_json_emit_value(jsval_region_t *region, jsval_t value, jsval_json_emit_state_t *state)
{
	size_t i;

	if (value.repr == JSVAL_REPR_JSON) {
		return jsval_json_emit_json_value(region, value, state);
	}

	switch (value.kind) {
	case JSVAL_KIND_NULL:
		return jsval_json_emit_ascii(state, "null");
	case JSVAL_KIND_BOOL:
		return jsval_json_emit_ascii(state, value.as.boolean ? "true" : "false");
		case JSVAL_KIND_NUMBER:
			if (!isfinite(value.as.number)) {
				errno = ENOTSUP;
				return -1;
			}
			{
				char numbuf[64];
				size_t len = 0;

				if (jsnum_format(value.as.number, numbuf, sizeof(numbuf), &len) < 0) {
					return -1;
				}
				return jsval_json_emit_append(state, (const uint8_t *)numbuf, len);
			}
	case JSVAL_KIND_STRING:
		return jsval_json_emit_native_string(region, value, state);
	case JSVAL_KIND_ARRAY:
	{
		jsval_native_array_t *array = jsval_native_array(region, value);
		jsval_t *values;

		if (array == NULL) {
			errno = EINVAL;
			return -1;
		}
		if (jsval_json_emit_push(state, value) < 0) {
			return -1;
		}
		if (jsval_json_emit_byte(state, '[') < 0) {
			jsval_json_emit_pop(state, value);
			return -1;
		}

		values = jsval_native_array_values(array);
		for (i = 0; i < array->len; i++) {
			if (i > 0 && jsval_json_emit_byte(state, ',') < 0) {
				jsval_json_emit_pop(state, value);
				return -1;
			}
			if (jsval_json_emit_value(region, values[i], state) < 0) {
				jsval_json_emit_pop(state, value);
				return -1;
			}
		}

		jsval_json_emit_pop(state, value);
		return jsval_json_emit_byte(state, ']');
	}
	case JSVAL_KIND_OBJECT:
	{
		jsval_native_object_t *object = jsval_native_object(region, value);
		jsval_native_prop_t *props;

		if (object == NULL) {
			errno = EINVAL;
			return -1;
		}
		if (jsval_json_emit_push(state, value) < 0) {
			return -1;
		}
		if (jsval_json_emit_byte(state, '{') < 0) {
			jsval_json_emit_pop(state, value);
			return -1;
		}

		props = jsval_native_object_props(object);
		for (i = 0; i < object->len; i++) {
			jsval_t name = jsval_undefined();

			if (i > 0 && jsval_json_emit_byte(state, ',') < 0) {
				jsval_json_emit_pop(state, value);
				return -1;
			}

			name.kind = JSVAL_KIND_STRING;
			name.repr = JSVAL_REPR_NATIVE;
			name.off = props[i].name_off;
			if (jsval_json_emit_native_string(region, name, state) < 0) {
				jsval_json_emit_pop(state, value);
				return -1;
			}
			if (jsval_json_emit_byte(state, ':') < 0) {
				jsval_json_emit_pop(state, value);
				return -1;
			}
			if (jsval_json_emit_value(region, props[i].value, state) < 0) {
				jsval_json_emit_pop(state, value);
				return -1;
			}
		}

		jsval_json_emit_pop(state, value);
		return jsval_json_emit_byte(state, '}');
	}
	case JSVAL_KIND_UNDEFINED:
	default:
		errno = ENOTSUP;
		return -1;
	}
}

static int jsval_value_utf16_len(jsval_region_t *region, jsval_t value, size_t *len_ptr);
static int jsval_value_copy_utf16(jsval_region_t *region, jsval_t value, uint16_t *buf, size_t cap, size_t *len_ptr);

static int jsval_ascii_copy_utf16(const char *text, uint16_t *buf, size_t cap, size_t *len_ptr)
{
	size_t len = strlen(text);
	size_t i;

	if (len_ptr != NULL) {
		*len_ptr = len;
	}
	if (buf == NULL) {
		return 0;
	}
	if (cap < len) {
		errno = ENOBUFS;
		return -1;
	}
	for (i = 0; i < len; i++) {
		buf[i] = (uint8_t)text[i];
	}
	return 0;
}

static const char *jsval_number_text(double number, char buf[64])
{
	if (jsnum_format(number, buf, 64, NULL) < 0) {
		return NULL;
	}
	return buf;
}

static int jsval_string_reserve_utf16(jsval_region_t *region, size_t cap,
		jsval_t *value_ptr, jsval_native_string_t **string_ptr)
{
	jsval_native_string_t *string;
	jsval_off_t off;
	size_t units_cap = cap > 0 ? cap : 1;
	size_t bytes_len;

	if (region == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (units_cap > (SIZE_MAX - sizeof(*string)) / sizeof(uint16_t)) {
		errno = EOVERFLOW;
		return -1;
	}

	bytes_len = sizeof(*string) + units_cap * sizeof(uint16_t);
	if (jsval_region_reserve(region, bytes_len, JSVAL_ALIGN, &off,
			(void **)&string) < 0) {
		return -1;
	}

	string->len = 0;
	string->cap = units_cap;
	*value_ptr = jsval_undefined();
	value_ptr->kind = JSVAL_KIND_STRING;
	value_ptr->repr = JSVAL_REPR_NATIVE;
	value_ptr->off = off;
	if (string_ptr != NULL) {
		*string_ptr = string;
	}
	return 0;
}

static int jsval_string_measure_utf16(const jsval_region_t *region,
		size_t *used_ptr, size_t cap)
{
	size_t units_cap = cap > 0 ? cap : 1;
	size_t bytes_len;

	if (used_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (units_cap > (SIZE_MAX - sizeof(jsval_native_string_t)) / sizeof(uint16_t)) {
		errno = EOVERFLOW;
		return -1;
	}

	bytes_len = sizeof(jsval_native_string_t) + units_cap * sizeof(uint16_t);
	return jsval_region_measure_reserve(region, used_ptr, bytes_len, JSVAL_ALIGN);
}

static int jsval_value_utf16_len(jsval_region_t *region, jsval_t value, size_t *len_ptr)
{
	jsval_json_doc_t *doc;
	char numbuf[64];
	const char *text;

	switch (value.kind) {
	case JSVAL_KIND_STRING:
		if (value.repr == JSVAL_REPR_NATIVE) {
			return jsval_native_string_copy_utf16(region, value, NULL, 0, len_ptr);
		}
		if (value.repr == JSVAL_REPR_JSON) {
			doc = jsval_json_doc(region, value);
			return jsval_json_string_copy_utf16(region, doc, value.as.index, NULL, 0, len_ptr);
		}
		break;
	case JSVAL_KIND_UNDEFINED:
		return jsval_ascii_copy_utf16("undefined", NULL, 0, len_ptr);
	case JSVAL_KIND_NULL:
		return jsval_ascii_copy_utf16("null", NULL, 0, len_ptr);
	case JSVAL_KIND_BOOL:
		return jsval_ascii_copy_utf16(value.repr == JSVAL_REPR_JSON ? (jsval_truthy(region, value) ? "true" : "false") : (value.as.boolean ? "true" : "false"), NULL, 0, len_ptr);
	case JSVAL_KIND_NUMBER:
		if (value.repr == JSVAL_REPR_JSON) {
			double number;
			doc = jsval_json_doc(region, value);
			if (jsval_json_number_value(region, doc, value.as.index, &number) < 0) {
				return -1;
			}
			text = jsval_number_text(number, numbuf);
			return jsval_ascii_copy_utf16(text, NULL, 0, len_ptr);
		}
		text = jsval_number_text(value.as.number, numbuf);
		return jsval_ascii_copy_utf16(text, NULL, 0, len_ptr);
	default:
		errno = ENOTSUP;
		return -1;
	}

	errno = EINVAL;
	return -1;
}

static int jsval_value_copy_utf16(jsval_region_t *region, jsval_t value, uint16_t *buf, size_t cap, size_t *len_ptr)
{
	jsval_json_doc_t *doc;
	double number;
	char numbuf[64];
	const char *text;

	switch (value.kind) {
	case JSVAL_KIND_STRING:
		if (value.repr == JSVAL_REPR_NATIVE) {
			return jsval_native_string_copy_utf16(region, value, buf, cap, len_ptr);
		}
		if (value.repr == JSVAL_REPR_JSON) {
			doc = jsval_json_doc(region, value);
			return jsval_json_string_copy_utf16(region, doc, value.as.index, buf, cap, len_ptr);
		}
		break;
	case JSVAL_KIND_UNDEFINED:
		return jsval_ascii_copy_utf16("undefined", buf, cap, len_ptr);
	case JSVAL_KIND_NULL:
		return jsval_ascii_copy_utf16("null", buf, cap, len_ptr);
	case JSVAL_KIND_BOOL:
		if (value.repr == JSVAL_REPR_JSON) {
			int boolean;
			doc = jsval_json_doc(region, value);
			if (jsval_json_bool_value(region, doc, value.as.index, &boolean) < 0) {
				return -1;
			}
			return jsval_ascii_copy_utf16(boolean ? "true" : "false", buf, cap, len_ptr);
		}
		return jsval_ascii_copy_utf16(value.as.boolean ? "true" : "false", buf, cap, len_ptr);
	case JSVAL_KIND_NUMBER:
		if (value.repr == JSVAL_REPR_JSON) {
			doc = jsval_json_doc(region, value);
			if (jsval_json_number_value(region, doc, value.as.index, &number) < 0) {
				return -1;
			}
			text = jsval_number_text(number, numbuf);
			return jsval_ascii_copy_utf16(text, buf, cap, len_ptr);
		}
		text = jsval_number_text(value.as.number, numbuf);
		return jsval_ascii_copy_utf16(text, buf, cap, len_ptr);
	default:
		errno = ENOTSUP;
		return -1;
	}

	errno = EINVAL;
	return -1;
}

static int jsval_ascii_space(uint8_t ch)
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

static int jsval_is_ascii_alpha(uint8_t ch)
{
	return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
}

static int jsval_exact_ascii(const uint8_t *buf, size_t len, const char *text)
{
	size_t text_len = strlen(text);
	return len == text_len && memcmp(buf, text, text_len) == 0;
}

static int jsval_string_to_number(jsval_region_t *region, jsval_t value,
		double *number_ptr)
{
	size_t len = 0;

	if (jsval_string_copy_utf8(region, value, NULL, 0, &len) < 0) {
		return -1;
	}

	{
		uint8_t bytes[len ? len : 1];

		if (len > 0 && jsval_string_copy_utf8(region, value, bytes, len, NULL) < 0) {
			return -1;
		}
		return jsnum_parse_string(bytes, len, number_ptr);
	}
}

int jsval_to_number(jsval_region_t *region, jsval_t value, double *number_ptr)
{
	jsval_json_doc_t *doc;
	int boolean;

	switch (value.kind) {
	case JSVAL_KIND_UNDEFINED:
		*number_ptr = NAN;
		return 0;
	case JSVAL_KIND_NULL:
		*number_ptr = 0;
		return 0;
	case JSVAL_KIND_BOOL:
		if (value.repr == JSVAL_REPR_JSON) {
			doc = jsval_json_doc(region, value);
			if (jsval_json_bool_value(region, doc, value.as.index, &boolean) < 0) {
				return -1;
			}
			*number_ptr = boolean ? 1 : 0;
			return 0;
		}
		*number_ptr = value.as.boolean ? 1 : 0;
		return 0;
	case JSVAL_KIND_NUMBER:
		if (value.repr == JSVAL_REPR_JSON) {
			doc = jsval_json_doc(region, value);
			return jsval_json_number_value(region, doc, value.as.index, number_ptr);
		}
		*number_ptr = value.as.number;
		return 0;
	case JSVAL_KIND_STRING:
		return jsval_string_to_number(region, value, number_ptr);
	default:
		errno = ENOTSUP;
		return -1;
	}
}

int jsval_to_int32(jsval_region_t *region, jsval_t value, int32_t *result_ptr)
{
	double number;

	if (result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_to_number(region, value, &number) < 0) {
		return -1;
	}
	*result_ptr = jsnum_to_int32(number);
	return 0;
}

int jsval_to_uint32(jsval_region_t *region, jsval_t value, uint32_t *result_ptr)
{
	double number;

	if (result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_to_number(region, value, &number) < 0) {
		return -1;
	}
	*result_ptr = jsnum_to_uint32(number);
	return 0;
}

static int jsval_method_output_cap(size_t input_len, size_t factor,
		size_t *cap_ptr)
{
	size_t cap;

	if (cap_ptr == NULL || factor == 0) {
		errno = EINVAL;
		return -1;
	}
	if (input_len > SIZE_MAX / factor) {
		errno = EOVERFLOW;
		return -1;
	}

	cap = input_len * factor;
	if (cap == 0) {
		cap = 1;
	}
	*cap_ptr = cap;
	return 0;
}

static int jsval_method_value_from_jsval(jsval_region_t *region, jsval_t value,
		uint16_t *string_storage, size_t string_storage_cap,
		jsmethod_value_t *method_value_ptr)
{
	jsval_json_doc_t *doc;
	jsval_native_string_t *string;
	double number;
	int boolean;
	size_t len;

	if (region == NULL || method_value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}

	switch (value.kind) {
	case JSVAL_KIND_UNDEFINED:
		*method_value_ptr = jsmethod_value_undefined();
		return 0;
	case JSVAL_KIND_NULL:
		*method_value_ptr = jsmethod_value_null();
		return 0;
	case JSVAL_KIND_BOOL:
		if (value.repr == JSVAL_REPR_JSON) {
			doc = jsval_json_doc(region, value);
			if (jsval_json_bool_value(region, doc, value.as.index, &boolean) < 0) {
				return -1;
			}
			*method_value_ptr = jsmethod_value_bool(boolean);
			return 0;
		}
		*method_value_ptr = jsmethod_value_bool(value.as.boolean);
		return 0;
	case JSVAL_KIND_NUMBER:
		if (value.repr == JSVAL_REPR_JSON) {
			doc = jsval_json_doc(region, value);
			if (jsval_json_number_value(region, doc, value.as.index, &number) < 0) {
				return -1;
			}
			*method_value_ptr = jsmethod_value_number(number);
			return 0;
		}
		*method_value_ptr = jsmethod_value_number(value.as.number);
		return 0;
	case JSVAL_KIND_STRING:
		if (value.repr == JSVAL_REPR_NATIVE) {
			string = jsval_native_string(region, value);
			if (string == NULL) {
				errno = EINVAL;
				return -1;
			}
			*method_value_ptr = jsmethod_value_string_utf16(
					jsval_native_string_units(string), string->len);
			return 0;
		}
		if (value.repr == JSVAL_REPR_JSON) {
			doc = jsval_json_doc(region, value);
			if (jsval_json_string_copy_utf16(region, doc, value.as.index, NULL, 0,
					&len) < 0) {
				return -1;
			}
			if (len == 0) {
				*method_value_ptr = jsmethod_value_string_utf16(NULL, 0);
				return 0;
			}
			if (string_storage == NULL) {
				errno = EINVAL;
				return -1;
			}
			if (jsval_json_string_copy_utf16(region, doc, value.as.index,
					string_storage, string_storage_cap, &len) < 0) {
				return -1;
			}
			*method_value_ptr = jsmethod_value_string_utf16(string_storage, len);
			return 0;
		}
		break;
	case JSVAL_KIND_OBJECT:
	case JSVAL_KIND_ARRAY:
		errno = ENOTSUP;
		return -1;
	default:
		break;
	}

	errno = EINVAL;
	return -1;
}

typedef int (*jsval_method_unary_fn)(jsstr16_t *out, jsmethod_value_t this_value,
		jsmethod_error_t *error);

typedef int (*jsval_method_repeat_fn)(jsstr16_t *out,
		jsmethod_value_t this_value, int have_count,
		jsmethod_value_t count_value, jsmethod_error_t *error);

typedef int (*jsval_method_locale_fn)(jsstr16_t *out, jsmethod_value_t this_value,
		int have_locale, jsmethod_value_t locale_value,
		uint16_t *locale_storage, size_t locale_storage_cap,
		jsmethod_error_t *error);

typedef int (*jsval_method_string_pos_fn)(jsstr16_t *out,
		jsmethod_value_t this_value, int have_position,
		jsmethod_value_t position_value, jsmethod_error_t *error);

typedef int (*jsval_method_string_optional_fn)(jsstr16_t *out,
		int *has_value_ptr, jsmethod_value_t this_value, int have_position,
		jsmethod_value_t position_value, jsmethod_error_t *error);

typedef int (*jsval_method_string_double_fn)(double *value_ptr,
		jsmethod_value_t this_value, int have_position,
		jsmethod_value_t position_value, jsmethod_error_t *error);

typedef int (*jsval_method_string_optional_double_fn)(int *has_value_ptr,
		double *value_ptr, jsmethod_value_t this_value, int have_position,
		jsmethod_value_t position_value, jsmethod_error_t *error);

typedef int (*jsval_method_string_range_fn)(jsstr16_t *out,
		jsmethod_value_t this_value, int have_start,
		jsmethod_value_t start_value, int have_end,
		jsmethod_value_t end_value, jsmethod_error_t *error);

typedef int (*jsval_method_string_index_fn)(ssize_t *index_ptr,
		jsmethod_value_t this_value, jsmethod_value_t search_value,
		int have_position, jsmethod_value_t position_value,
		jsmethod_error_t *error);

typedef int (*jsval_method_string_bool_fn)(int *result_ptr,
		jsmethod_value_t this_value, jsmethod_value_t search_value,
		int have_position, jsmethod_value_t position_value,
		jsmethod_error_t *error);

static int jsval_method_string_unary_bridge(jsval_region_t *region,
		jsval_t this_value, size_t expansion_factor,
		jsval_method_unary_fn fn, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	jsval_native_string_t *result_string;
	jsval_t result;
	jsstr16_t out;
	jsmethod_value_t method_value;
	size_t input_len;
	size_t output_cap;

	if (region == NULL || fn == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_value_utf16_len(region, this_value, &input_len) < 0) {
		return -1;
	}
	if (jsval_method_output_cap(input_len, expansion_factor, &output_cap) < 0) {
		return -1;
	}
	if (jsval_string_reserve_utf16(region, output_cap, &result,
			&result_string) < 0) {
		return -1;
	}

	jsstr16_init_from_buf(&out,
			(const char *)jsval_native_string_units(result_string),
			result_string->cap * sizeof(uint16_t));
	if (jsval_method_value_from_jsval(region, this_value, out.codeunits, out.cap,
			&method_value) < 0) {
		return -1;
	}
	if (fn(&out, method_value, error) < 0) {
		return -1;
	}

	result_string->len = out.len;
	*value_ptr = result;
	return 0;
}

static int
jsval_method_string_repeat_bridge(jsval_region_t *region,
		jsval_t this_value, int have_count, jsval_t count_value,
		jsval_method_repeat_fn fn, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	jsmethod_string_repeat_sizes_t sizes;
	jsval_native_string_t *result_string;
	jsval_t result;
	jsstr16_t out;
	size_t count_storage_cap = 0;
	jsmethod_value_t this_method_value;
	jsmethod_value_t count_method_value = jsmethod_value_undefined();

	if (region == NULL || fn == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_method_string_repeat_measure(region, this_value, have_count,
			count_value, &sizes, error) < 0) {
		return -1;
	}
	if (have_count &&
			jsval_value_utf16_len(region, count_value, &count_storage_cap) < 0) {
		return -1;
	}
	if (jsval_string_reserve_utf16(region, sizes.result_len, &result,
			&result_string) < 0) {
		return -1;
	}

	jsstr16_init_from_buf(&out,
			(const char *)jsval_native_string_units(result_string),
			result_string->cap * sizeof(uint16_t));
	if (jsval_method_value_from_jsval(region, this_value, out.codeunits, out.cap,
			&this_method_value) < 0) {
		return -1;
	}

	{
		uint16_t count_storage[count_storage_cap ? count_storage_cap : 1];

		if (have_count &&
				jsval_method_value_from_jsval(region, count_value,
				count_storage, count_storage_cap,
				&count_method_value) < 0) {
			return -1;
		}
		if (fn(&out, this_method_value, have_count, count_method_value,
				error) < 0) {
			return -1;
		}
	}

	result_string->len = out.len;
	*value_ptr = result;
	return 0;
}

static int jsval_method_string_locale_bridge(jsval_region_t *region,
		jsval_t this_value, int have_locale, jsval_t locale_value,
		size_t expansion_factor, jsval_method_locale_fn fn,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	jsval_native_string_t *result_string;
	jsval_t result;
	jsstr16_t out;
	jsmethod_value_t this_method_value;
	jsmethod_value_t locale_method_value = jsmethod_value_undefined();
	size_t input_len;
	size_t output_cap;
	size_t locale_storage_cap = 1;

	if (region == NULL || fn == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_value_utf16_len(region, this_value, &input_len) < 0) {
		return -1;
	}
	if (jsval_method_output_cap(input_len, expansion_factor, &output_cap) < 0) {
		return -1;
	}
	if (have_locale && locale_value.kind != JSVAL_KIND_UNDEFINED) {
		if (jsval_value_utf16_len(region, locale_value, &locale_storage_cap) < 0) {
			return -1;
		}
		if (locale_storage_cap == 0) {
			locale_storage_cap = 1;
		}
	}
	if (jsval_string_reserve_utf16(region, output_cap, &result,
			&result_string) < 0) {
		return -1;
	}

	jsstr16_init_from_buf(&out,
			(const char *)jsval_native_string_units(result_string),
			result_string->cap * sizeof(uint16_t));
	if (jsval_method_value_from_jsval(region, this_value, out.codeunits, out.cap,
			&this_method_value) < 0) {
		return -1;
	}

	{
		uint16_t locale_storage[locale_storage_cap ? locale_storage_cap : 1];

		if (have_locale &&
		    jsval_method_value_from_jsval(region, locale_value, locale_storage,
			    locale_storage_cap, &locale_method_value) < 0) {
			return -1;
		}
		if (fn(&out, this_method_value, have_locale, locale_method_value,
				locale_storage, locale_storage_cap, error) < 0) {
			return -1;
		}
	}

	result_string->len = out.len;
	*value_ptr = result;
	return 0;
}

static int
jsval_method_string_position_bridge(jsval_region_t *region,
		jsval_t this_value, int have_position, jsval_t position_value,
		jsval_method_string_pos_fn fn, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	size_t this_storage_cap;
	size_t position_storage_cap = 0;
	jsmethod_value_t this_method_value;
	jsmethod_value_t position_method_value = jsmethod_value_undefined();
	uint16_t out_storage[1];
	jsstr16_t out;

	if (region == NULL || fn == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_value_utf16_len(region, this_value, &this_storage_cap) < 0) {
		return -1;
	}
	if (have_position &&
			jsval_value_utf16_len(region, position_value,
			&position_storage_cap) < 0) {
		return -1;
	}

	{
		uint16_t this_storage[this_storage_cap ? this_storage_cap : 1];
		uint16_t position_storage[position_storage_cap ? position_storage_cap : 1];

		jsstr16_init_from_buf(&out, (const char *)out_storage,
				sizeof(out_storage));
		if (jsval_method_value_from_jsval(region, this_value, this_storage,
				this_storage_cap, &this_method_value) < 0) {
			return -1;
		}
		if (have_position &&
				jsval_method_value_from_jsval(region, position_value,
				position_storage, position_storage_cap,
				&position_method_value) < 0) {
			return -1;
		}
		if (fn(&out, this_method_value, have_position, position_method_value,
				error) < 0) {
			return -1;
		}
	}

	return jsval_string_new_utf16(region, out.codeunits, out.len, value_ptr);
}

static int
jsval_method_string_optional_bridge(jsval_region_t *region,
		jsval_t this_value, int have_position, jsval_t position_value,
		jsval_method_string_optional_fn fn, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	size_t this_storage_cap;
	size_t position_storage_cap = 0;
	jsmethod_value_t this_method_value;
	jsmethod_value_t position_method_value = jsmethod_value_undefined();
	uint16_t out_storage[1];
	jsstr16_t out;
	int has_value;

	if (region == NULL || fn == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_value_utf16_len(region, this_value, &this_storage_cap) < 0) {
		return -1;
	}
	if (have_position &&
			jsval_value_utf16_len(region, position_value,
			&position_storage_cap) < 0) {
		return -1;
	}

	{
		uint16_t this_storage[this_storage_cap ? this_storage_cap : 1];
		uint16_t position_storage[position_storage_cap ? position_storage_cap : 1];

		jsstr16_init_from_buf(&out, (const char *)out_storage,
				sizeof(out_storage));
		if (jsval_method_value_from_jsval(region, this_value, this_storage,
				this_storage_cap, &this_method_value) < 0) {
			return -1;
		}
		if (have_position &&
				jsval_method_value_from_jsval(region, position_value,
				position_storage, position_storage_cap,
				&position_method_value) < 0) {
			return -1;
		}
		if (fn(&out, &has_value, this_method_value, have_position,
				position_method_value, error) < 0) {
			return -1;
		}
	}

	if (!has_value) {
		*value_ptr = jsval_undefined();
		return 0;
	}
	return jsval_string_new_utf16(region, out.codeunits, out.len, value_ptr);
}

static int
jsval_method_string_double_bridge(jsval_region_t *region,
		jsval_t this_value, int have_position, jsval_t position_value,
		jsval_method_string_double_fn fn, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	size_t this_storage_cap;
	size_t position_storage_cap = 0;
	jsmethod_value_t this_method_value;
	jsmethod_value_t position_method_value = jsmethod_value_undefined();
	double value;

	if (region == NULL || fn == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_value_utf16_len(region, this_value, &this_storage_cap) < 0) {
		return -1;
	}
	if (have_position &&
			jsval_value_utf16_len(region, position_value,
			&position_storage_cap) < 0) {
		return -1;
	}

	{
		uint16_t this_storage[this_storage_cap ? this_storage_cap : 1];
		uint16_t position_storage[position_storage_cap ? position_storage_cap : 1];

		if (jsval_method_value_from_jsval(region, this_value, this_storage,
				this_storage_cap, &this_method_value) < 0) {
			return -1;
		}
		if (have_position &&
				jsval_method_value_from_jsval(region, position_value,
				position_storage, position_storage_cap,
				&position_method_value) < 0) {
			return -1;
		}
		if (fn(&value, this_method_value, have_position, position_method_value,
				error) < 0) {
			return -1;
		}
	}

	*value_ptr = jsval_number(value);
	return 0;
}

static int
jsval_method_string_optional_double_bridge(jsval_region_t *region,
		jsval_t this_value, int have_position, jsval_t position_value,
		jsval_method_string_optional_double_fn fn, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	size_t this_storage_cap;
	size_t position_storage_cap = 0;
	jsmethod_value_t this_method_value;
	jsmethod_value_t position_method_value = jsmethod_value_undefined();
	int has_value;
	double value;

	if (region == NULL || fn == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_value_utf16_len(region, this_value, &this_storage_cap) < 0) {
		return -1;
	}
	if (have_position &&
			jsval_value_utf16_len(region, position_value,
			&position_storage_cap) < 0) {
		return -1;
	}

	{
		uint16_t this_storage[this_storage_cap ? this_storage_cap : 1];
		uint16_t position_storage[position_storage_cap ? position_storage_cap : 1];

		if (jsval_method_value_from_jsval(region, this_value, this_storage,
				this_storage_cap, &this_method_value) < 0) {
			return -1;
		}
		if (have_position &&
				jsval_method_value_from_jsval(region, position_value,
				position_storage, position_storage_cap,
				&position_method_value) < 0) {
			return -1;
		}
		if (fn(&has_value, &value, this_method_value, have_position,
				position_method_value, error) < 0) {
			return -1;
		}
	}

	if (!has_value) {
		*value_ptr = jsval_undefined();
		return 0;
	}
	*value_ptr = jsval_number(value);
	return 0;
}

static int
jsval_method_string_range_bridge(jsval_region_t *region,
		jsval_t this_value, int have_start, jsval_t start_value,
		int have_end, jsval_t end_value, jsval_method_string_range_fn fn,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	jsval_native_string_t *result_string;
	jsval_t result;
	jsstr16_t out;
	size_t this_storage_cap;
	size_t start_storage_cap = 0;
	size_t end_storage_cap = 0;
	jsmethod_value_t this_method_value;
	jsmethod_value_t start_method_value = jsmethod_value_undefined();
	jsmethod_value_t end_method_value = jsmethod_value_undefined();
	size_t output_cap;

	if (region == NULL || fn == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_value_utf16_len(region, this_value, &this_storage_cap) < 0) {
		return -1;
	}
	output_cap = this_storage_cap;
	if (have_start &&
			jsval_value_utf16_len(region, start_value, &start_storage_cap) < 0) {
		return -1;
	}
	if (have_end &&
			jsval_value_utf16_len(region, end_value, &end_storage_cap) < 0) {
		return -1;
	}
	if (jsval_string_reserve_utf16(region, output_cap, &result,
			&result_string) < 0) {
		return -1;
	}

	{
		uint16_t this_storage[this_storage_cap ? this_storage_cap : 1];
		uint16_t start_storage[start_storage_cap ? start_storage_cap : 1];
		uint16_t end_storage[end_storage_cap ? end_storage_cap : 1];

		jsstr16_init_from_buf(&out,
				(const char *)jsval_native_string_units(result_string),
				result_string->cap * sizeof(uint16_t));
		if (jsval_method_value_from_jsval(region, this_value, this_storage,
				this_storage_cap, &this_method_value) < 0) {
			return -1;
		}
		if (have_start &&
				jsval_method_value_from_jsval(region, start_value, start_storage,
				start_storage_cap, &start_method_value) < 0) {
			return -1;
		}
		if (have_end &&
				jsval_method_value_from_jsval(region, end_value, end_storage,
				end_storage_cap, &end_method_value) < 0) {
			return -1;
		}
		if (fn(&out, this_method_value, have_start, start_method_value,
				have_end, end_method_value, error) < 0) {
			return -1;
		}
	}

	result_string->len = out.len;
	*value_ptr = result;
	return 0;
}

static int
jsval_method_string_index_bridge(jsval_region_t *region, jsval_t this_value,
		jsval_t search_value, int have_position, jsval_t position_value,
		jsval_method_string_index_fn fn, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	size_t this_storage_cap;
	size_t search_storage_cap;
	size_t position_storage_cap = 0;
	jsmethod_value_t this_method_value;
	jsmethod_value_t search_method_value;
	jsmethod_value_t position_method_value = jsmethod_value_undefined();
	ssize_t index;

	if (region == NULL || fn == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_value_utf16_len(region, this_value, &this_storage_cap) < 0) {
		return -1;
	}
	if (jsval_value_utf16_len(region, search_value, &search_storage_cap) < 0) {
		return -1;
	}
	if (have_position &&
			jsval_value_utf16_len(region, position_value,
			&position_storage_cap) < 0) {
		return -1;
	}

	{
		uint16_t this_storage[this_storage_cap ? this_storage_cap : 1];
		uint16_t search_storage[search_storage_cap ? search_storage_cap : 1];
		uint16_t position_storage[position_storage_cap ? position_storage_cap : 1];

		if (jsval_method_value_from_jsval(region, this_value, this_storage,
				this_storage_cap, &this_method_value) < 0) {
			return -1;
		}
		if (jsval_method_value_from_jsval(region, search_value, search_storage,
				search_storage_cap, &search_method_value) < 0) {
			return -1;
		}
		if (have_position &&
				jsval_method_value_from_jsval(region, position_value,
				position_storage, position_storage_cap,
				&position_method_value) < 0) {
			return -1;
		}
		if (fn(&index, this_method_value, search_method_value, have_position,
				position_method_value, error) < 0) {
			return -1;
		}
	}

	*value_ptr = jsval_number((double)index);
	return 0;
}

static int
jsval_method_string_bool_bridge(jsval_region_t *region, jsval_t this_value,
		jsval_t search_value, int have_position, jsval_t position_value,
		jsval_method_string_bool_fn fn, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	size_t this_storage_cap;
	size_t search_storage_cap;
	size_t position_storage_cap = 0;
	jsmethod_value_t this_method_value;
	jsmethod_value_t search_method_value;
	jsmethod_value_t position_method_value = jsmethod_value_undefined();
	int result;

	if (region == NULL || fn == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_value_utf16_len(region, this_value, &this_storage_cap) < 0) {
		return -1;
	}
	if (jsval_value_utf16_len(region, search_value, &search_storage_cap) < 0) {
		return -1;
	}
	if (have_position &&
			jsval_value_utf16_len(region, position_value,
			&position_storage_cap) < 0) {
		return -1;
	}

	{
		uint16_t this_storage[this_storage_cap ? this_storage_cap : 1];
		uint16_t search_storage[search_storage_cap ? search_storage_cap : 1];
		uint16_t position_storage[position_storage_cap ? position_storage_cap : 1];

		if (jsval_method_value_from_jsval(region, this_value, this_storage,
				this_storage_cap, &this_method_value) < 0) {
			return -1;
		}
		if (jsval_method_value_from_jsval(region, search_value, search_storage,
				search_storage_cap, &search_method_value) < 0) {
			return -1;
		}
		if (have_position &&
				jsval_method_value_from_jsval(region, position_value,
				position_storage, position_storage_cap,
				&position_method_value) < 0) {
			return -1;
		}
		if (fn(&result, this_method_value, search_method_value, have_position,
				position_method_value, error) < 0) {
			return -1;
		}
	}

	*value_ptr = jsval_bool(result);
	return 0;
}

int jsval_method_string_normalize_measure(jsval_region_t *region,
		jsval_t this_value, int have_form, jsval_t form_value,
		jsmethod_string_normalize_sizes_t *sizes,
		jsmethod_error_t *error)
{
	jsmethod_value_t this_method_value;
	jsmethod_value_t form_method_value = jsmethod_value_undefined();
	size_t this_storage_cap = 0;
	size_t form_storage_cap = 0;

	if (region == NULL || sizes == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_value_utf16_len(region, this_value, &this_storage_cap) < 0) {
		return -1;
	}
	if (have_form && form_value.kind != JSVAL_KIND_UNDEFINED &&
			jsval_value_utf16_len(region, form_value, &form_storage_cap) < 0) {
		return -1;
	}

	{
		uint16_t this_storage[this_storage_cap ? this_storage_cap : 1];
		uint16_t form_storage[form_storage_cap ? form_storage_cap : 1];

		if (jsval_method_value_from_jsval(region, this_value, this_storage,
				this_storage_cap, &this_method_value) < 0) {
			return -1;
		}
		if (have_form && form_value.kind != JSVAL_KIND_UNDEFINED &&
				jsval_method_value_from_jsval(region, form_value, form_storage,
					form_storage_cap, &form_method_value) < 0) {
			return -1;
		}
		return jsmethod_string_normalize_measure(this_method_value, have_form,
				form_method_value, sizes, error);
	}
}

int jsval_method_string_repeat_measure(jsval_region_t *region,
		jsval_t this_value, int have_count, jsval_t count_value,
		jsmethod_string_repeat_sizes_t *sizes,
		jsmethod_error_t *error)
{
	jsmethod_value_t this_method_value;
	jsmethod_value_t count_method_value = jsmethod_value_undefined();
	size_t this_storage_cap = 0;
	size_t count_storage_cap = 0;

	if (region == NULL || sizes == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_value_utf16_len(region, this_value, &this_storage_cap) < 0) {
		return -1;
	}
	if (have_count &&
			jsval_value_utf16_len(region, count_value, &count_storage_cap) < 0) {
		return -1;
	}

	{
		uint16_t this_storage[this_storage_cap ? this_storage_cap : 1];
		uint16_t count_storage[count_storage_cap ? count_storage_cap : 1];

		if (jsval_method_value_from_jsval(region, this_value, this_storage,
				this_storage_cap, &this_method_value) < 0) {
			return -1;
		}
		if (have_count &&
				jsval_method_value_from_jsval(region, count_value,
				count_storage, count_storage_cap,
				&count_method_value) < 0) {
			return -1;
		}
		return jsmethod_string_repeat_measure(this_method_value, have_count,
				count_method_value, sizes, error);
	}
}

int jsval_method_string_normalize(jsval_region_t *region, jsval_t this_value,
		int have_form, jsval_t form_value,
		uint16_t *this_storage, size_t this_storage_cap,
		uint16_t *form_storage, size_t form_storage_cap,
		uint32_t *workspace, size_t workspace_cap,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	jsmethod_string_normalize_sizes_t sizes;
	jsmethod_value_t this_method_value;
	jsmethod_value_t form_method_value = jsmethod_value_undefined();
	jsval_native_string_t *result_string;
	jsval_t result;
	jsstr16_t out;

	if (region == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_method_string_normalize_measure(region, this_value, have_form,
			form_value, &sizes, error) < 0) {
		return -1;
	}
	if (this_storage_cap < sizes.this_storage_len ||
			form_storage_cap < sizes.form_storage_len ||
			workspace_cap < sizes.workspace_len) {
		errno = ENOBUFS;
		return -1;
	}
	if (jsval_string_reserve_utf16(region, sizes.result_len, &result,
			&result_string) < 0) {
		return -1;
	}

	jsstr16_init_from_buf(&out,
			(const char *)jsval_native_string_units(result_string),
			result_string->cap * sizeof(uint16_t));
	if (jsval_method_value_from_jsval(region, this_value, this_storage,
			this_storage_cap, &this_method_value) < 0) {
		return -1;
	}
	if (have_form && form_value.kind != JSVAL_KIND_UNDEFINED &&
			jsval_method_value_from_jsval(region, form_value, form_storage,
				form_storage_cap, &form_method_value) < 0) {
		return -1;
	}
	if (jsmethod_string_normalize_into(&out, this_method_value,
			this_storage, this_storage_cap,
			have_form, form_method_value,
			form_storage, form_storage_cap,
			workspace, workspace_cap, error) < 0) {
		return -1;
	}

	result_string->len = out.len;
	*value_ptr = result;
	return 0;
}

void jsval_region_init(jsval_region_t *region, void *buf, size_t len)
{
	size_t head_size = jsval_pages_head_size_aligned();

	region->base = (uint8_t *)buf;
	region->len = len;
	region->used = 0;
	region->pages = NULL;

	if (buf == NULL || len < head_size || len > UINT32_MAX) {
		return;
	}

	region->pages = (jsval_pages_t *)buf;
	region->pages->magic = JSVAL_PAGES_MAGIC;
	region->pages->version = JSVAL_PAGES_VERSION;
	region->pages->header_size = sizeof(jsval_pages_t);
	region->pages->total_len = (uint32_t)len;
	region->pages->used_len = (uint32_t)head_size;
	region->pages->root = jsval_undefined();
	region->used = head_size;
}

void jsval_region_rebase(jsval_region_t *region, void *buf, size_t len)
{
	jsval_pages_t *pages;

	region->base = (uint8_t *)buf;
	region->len = len;
	region->used = 0;
	region->pages = NULL;

	if (buf == NULL || len < sizeof(jsval_pages_t)) {
		return;
	}

	pages = (jsval_pages_t *)buf;
	if (pages->magic != JSVAL_PAGES_MAGIC ||
	    pages->version != JSVAL_PAGES_VERSION ||
	    pages->header_size != sizeof(jsval_pages_t) ||
	    pages->used_len < jsval_pages_head_size_aligned() ||
	    pages->used_len > pages->total_len ||
	    len < pages->total_len) {
		return;
	}

	region->pages = pages;
	region->len = pages->total_len;
	region->used = pages->used_len;
}

size_t jsval_region_remaining(jsval_region_t *region)
{
	if (!jsval_region_valid(region)) {
		return 0;
	}
	if (region->used >= region->len) {
		return 0;
	}
	return region->len - region->used;
}

size_t jsval_pages_head_size(void)
{
	return jsval_pages_head_size_aligned();
}

const jsval_pages_t *jsval_region_pages(const jsval_region_t *region)
{
	if (!jsval_region_valid(region)) {
		return NULL;
	}
	return region->pages;
}

int jsval_region_root(jsval_region_t *region, jsval_t *value_ptr)
{
	if (!jsval_region_valid(region) || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}

	*value_ptr = region->pages->root;
	return 0;
}

int jsval_region_set_root(jsval_region_t *region, jsval_t value)
{
	if (!jsval_region_valid(region)) {
		errno = EINVAL;
		return -1;
	}

	region->pages->root = value;
	return 0;
}

int jsval_is_native(jsval_t value)
{
	return value.repr == JSVAL_REPR_NATIVE;
}

int jsval_is_json_backed(jsval_t value)
{
	return value.repr == JSVAL_REPR_JSON;
}

jsval_t jsval_undefined(void)
{
	jsval_t value;
	memset(&value, 0, sizeof(value));
	value.kind = JSVAL_KIND_UNDEFINED;
	value.repr = JSVAL_REPR_INLINE;
	return value;
}

jsval_t jsval_null(void)
{
	jsval_t value = jsval_undefined();
	value.kind = JSVAL_KIND_NULL;
	return value;
}

jsval_t jsval_bool(int boolean)
{
	jsval_t value = jsval_undefined();
	value.kind = JSVAL_KIND_BOOL;
	value.as.boolean = boolean ? 1 : 0;
	return value;
}

jsval_t jsval_number(double number)
{
	jsval_t value = jsval_undefined();
	value.kind = JSVAL_KIND_NUMBER;
	value.as.number = number;
	return value;
}

int jsval_string_new_utf8(jsval_region_t *region, const uint8_t *str, size_t len, jsval_t *value_ptr)
{
	size_t utf16_len;
	jsval_native_string_t *string;
	jsval_off_t off;
	size_t bytes_len;
	const uint8_t *read;
	const uint8_t *read_stop;
	uint16_t *write;
	uint16_t *write_stop;

	utf16_len = jsval_utf8_utf16len(str, len);
	bytes_len = sizeof(*string) + utf16_len * sizeof(uint16_t);
	if (jsval_region_reserve(region, bytes_len, JSVAL_ALIGN, &off, (void **)&string) < 0) {
		return -1;
	}

	string->len = utf16_len;
	string->cap = utf16_len;
	read = str;
	read_stop = str + len;
	write = jsval_native_string_units(string);
	write_stop = write + utf16_len;
	while (read < read_stop) {
		uint32_t codepoint = 0;
		int seq_len = 0;
		const uint32_t *codepoint_ptr = &codepoint;

		UTF8_CHAR(read, read_stop, &codepoint, &seq_len);
		if (seq_len == 0) {
			break;
		}
		if (seq_len < 0) {
			seq_len = -seq_len;
			codepoint = 0xFFFD;
		}

		UTF16_ENCODE(&codepoint_ptr, codepoint_ptr + 1, &write, write_stop);
		read += seq_len;
	}

	*value_ptr = jsval_undefined();
	value_ptr->kind = JSVAL_KIND_STRING;
	value_ptr->repr = JSVAL_REPR_NATIVE;
	value_ptr->off = off;
	return 0;
}

int jsval_string_new_utf16(jsval_region_t *region, const uint16_t *str, size_t len,
		jsval_t *value_ptr)
{
	jsval_native_string_t *string;
	jsval_t value;

	if (region == NULL || value_ptr == NULL || (len > 0 && str == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_string_reserve_utf16(region, len, &value, &string) < 0) {
		return -1;
	}
	if (len > 0) {
		memcpy(jsval_native_string_units(string), str, len * sizeof(uint16_t));
	}
	string->len = len;
	*value_ptr = value;
	return 0;
}

int jsval_object_new(jsval_region_t *region, size_t cap, jsval_t *value_ptr)
{
	jsval_native_object_t *object;
	jsval_off_t off;
	size_t bytes_len = sizeof(*object) + cap * sizeof(jsval_native_prop_t);
	size_t i;

	if (jsval_region_reserve(region, bytes_len, JSVAL_ALIGN, &off, (void **)&object) < 0) {
		return -1;
	}

	object->len = 0;
	object->cap = cap;
	for (i = 0; i < cap; i++) {
		jsval_native_prop_t *prop = &jsval_native_object_props(object)[i];
		prop->name_off = 0;
		prop->value = jsval_undefined();
	}

	*value_ptr = jsval_undefined();
	value_ptr->kind = JSVAL_KIND_OBJECT;
	value_ptr->repr = JSVAL_REPR_NATIVE;
	value_ptr->off = off;
	return 0;
}

int jsval_array_new(jsval_region_t *region, size_t cap, jsval_t *value_ptr)
{
	jsval_native_array_t *array;
	jsval_off_t off;
	size_t bytes_len = sizeof(*array) + cap * sizeof(jsval_t);
	size_t i;

	if (jsval_region_reserve(region, bytes_len, JSVAL_ALIGN, &off, (void **)&array) < 0) {
		return -1;
	}

	array->len = 0;
	array->cap = cap;
	for (i = 0; i < cap; i++) {
		jsval_native_array_values(array)[i] = jsval_undefined();
	}

	*value_ptr = jsval_undefined();
	value_ptr->kind = JSVAL_KIND_ARRAY;
	value_ptr->repr = JSVAL_REPR_NATIVE;
	value_ptr->off = off;
	return 0;
}

int jsval_json_parse(jsval_region_t *region, const uint8_t *json, size_t len, unsigned int token_cap, jsval_t *value_ptr)
{
	jsval_json_doc_t *doc;
	jsval_off_t doc_off;
	jsval_off_t json_off;
	jsval_off_t tokens_off;
	uint8_t *json_copy;
	jsmntok_t *tokens;
	jsmn_parser parser;
	int rc;

	if (token_cap == 0) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_region_reserve(region, sizeof(*doc), JSVAL_ALIGN, &doc_off, (void **)&doc) < 0) {
		return -1;
	}
	if (jsval_region_reserve(region, len + 1, 1, &json_off, (void **)&json_copy) < 0) {
		return -1;
	}
	if (jsval_region_reserve(region, token_cap * sizeof(jsmntok_t), JSVAL_ALIGN, &tokens_off, (void **)&tokens) < 0) {
		return -1;
	}

	memcpy(json_copy, json, len);
	json_copy[len] = '\0';
	memset(tokens, 0, token_cap * sizeof(jsmntok_t));

	jsmn_init(&parser);
	rc = jsmn_parse(&parser, (const char *)json_copy, len, tokens, token_cap);
	if (rc < 0) {
		return rc;
	}

	doc->json_off = json_off;
	doc->json_len = len;
	doc->tokens_off = tokens_off;
	doc->tokcap = token_cap;
	doc->tokused = parser.toknext;
	doc->root_i = 0;

	*value_ptr = jsval_undefined();
	value_ptr->repr = JSVAL_REPR_JSON;
	value_ptr->off = doc_off;
	value_ptr->as.index = 0;
	value_ptr->kind = jsval_json_token_kind(region, doc, 0);
	jsval_region_set_root(region, *value_ptr);
	return 0;
}

int jsval_copy_json(jsval_region_t *region, jsval_t value, uint8_t *buf, size_t cap, size_t *len_ptr)
{
	jsval_json_emit_state_t state;

	memset(&state, 0, sizeof(state));
	state.buf = buf;
	state.cap = cap;
	if (jsval_json_emit_value(region, value, &state) < 0) {
		return -1;
	}
	if (len_ptr != NULL) {
		*len_ptr = state.len;
	}
	return 0;
}

int jsval_string_copy_utf8(jsval_region_t *region, jsval_t value, uint8_t *buf, size_t cap, size_t *len_ptr)
{
	if (value.kind != JSVAL_KIND_STRING) {
		errno = EINVAL;
		return -1;
	}

	if (value.repr == JSVAL_REPR_NATIVE) {
		return jsval_native_string_copy_utf8(region, value, buf, cap, len_ptr);
	}
	if (value.repr == JSVAL_REPR_JSON) {
		jsval_json_doc_t *doc = jsval_json_doc(region, value);
		return jsval_json_string_copy_utf8_internal(region, doc, value.as.index, buf, cap, len_ptr);
	}

	errno = EINVAL;
	return -1;
}

size_t jsval_object_size(jsval_region_t *region, jsval_t object)
{
	if (object.kind != JSVAL_KIND_OBJECT) {
		return 0;
	}
	if (object.repr == JSVAL_REPR_NATIVE) {
		jsval_native_object_t *native = jsval_native_object(region, object);
		return native == NULL ? 0 : native->len;
	}
	if (object.repr == JSVAL_REPR_JSON) {
		jsval_json_doc_t *doc = jsval_json_doc(region, object);
		jsmntok_t *tokens = jsval_json_doc_tokens(region, doc);
		return tokens[object.as.index].size;
	}
	return 0;
}

size_t jsval_array_length(jsval_region_t *region, jsval_t array)
{
	if (array.kind != JSVAL_KIND_ARRAY) {
		return 0;
	}
	if (array.repr == JSVAL_REPR_NATIVE) {
		jsval_native_array_t *native = jsval_native_array(region, array);
		return native == NULL ? 0 : native->len;
	}
	if (array.repr == JSVAL_REPR_JSON) {
		jsval_json_doc_t *doc = jsval_json_doc(region, array);
		jsmntok_t *tokens = jsval_json_doc_tokens(region, doc);
		return tokens[array.as.index].size;
	}
	return 0;
}

static int jsval_native_object_find_utf8(jsval_region_t *region,
		jsval_native_object_t *native, const uint8_t *key, size_t key_len,
		size_t *index_ptr)
{
	size_t i;
	jsval_native_prop_t *props;

	if (native == NULL) {
		errno = EINVAL;
		return -1;
	}

	props = jsval_native_object_props(native);
	for (i = 0; i < native->len; i++) {
		jsval_t name = jsval_undefined();

		name.kind = JSVAL_KIND_STRING;
		name.repr = JSVAL_REPR_NATIVE;
		name.off = props[i].name_off;
		if (jsval_native_string_eq_utf8(region, name, key, key_len)) {
			if (index_ptr != NULL) {
				*index_ptr = i;
			}
			return 1;
		}
	}

	return 0;
}

int jsval_object_has_own_utf8(jsval_region_t *region, jsval_t object,
		const uint8_t *key, size_t key_len, int *has_ptr)
{
	if (has_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (object.kind != JSVAL_KIND_OBJECT) {
		errno = EINVAL;
		return -1;
	}

	if (object.repr == JSVAL_REPR_NATIVE) {
		jsval_native_object_t *native = jsval_native_object(region, object);
		int found = jsval_native_object_find_utf8(region, native, key, key_len, NULL);

		if (found < 0) {
			return -1;
		}
		*has_ptr = found;
		return 0;
	}

	if (object.repr == JSVAL_REPR_JSON) {
		int cursor;
		unsigned int i;
		jsval_json_doc_t *doc = jsval_json_doc(region, object);
		jsmntok_t *tokens = jsval_json_doc_tokens(region, doc);

		cursor = (int)object.as.index + 1;
		for (i = 0; i < (unsigned int)tokens[object.as.index].size; i++) {
			int key_index = cursor;
			int value_index = jsval_json_next(region, doc, key_index);

			if (value_index < 0) {
				break;
			}
			if (jsval_json_string_eq_utf8(region, doc, key_index, key, key_len)) {
				*has_ptr = 1;
				return 0;
			}
			cursor = jsval_json_next(region, doc, value_index);
			if (cursor < 0) {
				break;
			}
		}

		*has_ptr = 0;
		return 0;
	}

	errno = EINVAL;
	return -1;
}

int jsval_object_get_utf8(jsval_region_t *region, jsval_t object, const uint8_t *key, size_t key_len, jsval_t *value_ptr)
{
	if (object.kind != JSVAL_KIND_OBJECT) {
		errno = EINVAL;
		return -1;
	}

	if (object.repr == JSVAL_REPR_NATIVE) {
		jsval_native_object_t *native = jsval_native_object(region, object);
		jsval_native_prop_t *props;
		size_t index;
		int found;

		found = jsval_native_object_find_utf8(region, native, key, key_len, &index);
		if (found < 0) {
			return -1;
		}
		props = jsval_native_object_props(native);
		if (found) {
			*value_ptr = props[index].value;
			return 0;
		}

		*value_ptr = jsval_undefined();
		return 0;
	}

	if (object.repr == JSVAL_REPR_JSON) {
		int cursor;
		unsigned int i;
		jsval_json_doc_t *doc = jsval_json_doc(region, object);
		jsmntok_t *tokens = jsval_json_doc_tokens(region, doc);

		cursor = (int)object.as.index + 1;
		for (i = 0; i < (unsigned int)tokens[object.as.index].size; i++) {
			int key_index = cursor;
			int value_index = jsval_json_next(region, doc, key_index);
			if (value_index < 0) {
				break;
			}
			if (jsval_json_string_eq_utf8(region, doc, key_index, key, key_len)) {
				*value_ptr = jsval_json_value(region, object, (uint32_t)value_index);
				return 0;
			}
			cursor = jsval_json_next(region, doc, value_index);
			if (cursor < 0) {
				break;
			}
		}

		*value_ptr = jsval_undefined();
		return 0;
	}

	errno = EINVAL;
	return -1;
}

int jsval_object_set_utf8(jsval_region_t *region, jsval_t object, const uint8_t *key, size_t key_len, jsval_t value)
{
	size_t index;
	jsval_native_object_t *native;
	jsval_native_prop_t *props;
	int found;

	if (object.kind != JSVAL_KIND_OBJECT || object.repr != JSVAL_REPR_NATIVE) {
		errno = ENOTSUP;
		return -1;
	}

	native = jsval_native_object(region, object);
	if (native == NULL) {
		errno = EINVAL;
		return -1;
	}

	props = jsval_native_object_props(native);
	found = jsval_native_object_find_utf8(region, native, key, key_len, &index);
	if (found < 0) {
		return -1;
	}
	if (found) {
		props[index].value = value;
		return 0;
	}

	if (native->len >= native->cap) {
		errno = ENOBUFS;
		return -1;
	}

	{
		jsval_t name;
		if (jsval_string_new_utf8(region, key, key_len, &name) < 0) {
			return -1;
		}
		props[native->len].name_off = name.off;
		props[native->len].value = value;
		native->len++;
	}

	return 0;
}

int jsval_object_delete_utf8(jsval_region_t *region, jsval_t object,
		const uint8_t *key, size_t key_len, int *deleted_ptr)
{
	jsval_native_object_t *native;
	jsval_native_prop_t *props;
	size_t index;
	size_t i;
	int found;

	if (deleted_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (object.kind != JSVAL_KIND_OBJECT) {
		errno = EINVAL;
		return -1;
	}
	if (object.repr != JSVAL_REPR_NATIVE) {
		errno = ENOTSUP;
		return -1;
	}

	native = jsval_native_object(region, object);
	if (native == NULL) {
		errno = EINVAL;
		return -1;
	}

	found = jsval_native_object_find_utf8(region, native, key, key_len, &index);
	if (found < 0) {
		return -1;
	}
	if (!found) {
		*deleted_ptr = 0;
		return 0;
	}

	props = jsval_native_object_props(native);
	for (i = index + 1; i < native->len; i++) {
		props[i - 1] = props[i];
	}
	props[native->len - 1].name_off = 0;
	props[native->len - 1].value = jsval_undefined();
	native->len--;
	*deleted_ptr = 1;
	return 0;
}

int jsval_array_get(jsval_region_t *region, jsval_t array, size_t index, jsval_t *value_ptr)
{
	if (array.kind != JSVAL_KIND_ARRAY) {
		errno = EINVAL;
		return -1;
	}

	if (array.repr == JSVAL_REPR_NATIVE) {
		jsval_native_array_t *native = jsval_native_array(region, array);
		if (native == NULL) {
			errno = EINVAL;
			return -1;
		}
		if (index >= native->len) {
			*value_ptr = jsval_undefined();
			return 0;
		}
		*value_ptr = jsval_native_array_values(native)[index];
		return 0;
	}

	if (array.repr == JSVAL_REPR_JSON) {
		int cursor;
		size_t i;
		jsval_json_doc_t *doc = jsval_json_doc(region, array);
		jsmntok_t *tokens = jsval_json_doc_tokens(region, doc);

		if (index >= (size_t)tokens[array.as.index].size) {
			*value_ptr = jsval_undefined();
			return 0;
		}

		cursor = (int)array.as.index + 1;
		for (i = 0; i < index; i++) {
			cursor = jsval_json_next(region, doc, cursor);
			if (cursor < 0) {
				errno = EINVAL;
				return -1;
			}
		}

		*value_ptr = jsval_json_value(region, array, (uint32_t)cursor);
		return 0;
	}

	errno = EINVAL;
	return -1;
}

int jsval_array_set(jsval_region_t *region, jsval_t array, size_t index, jsval_t value)
{
	size_t i;
	jsval_native_array_t *native;
	jsval_t *values;

	if (array.kind != JSVAL_KIND_ARRAY || array.repr != JSVAL_REPR_NATIVE) {
		errno = ENOTSUP;
		return -1;
	}

	native = jsval_native_array(region, array);
	if (native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (index >= native->cap) {
		errno = ENOBUFS;
		return -1;
	}

	values = jsval_native_array_values(native);
	for (i = native->len; i < index; i++) {
		values[i] = jsval_undefined();
	}
	values[index] = value;
	if (index >= native->len) {
		native->len = index + 1;
	}
	return 0;
}

int jsval_array_push(jsval_region_t *region, jsval_t array, jsval_t value)
{
	size_t len;

	if (array.kind != JSVAL_KIND_ARRAY) {
		errno = EINVAL;
		return -1;
	}
	if (array.repr != JSVAL_REPR_NATIVE) {
		errno = ENOTSUP;
		return -1;
	}

	len = jsval_array_length(region, array);
	return jsval_array_set(region, array, len, value);
}

int jsval_array_set_length(jsval_region_t *region, jsval_t array, size_t new_len)
{
	jsval_native_array_t *native;
	jsval_t *values;
	size_t i;

	if (array.kind != JSVAL_KIND_ARRAY) {
		errno = EINVAL;
		return -1;
	}
	if (array.repr != JSVAL_REPR_NATIVE) {
		errno = ENOTSUP;
		return -1;
	}

	native = jsval_native_array(region, array);
	if (native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (new_len > native->cap) {
		errno = ENOBUFS;
		return -1;
	}

	values = jsval_native_array_values(native);
	if (new_len < native->len) {
		for (i = new_len; i < native->len; i++) {
			values[i] = jsval_undefined();
		}
	} else if (new_len > native->len) {
		for (i = native->len; i < new_len; i++) {
			values[i] = jsval_undefined();
		}
	}
	native->len = new_len;
	return 0;
}

int jsval_truthy(jsval_region_t *region, jsval_t value)
{
	jsval_json_doc_t *doc;
	size_t len;
	double number;
	int boolean;

	switch (value.kind) {
	case JSVAL_KIND_UNDEFINED:
	case JSVAL_KIND_NULL:
		return 0;
	case JSVAL_KIND_BOOL:
		if (value.repr == JSVAL_REPR_JSON) {
			doc = jsval_json_doc(region, value);
			if (jsval_json_bool_value(region, doc, value.as.index, &boolean) < 0) {
				return 0;
			}
			return boolean;
		}
		return value.as.boolean != 0;
	case JSVAL_KIND_NUMBER:
		if (value.repr == JSVAL_REPR_JSON) {
			doc = jsval_json_doc(region, value);
			if (jsval_json_number_value(region, doc, value.as.index, &number) < 0) {
				return 0;
			}
			return number != 0 && number == number;
		}
		return value.as.number != 0 && value.as.number == value.as.number;
	case JSVAL_KIND_STRING:
		if (value.repr == JSVAL_REPR_NATIVE) {
			jsval_native_string_t *string = jsval_native_string(region, value);
			return string != NULL && string->len > 0;
		}
		doc = jsval_json_doc(region, value);
		if (jsval_json_string_copy_utf16(region, doc, value.as.index, NULL, 0, &len) < 0) {
			return 0;
		}
		return len > 0;
	case JSVAL_KIND_OBJECT:
	case JSVAL_KIND_ARRAY:
		return 1;
	default:
		return 0;
	}
}

int jsval_strict_eq(jsval_region_t *region, jsval_t left, jsval_t right)
{
	if (left.kind != right.kind) {
		return 0;
	}

	switch (left.kind) {
	case JSVAL_KIND_UNDEFINED:
	case JSVAL_KIND_NULL:
		return 1;
	case JSVAL_KIND_BOOL:
		return jsval_truthy(region, left) == jsval_truthy(region, right);
	case JSVAL_KIND_NUMBER:
	{
		double left_number;
		double right_number;

		if (jsval_to_number(region, left, &left_number) < 0 || jsval_to_number(region, right, &right_number) < 0) {
			return 0;
		}
		if (left_number != left_number || right_number != right_number) {
			return 0;
		}
		return left_number == right_number;
	}
	case JSVAL_KIND_STRING:
	{
		size_t left_len;
		size_t right_len;

		if (jsval_value_utf16_len(region, left, &left_len) < 0 || jsval_value_utf16_len(region, right, &right_len) < 0) {
			return 0;
		}
		if (left_len != right_len) {
			return 0;
		}

		{
			uint16_t left_buf[left_len ? left_len : 1];
			uint16_t right_buf[right_len ? right_len : 1];

			if (jsval_value_copy_utf16(region, left, left_buf, left_len, NULL) < 0 ||
			    jsval_value_copy_utf16(region, right, right_buf, right_len, NULL) < 0) {
				return 0;
			}
			return memcmp(left_buf, right_buf, left_len * sizeof(uint16_t)) == 0;
		}
	}
	case JSVAL_KIND_OBJECT:
	case JSVAL_KIND_ARRAY:
		if (left.repr != right.repr) {
			return 0;
		}
		if (left.repr == JSVAL_REPR_JSON) {
			return left.off == right.off && left.as.index == right.as.index;
		}
		return left.off == right.off;
	default:
		return 0;
	}
}

int jsval_abstract_eq(jsval_region_t *region, jsval_t left, jsval_t right,
		int *result_ptr)
{
	double left_number;
	double right_number;

	if (result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (left.kind == JSVAL_KIND_OBJECT || left.kind == JSVAL_KIND_ARRAY
			|| right.kind == JSVAL_KIND_OBJECT
			|| right.kind == JSVAL_KIND_ARRAY) {
		errno = ENOTSUP;
		return -1;
	}
	if (left.kind == right.kind) {
		*result_ptr = jsval_strict_eq(region, left, right);
		return 0;
	}
	if ((left.kind == JSVAL_KIND_UNDEFINED && right.kind == JSVAL_KIND_NULL)
			|| (left.kind == JSVAL_KIND_NULL
				&& right.kind == JSVAL_KIND_UNDEFINED)) {
		*result_ptr = 1;
		return 0;
	}
	if (left.kind == JSVAL_KIND_UNDEFINED || left.kind == JSVAL_KIND_NULL
			|| right.kind == JSVAL_KIND_UNDEFINED
			|| right.kind == JSVAL_KIND_NULL) {
		*result_ptr = 0;
		return 0;
	}
	if ((left.kind == JSVAL_KIND_BOOL || left.kind == JSVAL_KIND_NUMBER
				|| left.kind == JSVAL_KIND_STRING)
			&& (right.kind == JSVAL_KIND_BOOL || right.kind == JSVAL_KIND_NUMBER
				|| right.kind == JSVAL_KIND_STRING)) {
		if (jsval_to_number(region, left, &left_number) < 0
				|| jsval_to_number(region, right, &right_number) < 0) {
			return -1;
		}
		if (left_number != left_number || right_number != right_number) {
			*result_ptr = 0;
			return 0;
		}
		*result_ptr = left_number == right_number;
		return 0;
	}

	errno = EINVAL;
	return -1;
}

int jsval_abstract_ne(jsval_region_t *region, jsval_t left, jsval_t right,
		int *result_ptr)
{
	int result;

	if (result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_abstract_eq(region, left, right, &result) < 0) {
		return -1;
	}
	*result_ptr = !result;
	return 0;
}

static int
jsval_string_compare(jsval_region_t *region, jsval_t left, jsval_t right,
		int *result_ptr)
{
	size_t left_len;
	size_t right_len;
	size_t min_len;
	int result = 0;

	if (result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (left.kind != JSVAL_KIND_STRING || right.kind != JSVAL_KIND_STRING) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_value_utf16_len(region, left, &left_len) < 0
			|| jsval_value_utf16_len(region, right, &right_len) < 0) {
		return -1;
	}

	{
		uint16_t left_buf[left_len ? left_len : 1];
		uint16_t right_buf[right_len ? right_len : 1];

		if (jsval_value_copy_utf16(region, left, left_buf, left_len, NULL) < 0
				|| jsval_value_copy_utf16(region, right, right_buf, right_len,
					NULL) < 0) {
			return -1;
		}

		min_len = left_len < right_len ? left_len : right_len;
		for (size_t i = 0; i < min_len; i++) {
			if (left_buf[i] < right_buf[i]) {
				result = -1;
				goto done;
			}
			if (left_buf[i] > right_buf[i]) {
				result = 1;
				goto done;
			}
		}
	}

	if (left_len < right_len) {
		result = -1;
	} else if (left_len > right_len) {
		result = 1;
	}

done:
	*result_ptr = result;
	return 0;
}

typedef enum jsval_relop_e {
	JSVAL_RELOP_LT,
	JSVAL_RELOP_LE,
	JSVAL_RELOP_GT,
	JSVAL_RELOP_GE
} jsval_relop_t;

static int
jsval_relop(jsval_region_t *region, jsval_t left, jsval_t right,
		jsval_relop_t op, int *result_ptr)
{
	double left_number;
	double right_number;

	if (result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (left.kind == JSVAL_KIND_STRING && right.kind == JSVAL_KIND_STRING) {
		int cmp;

		if (jsval_string_compare(region, left, right, &cmp) < 0) {
			return -1;
		}
		switch (op) {
		case JSVAL_RELOP_LT:
			*result_ptr = cmp < 0;
			return 0;
		case JSVAL_RELOP_LE:
			*result_ptr = cmp <= 0;
			return 0;
		case JSVAL_RELOP_GT:
			*result_ptr = cmp > 0;
			return 0;
		case JSVAL_RELOP_GE:
			*result_ptr = cmp >= 0;
			return 0;
		default:
			errno = EINVAL;
			return -1;
		}
	}
	if (jsval_to_number(region, left, &left_number) < 0
			|| jsval_to_number(region, right, &right_number) < 0) {
		return -1;
	}
	if (left_number != left_number || right_number != right_number) {
		*result_ptr = 0;
		return 0;
	}

	switch (op) {
	case JSVAL_RELOP_LT:
		*result_ptr = left_number < right_number;
		return 0;
	case JSVAL_RELOP_LE:
		*result_ptr = left_number <= right_number;
		return 0;
	case JSVAL_RELOP_GT:
		*result_ptr = left_number > right_number;
		return 0;
	case JSVAL_RELOP_GE:
		*result_ptr = left_number >= right_number;
		return 0;
	default:
		errno = EINVAL;
		return -1;
	}
}

int jsval_less_than(jsval_region_t *region, jsval_t left, jsval_t right,
		int *result_ptr)
{
	return jsval_relop(region, left, right, JSVAL_RELOP_LT, result_ptr);
}

int jsval_less_equal(jsval_region_t *region, jsval_t left, jsval_t right,
		int *result_ptr)
{
	return jsval_relop(region, left, right, JSVAL_RELOP_LE, result_ptr);
}

int jsval_greater_than(jsval_region_t *region, jsval_t left, jsval_t right,
		int *result_ptr)
{
	return jsval_relop(region, left, right, JSVAL_RELOP_GT, result_ptr);
}

int jsval_greater_equal(jsval_region_t *region, jsval_t left, jsval_t right,
		int *result_ptr)
{
	return jsval_relop(region, left, right, JSVAL_RELOP_GE, result_ptr);
}

int jsval_add(jsval_region_t *region, jsval_t left, jsval_t right, jsval_t *value_ptr)
{
	if (left.kind == JSVAL_KIND_STRING || right.kind == JSVAL_KIND_STRING) {
		size_t left_len;
		size_t right_len;
		uint16_t *buf;
		size_t total_len;
		jsval_native_string_t *string;
		jsval_off_t off;

		if (jsval_value_utf16_len(region, left, &left_len) < 0 || jsval_value_utf16_len(region, right, &right_len) < 0) {
			return -1;
		}

		total_len = left_len + right_len;
		if (jsval_region_reserve(region, sizeof(*string) + total_len * sizeof(uint16_t), JSVAL_ALIGN, &off, (void **)&string) < 0) {
			return -1;
		}

		string->len = total_len;
		string->cap = total_len;
		buf = jsval_native_string_units(string);
		if (jsval_value_copy_utf16(region, left, buf, left_len, NULL) < 0) {
			return -1;
		}
		if (jsval_value_copy_utf16(region, right, buf + left_len, right_len, NULL) < 0) {
			return -1;
		}

		*value_ptr = jsval_undefined();
		value_ptr->kind = JSVAL_KIND_STRING;
		value_ptr->repr = JSVAL_REPR_NATIVE;
		value_ptr->off = off;
		return 0;
	}

	{
		double left_number;
		double right_number;

		if (jsval_to_number(region, left, &left_number) < 0 || jsval_to_number(region, right, &right_number) < 0) {
			return -1;
		}

		*value_ptr = jsval_number(left_number + right_number);
		return 0;
	}
}

int jsval_unary_plus(jsval_region_t *region, jsval_t value, jsval_t *value_ptr)
{
	double number;

	if (value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_to_number(region, value, &number) < 0) {
		return -1;
	}

	*value_ptr = jsval_number(number);
	return 0;
}

int jsval_unary_minus(jsval_region_t *region, jsval_t value, jsval_t *value_ptr)
{
	double number;

	if (value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_to_number(region, value, &number) < 0) {
		return -1;
	}

	*value_ptr = jsval_number(-number);
	return 0;
}

int jsval_bitwise_not(jsval_region_t *region, jsval_t value,
		jsval_t *value_ptr)
{
	int32_t integer;

	if (value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_to_int32(region, value, &integer) < 0) {
		return -1;
	}

	*value_ptr = jsval_number((double)(~integer));
	return 0;
}

typedef enum jsval_bitwise_op_e {
	JSVAL_BITWISE_AND,
	JSVAL_BITWISE_OR,
	JSVAL_BITWISE_XOR
} jsval_bitwise_op_t;

typedef enum jsval_shift_op_e {
	JSVAL_SHIFT_LEFT,
	JSVAL_SHIFT_RIGHT,
	JSVAL_SHIFT_RIGHT_UNSIGNED
} jsval_shift_op_t;

static int jsval_bitwise(jsval_region_t *region, jsval_t left, jsval_t right,
		jsval_bitwise_op_t op, jsval_t *value_ptr)
{
	int32_t left_integer;
	int32_t right_integer;
	int32_t result;

	if (value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_to_int32(region, left, &left_integer) < 0
			|| jsval_to_int32(region, right, &right_integer) < 0) {
		return -1;
	}

	switch (op) {
	case JSVAL_BITWISE_AND:
		result = left_integer & right_integer;
		break;
	case JSVAL_BITWISE_OR:
		result = left_integer | right_integer;
		break;
	case JSVAL_BITWISE_XOR:
		result = left_integer ^ right_integer;
		break;
	default:
		errno = EINVAL;
		return -1;
	}

	*value_ptr = jsval_number((double)result);
	return 0;
}

static int jsval_shift(jsval_region_t *region, jsval_t left, jsval_t right,
		jsval_shift_op_t op, jsval_t *value_ptr)
{
	uint32_t right_integer;
	uint32_t shift_count;
	uint32_t bits;
	int32_t left_integer;

	if (value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_to_uint32(region, right, &right_integer) < 0) {
		return -1;
	}

	shift_count = right_integer & 0x1f;
	switch (op) {
	case JSVAL_SHIFT_LEFT:
		if (jsval_to_int32(region, left, &left_integer) < 0) {
			return -1;
		}
		bits = ((uint32_t)left_integer) << shift_count;
		*value_ptr = jsval_number((double)(int32_t)bits);
		return 0;
	case JSVAL_SHIFT_RIGHT:
		if (jsval_to_int32(region, left, &left_integer) < 0) {
			return -1;
		}
		if (shift_count == 0) {
			*value_ptr = jsval_number((double)left_integer);
			return 0;
		}

		bits = ((uint32_t)left_integer) >> shift_count;
		if (left_integer < 0) {
			bits |= (~(uint32_t)0) << (32 - shift_count);
		}
		*value_ptr = jsval_number((double)(int32_t)bits);
		return 0;
	case JSVAL_SHIFT_RIGHT_UNSIGNED:
		if (jsval_to_uint32(region, left, &bits) < 0) {
			return -1;
		}
		*value_ptr = jsval_number((double)(bits >> shift_count));
		return 0;
	default:
		errno = EINVAL;
		return -1;
	}
}

int jsval_subtract(jsval_region_t *region, jsval_t left, jsval_t right,
		jsval_t *value_ptr)
{
	double left_number;
	double right_number;

	if (value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_to_number(region, left, &left_number) < 0
			|| jsval_to_number(region, right, &right_number) < 0) {
		return -1;
	}

	*value_ptr = jsval_number(left_number - right_number);
	return 0;
}

int jsval_bitwise_and(jsval_region_t *region, jsval_t left, jsval_t right,
		jsval_t *value_ptr)
{
	return jsval_bitwise(region, left, right, JSVAL_BITWISE_AND, value_ptr);
}

int jsval_bitwise_or(jsval_region_t *region, jsval_t left, jsval_t right,
		jsval_t *value_ptr)
{
	return jsval_bitwise(region, left, right, JSVAL_BITWISE_OR, value_ptr);
}

int jsval_bitwise_xor(jsval_region_t *region, jsval_t left, jsval_t right,
		jsval_t *value_ptr)
{
	return jsval_bitwise(region, left, right, JSVAL_BITWISE_XOR, value_ptr);
}

int jsval_shift_left(jsval_region_t *region, jsval_t left, jsval_t right,
		jsval_t *value_ptr)
{
	return jsval_shift(region, left, right, JSVAL_SHIFT_LEFT, value_ptr);
}

int jsval_shift_right(jsval_region_t *region, jsval_t left, jsval_t right,
		jsval_t *value_ptr)
{
	return jsval_shift(region, left, right, JSVAL_SHIFT_RIGHT, value_ptr);
}

int jsval_shift_right_unsigned(jsval_region_t *region, jsval_t left,
		jsval_t right, jsval_t *value_ptr)
{
	return jsval_shift(region, left, right, JSVAL_SHIFT_RIGHT_UNSIGNED,
			value_ptr);
}

int jsval_multiply(jsval_region_t *region, jsval_t left, jsval_t right,
		jsval_t *value_ptr)
{
	double left_number;
	double right_number;

	if (value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_to_number(region, left, &left_number) < 0
			|| jsval_to_number(region, right, &right_number) < 0) {
		return -1;
	}

	*value_ptr = jsval_number(left_number * right_number);
	return 0;
}

int jsval_divide(jsval_region_t *region, jsval_t left, jsval_t right,
		jsval_t *value_ptr)
{
	double left_number;
	double right_number;

	if (value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_to_number(region, left, &left_number) < 0
			|| jsval_to_number(region, right, &right_number) < 0) {
		return -1;
	}

	*value_ptr = jsval_number(left_number / right_number);
	return 0;
}

int jsval_remainder(jsval_region_t *region, jsval_t left, jsval_t right,
		jsval_t *value_ptr)
{
	double left_number;
	double right_number;

	if (value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_to_number(region, left, &left_number) < 0
			|| jsval_to_number(region, right, &right_number) < 0) {
		return -1;
	}

	*value_ptr = jsval_number(jsnum_remainder(left_number, right_number));
	return 0;
}

int jsval_method_string_to_lower_case(jsval_region_t *region, jsval_t this_value,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	return jsval_method_string_unary_bridge(region, this_value,
			JSVAL_METHOD_CASE_EXPANSION_MAX,
			jsmethod_string_to_lower_case, value_ptr, error);
}

int jsval_method_string_to_upper_case(jsval_region_t *region, jsval_t this_value,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	return jsval_method_string_unary_bridge(region, this_value,
			JSVAL_METHOD_CASE_EXPANSION_MAX,
			jsmethod_string_to_upper_case, value_ptr, error);
}

int jsval_method_string_to_locale_lower_case(jsval_region_t *region,
		jsval_t this_value, int have_locale, jsval_t locale_value,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	return jsval_method_string_locale_bridge(region, this_value, have_locale,
			locale_value, JSVAL_METHOD_CASE_EXPANSION_MAX,
			jsmethod_string_to_locale_lower_case, value_ptr, error);
}

int jsval_method_string_to_locale_upper_case(jsval_region_t *region,
		jsval_t this_value, int have_locale, jsval_t locale_value,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	return jsval_method_string_locale_bridge(region, this_value, have_locale,
			locale_value, JSVAL_METHOD_CASE_EXPANSION_MAX,
			jsmethod_string_to_locale_upper_case, value_ptr, error);
}

int jsval_method_string_to_well_formed(jsval_region_t *region,
		jsval_t this_value, jsval_t *value_ptr, jsmethod_error_t *error)
{
	return jsval_method_string_unary_bridge(region, this_value, 1,
			jsmethod_string_to_well_formed, value_ptr, error);
}

int jsval_method_string_is_well_formed(jsval_region_t *region,
		jsval_t this_value, jsval_t *value_ptr, jsmethod_error_t *error)
{
	jsmethod_value_t method_value;
	int is_well_formed;
	size_t storage_cap;

	if (region == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_value_utf16_len(region, this_value, &storage_cap) < 0) {
		return -1;
	}
	if (storage_cap == 0) {
		storage_cap = 1;
	}

	{
		uint16_t storage[storage_cap];

		if (jsval_method_value_from_jsval(region, this_value, storage,
				storage_cap, &method_value) < 0) {
			return -1;
		}
		if (jsmethod_string_is_well_formed(&is_well_formed, method_value,
				storage, storage_cap, error) < 0) {
			return -1;
		}
	}

	*value_ptr = jsval_bool(is_well_formed);
	return 0;
}

int jsval_method_string_trim(jsval_region_t *region, jsval_t this_value,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	return jsval_method_string_unary_bridge(region, this_value, 1,
			jsmethod_string_trim, value_ptr, error);
}

int jsval_method_string_trim_start(jsval_region_t *region, jsval_t this_value,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	return jsval_method_string_unary_bridge(region, this_value, 1,
			jsmethod_string_trim_start, value_ptr, error);
}

int jsval_method_string_trim_end(jsval_region_t *region, jsval_t this_value,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	return jsval_method_string_unary_bridge(region, this_value, 1,
			jsmethod_string_trim_end, value_ptr, error);
}

int jsval_method_string_repeat(jsval_region_t *region, jsval_t this_value,
		int have_count, jsval_t count_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	return jsval_method_string_repeat_bridge(region, this_value, have_count,
			count_value, jsmethod_string_repeat, value_ptr, error);
}

int
jsval_method_string_char_at(jsval_region_t *region, jsval_t this_value,
		int have_position, jsval_t position_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	return jsval_method_string_position_bridge(region, this_value,
			have_position, position_value, jsmethod_string_char_at,
			value_ptr, error);
}

int
jsval_method_string_at(jsval_region_t *region, jsval_t this_value,
		int have_position, jsval_t position_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	return jsval_method_string_optional_bridge(region, this_value,
			have_position, position_value, jsmethod_string_at,
			value_ptr, error);
}

int
jsval_method_string_char_code_at(jsval_region_t *region,
		jsval_t this_value, int have_position, jsval_t position_value,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	return jsval_method_string_double_bridge(region, this_value,
			have_position, position_value, jsmethod_string_char_code_at,
			value_ptr, error);
}

int
jsval_method_string_code_point_at(jsval_region_t *region,
		jsval_t this_value, int have_position, jsval_t position_value,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	return jsval_method_string_optional_double_bridge(region, this_value,
			have_position, position_value, jsmethod_string_code_point_at,
			value_ptr, error);
}

int
jsval_method_string_slice(jsval_region_t *region, jsval_t this_value,
		int have_start, jsval_t start_value,
		int have_end, jsval_t end_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	return jsval_method_string_range_bridge(region, this_value,
			have_start, start_value, have_end, end_value,
			jsmethod_string_slice, value_ptr, error);
}

int
jsval_method_string_substring(jsval_region_t *region, jsval_t this_value,
		int have_start, jsval_t start_value,
		int have_end, jsval_t end_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	return jsval_method_string_range_bridge(region, this_value,
			have_start, start_value, have_end, end_value,
			jsmethod_string_substring, value_ptr, error);
}

int
jsval_method_string_index_of(jsval_region_t *region, jsval_t this_value,
		jsval_t search_value, int have_position, jsval_t position_value,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	return jsval_method_string_index_bridge(region, this_value, search_value,
			have_position, position_value, jsmethod_string_index_of,
			value_ptr, error);
}

int
jsval_method_string_last_index_of(jsval_region_t *region,
		jsval_t this_value, jsval_t search_value, int have_position,
		jsval_t position_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	return jsval_method_string_index_bridge(region, this_value, search_value,
			have_position, position_value, jsmethod_string_last_index_of,
			value_ptr, error);
}

int
jsval_method_string_includes(jsval_region_t *region, jsval_t this_value,
		jsval_t search_value, int have_position, jsval_t position_value,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	return jsval_method_string_bool_bridge(region, this_value, search_value,
			have_position, position_value, jsmethod_string_includes,
			value_ptr, error);
}

int
jsval_method_string_starts_with(jsval_region_t *region,
		jsval_t this_value, jsval_t search_value, int have_position,
		jsval_t position_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	return jsval_method_string_bool_bridge(region, this_value, search_value,
			have_position, position_value, jsmethod_string_starts_with,
			value_ptr, error);
}

int
jsval_method_string_ends_with(jsval_region_t *region, jsval_t this_value,
		jsval_t search_value, int have_end_position,
		jsval_t end_position_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	return jsval_method_string_bool_bridge(region, this_value, search_value,
			have_end_position, end_position_value,
			jsmethod_string_ends_with, value_ptr, error);
}

int jsval_promote_object_shallow_measure(jsval_region_t *region, jsval_t object,
		size_t prop_cap, size_t *bytes_ptr)
{
	size_t start_used;
	size_t used;
	size_t len;

	if (!jsval_region_valid(region) || bytes_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (object.kind != JSVAL_KIND_OBJECT) {
		errno = EINVAL;
		return -1;
	}

	len = jsval_object_size(region, object);
	if (len > prop_cap) {
		errno = ENOBUFS;
		return -1;
	}

	if (object.repr == JSVAL_REPR_NATIVE) {
		jsval_native_object_t *native = jsval_native_object(region, object);

		if (native == NULL) {
			errno = EINVAL;
			return -1;
		}
		if (prop_cap > native->cap) {
			errno = ENOBUFS;
			return -1;
		}

		*bytes_ptr = 0;
		return 0;
	}
	if (object.repr != JSVAL_REPR_JSON) {
		errno = EINVAL;
		return -1;
	}

	if (prop_cap > (SIZE_MAX - sizeof(jsval_native_object_t)) /
			sizeof(jsval_native_prop_t)) {
		errno = EOVERFLOW;
		return -1;
	}

	used = region->pages->used_len;
	start_used = used;
	if (jsval_region_measure_reserve(region, &used,
			sizeof(jsval_native_object_t) + prop_cap * sizeof(jsval_native_prop_t),
			JSVAL_ALIGN) < 0) {
		return -1;
	}

	{
		int cursor;
		unsigned int i;
		jsval_json_doc_t *doc = jsval_json_doc(region, object);
		jsmntok_t *tokens = jsval_json_doc_tokens(region, doc);

		cursor = (int)object.as.index + 1;
		for (i = 0; i < (unsigned int)tokens[object.as.index].size; i++) {
			int key_index = cursor;
			int value_index = jsval_json_next(region, doc, key_index);
			size_t key_utf16_len;

			if (value_index < 0) {
				errno = EINVAL;
				return -1;
			}
			if (jsval_json_string_copy_utf16(region, doc, (uint32_t)key_index, NULL,
					0, &key_utf16_len) < 0) {
				return -1;
			}
			if (jsval_string_measure_utf16(region, &used, key_utf16_len) < 0) {
				return -1;
			}

			cursor = jsval_json_next(region, doc, value_index);
			if (cursor < 0) {
				errno = EINVAL;
				return -1;
			}
		}
	}

	*bytes_ptr = used - start_used;
	return 0;
}

int jsval_promote_object_shallow(jsval_region_t *region, jsval_t object,
		size_t prop_cap, jsval_t *value_ptr)
{
	size_t len;

	if (!jsval_region_valid(region) || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (object.kind != JSVAL_KIND_OBJECT) {
		errno = EINVAL;
		return -1;
	}

	len = jsval_object_size(region, object);
	if (len > prop_cap) {
		errno = ENOBUFS;
		return -1;
	}

	if (object.repr == JSVAL_REPR_NATIVE) {
		jsval_native_object_t *native = jsval_native_object(region, object);

		if (native == NULL) {
			errno = EINVAL;
			return -1;
		}
		if (prop_cap > native->cap) {
			errno = ENOBUFS;
			return -1;
		}

		*value_ptr = object;
		return 0;
	}
	if (object.repr != JSVAL_REPR_JSON) {
		errno = EINVAL;
		return -1;
	}

	{
		size_t needed;
		int cursor;
		unsigned int i;
		jsval_t out;
		jsval_json_doc_t *doc = jsval_json_doc(region, object);
		jsmntok_t *tokens = jsval_json_doc_tokens(region, doc);

		if (jsval_promote_object_shallow_measure(region, object, prop_cap,
				&needed) < 0) {
			return -1;
		}
		(void)needed;
		if (jsval_object_new(region, prop_cap, &out) < 0) {
			return -1;
		}

		cursor = (int)object.as.index + 1;
		for (i = 0; i < (unsigned int)tokens[object.as.index].size; i++) {
			int key_index = cursor;
			int value_index = jsval_json_next(region, doc, key_index);
			size_t key_len;
			jsval_t child;

			if (value_index < 0) {
				errno = EINVAL;
				return -1;
			}
			if (jsval_json_string_copy_utf8_internal(region, doc,
					(uint32_t)key_index, NULL, 0, &key_len) < 0) {
				return -1;
			}

			{
				uint8_t key_buf[key_len ? key_len : 1];

				if (jsval_json_string_copy_utf8_internal(region, doc,
						(uint32_t)key_index, key_buf, key_len, NULL) < 0) {
					return -1;
				}
				child = jsval_json_value(region, object, (uint32_t)value_index);
				if (jsval_object_set_utf8(region, out, key_buf, key_len, child) < 0) {
					return -1;
				}
			}

			cursor = jsval_json_next(region, doc, value_index);
			if (cursor < 0) {
				errno = EINVAL;
				return -1;
			}
		}

		*value_ptr = out;
		return 0;
	}
}

int jsval_promote_object_shallow_in_place(jsval_region_t *region,
		jsval_t *value_ptr, size_t prop_cap)
{
	if (value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}

	return jsval_promote_object_shallow(region, *value_ptr, prop_cap, value_ptr);
}

int jsval_promote_array_shallow_measure(jsval_region_t *region, jsval_t array,
		size_t elem_cap, size_t *bytes_ptr)
{
	size_t len;
	size_t used;
	size_t start_used;

	if (!jsval_region_valid(region) || bytes_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (array.kind != JSVAL_KIND_ARRAY) {
		errno = EINVAL;
		return -1;
	}

	len = jsval_array_length(region, array);
	if (len > elem_cap) {
		errno = ENOBUFS;
		return -1;
	}

	if (array.repr == JSVAL_REPR_NATIVE) {
		jsval_native_array_t *native = jsval_native_array(region, array);

		if (native == NULL) {
			errno = EINVAL;
			return -1;
		}
		if (elem_cap > native->cap) {
			errno = ENOBUFS;
			return -1;
		}

		*bytes_ptr = 0;
		return 0;
	}
	if (array.repr != JSVAL_REPR_JSON) {
		errno = EINVAL;
		return -1;
	}
	if (elem_cap > (SIZE_MAX - sizeof(jsval_native_array_t)) / sizeof(jsval_t)) {
		errno = EOVERFLOW;
		return -1;
	}

	used = region->pages->used_len;
	start_used = used;
	if (jsval_region_measure_reserve(region, &used,
			sizeof(jsval_native_array_t) + elem_cap * sizeof(jsval_t),
			JSVAL_ALIGN) < 0) {
		return -1;
	}
	*bytes_ptr = used - start_used;
	return 0;
}

int jsval_promote_array_shallow(jsval_region_t *region, jsval_t array,
		size_t elem_cap, jsval_t *value_ptr)
{
	size_t len;

	if (!jsval_region_valid(region) || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (array.kind != JSVAL_KIND_ARRAY) {
		errno = EINVAL;
		return -1;
	}

	len = jsval_array_length(region, array);
	if (len > elem_cap) {
		errno = ENOBUFS;
		return -1;
	}

	if (array.repr == JSVAL_REPR_NATIVE) {
		jsval_native_array_t *native = jsval_native_array(region, array);

		if (native == NULL) {
			errno = EINVAL;
			return -1;
		}
		if (elem_cap > native->cap) {
			errno = ENOBUFS;
			return -1;
		}

		*value_ptr = array;
		return 0;
	}
	if (array.repr != JSVAL_REPR_JSON) {
		errno = EINVAL;
		return -1;
	}

	{
		size_t needed;
		size_t i;
		jsval_t out;

		if (jsval_promote_array_shallow_measure(region, array, elem_cap,
				&needed) < 0) {
			return -1;
		}
		(void)needed;
		if (jsval_array_new(region, elem_cap, &out) < 0) {
			return -1;
		}
		for (i = 0; i < len; i++) {
			jsval_t child;

			if (jsval_array_get(region, array, i, &child) < 0) {
				return -1;
			}
			if (jsval_array_set(region, out, i, child) < 0) {
				return -1;
			}
		}

		*value_ptr = out;
		return 0;
	}
}

int jsval_promote_array_shallow_in_place(jsval_region_t *region,
		jsval_t *value_ptr, size_t elem_cap)
{
	if (value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}

	return jsval_promote_array_shallow(region, *value_ptr, elem_cap, value_ptr);
}

int jsval_promote(jsval_region_t *region, jsval_t value, jsval_t *value_ptr)
{
	jsval_json_doc_t *doc;

	if (value.repr != JSVAL_REPR_JSON) {
		*value_ptr = value;
		return 0;
	}

	doc = jsval_json_doc(region, value);
	switch (value.kind) {
	case JSVAL_KIND_UNDEFINED:
		*value_ptr = jsval_undefined();
		return 0;
	case JSVAL_KIND_NULL:
		*value_ptr = jsval_null();
		return 0;
	case JSVAL_KIND_BOOL:
	{
		int boolean;
		if (jsval_json_bool_value(region, doc, value.as.index, &boolean) < 0) {
			return -1;
		}
		*value_ptr = jsval_bool(boolean);
		return 0;
	}
	case JSVAL_KIND_NUMBER:
	{
		double number;
		if (jsval_json_number_value(region, doc, value.as.index, &number) < 0) {
			return -1;
		}
		*value_ptr = jsval_number(number);
		return 0;
	}
	case JSVAL_KIND_STRING:
	{
		size_t utf8_len;
		if (jsval_json_string_copy_utf8_internal(region, doc, value.as.index, NULL, 0, &utf8_len) < 0) {
			return -1;
		}
		{
			uint8_t buf[utf8_len ? utf8_len : 1];
			if (jsval_json_string_copy_utf8_internal(region, doc, value.as.index, buf, utf8_len, NULL) < 0) {
				return -1;
			}
			return jsval_string_new_utf8(region, buf, utf8_len, value_ptr);
		}
	}
	case JSVAL_KIND_ARRAY:
	{
		size_t len = jsval_array_length(region, value);
		size_t i;
		jsval_t array;

		if (jsval_array_new(region, len, &array) < 0) {
			return -1;
		}
		for (i = 0; i < len; i++) {
			jsval_t child;
			jsval_t promoted;

			if (jsval_array_get(region, value, i, &child) < 0) {
				return -1;
			}
			if (jsval_promote(region, child, &promoted) < 0) {
				return -1;
			}
			if (jsval_array_set(region, array, i, promoted) < 0) {
				return -1;
			}
		}

		*value_ptr = array;
		return 0;
	}
	case JSVAL_KIND_OBJECT:
	{
		size_t len = jsval_object_size(region, value);
		size_t i;
		int cursor;
		jsval_t object_out;
		jsmntok_t *tokens = jsval_json_doc_tokens(region, doc);

		if (jsval_object_new(region, len, &object_out) < 0) {
			return -1;
		}

		cursor = (int)value.as.index + 1;
		for (i = 0; i < (size_t)tokens[value.as.index].size; i++) {
			int key_index = cursor;
			int value_index = jsval_json_next(region, doc, key_index);
			size_t key_len;
			jsval_t child;
			jsval_t promoted;

			if (value_index < 0) {
				errno = EINVAL;
				return -1;
			}
			if (jsval_json_string_copy_utf8_internal(region, doc, (uint32_t)key_index, NULL, 0, &key_len) < 0) {
				return -1;
			}

			{
				uint8_t key_buf[key_len ? key_len : 1];
				if (jsval_json_string_copy_utf8_internal(region, doc, (uint32_t)key_index, key_buf, key_len, NULL) < 0) {
					return -1;
				}
				child = jsval_json_value(region, value, (uint32_t)value_index);
				if (jsval_promote(region, child, &promoted) < 0) {
					return -1;
				}
				if (jsval_object_set_utf8(region, object_out, key_buf, key_len, promoted) < 0) {
					return -1;
				}
			}

			cursor = jsval_json_next(region, doc, value_index);
			if (cursor < 0) {
				errno = EINVAL;
				return -1;
			}
		}

		*value_ptr = object_out;
		return 0;
	}
	default:
		errno = ENOTSUP;
		return -1;
	}
}

int jsval_promote_in_place(jsval_region_t *region, jsval_t *value_ptr)
{
	if (value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}

	if (!jsval_is_json_backed(*value_ptr)) {
		return 0;
	}

	return jsval_promote(region, *value_ptr, value_ptr);
}

int jsval_region_promote_root(jsval_region_t *region, jsval_t *value_ptr)
{
	jsval_t root;

	if (jsval_region_root(region, &root) < 0) {
		return -1;
	}
	if (jsval_promote_in_place(region, &root) < 0) {
		return -1;
	}
	if (jsval_region_set_root(region, root) < 0) {
		return -1;
	}
	if (value_ptr != NULL) {
		*value_ptr = root;
	}
	return 0;
}
