#ifndef JSMX_COMPLIANCE_STRING_REPLACE_CALLBACK_HELPERS_H
#define JSMX_COMPLIANCE_STRING_REPLACE_CALLBACK_HELPERS_H

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "compliance/generated/string_replace_helpers.h"

typedef struct generated_replace_callback_record_s {
	size_t call_count;
	size_t max_calls;
	const char *replacement_text;
	int return_undefined;
	int return_offset_number;
	double offset_delta;
	int should_throw;
	jsmethod_error_kind_t error_kind;
	const char *error_message;
	char input[128];
	int input_set;
	size_t offsets[8];
	size_t capture_counts[8];
	char matches[8][32];
	int capture_defined[8][2];
	char captures[8][2][32];
} generated_replace_callback_record_t;

static inline int
generated_copy_jsval_utf8_value(jsval_region_t *region, jsval_t value, char *buf,
		size_t cap)
{
	size_t len = 0;

	if (region == NULL || buf == NULL || cap == 0) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_string_copy_utf8(region, value, NULL, 0, &len) < 0) {
		return -1;
	}
	if (len + 1 > cap) {
		errno = ENOBUFS;
		return -1;
	}
	if (len > 0 && jsval_string_copy_utf8(region, value, (uint8_t *)buf, len,
			NULL) < 0) {
		return -1;
	}
	buf[len] = '\0';
	return 0;
}

static inline int
generated_record_replace_callback(jsval_region_t *region, void *opaque,
		const jsval_replace_call_t *call, jsval_t *result_ptr,
		jsmethod_error_t *error)
{
	generated_replace_callback_record_t *ctx =
			(generated_replace_callback_record_t *)opaque;
	size_t index;
	size_t capture_limit;

	if (region == NULL || ctx == NULL || call == NULL || result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (ctx->should_throw) {
		ctx->call_count++;
		errno = EINVAL;
		if (error != NULL) {
			error->kind = ctx->error_kind;
			error->message = ctx->error_message;
		}
		return -1;
	}
	index = ctx->call_count++;
	if (index >= ctx->max_calls || index >= 8) {
		errno = EOVERFLOW;
		return -1;
	}
	if (!ctx->input_set) {
		if (generated_copy_jsval_utf8_value(region, call->input, ctx->input,
				sizeof(ctx->input)) < 0) {
			return -1;
		}
		ctx->input_set = 1;
	}
	ctx->offsets[index] = call->offset;
	ctx->capture_counts[index] = call->capture_count;
	if (generated_copy_jsval_utf8_value(region, call->match, ctx->matches[index],
			sizeof(ctx->matches[index])) < 0) {
		return -1;
	}
	capture_limit = call->capture_count < 2 ? call->capture_count : 2;
	for (size_t i = 0; i < capture_limit; i++) {
		if (call->captures[i].kind == JSVAL_KIND_UNDEFINED) {
			ctx->capture_defined[index][i] = 0;
			ctx->captures[index][i][0] = '\0';
		} else {
			ctx->capture_defined[index][i] = 1;
			if (generated_copy_jsval_utf8_value(region, call->captures[i],
					ctx->captures[index][i],
					sizeof(ctx->captures[index][i])) < 0) {
				return -1;
			}
		}
	}
	for (size_t i = capture_limit; i < 2; i++) {
		ctx->capture_defined[index][i] = 0;
		ctx->captures[index][i][0] = '\0';
	}
	if (ctx->return_undefined) {
		*result_ptr = jsval_undefined();
		return 0;
	}
	if (ctx->return_offset_number) {
		*result_ptr = jsval_number((double)call->offset + ctx->offset_delta);
		return 0;
	}
	return jsval_string_new_utf8(region,
			(const uint8_t *)ctx->replacement_text,
			strlen(ctx->replacement_text), result_ptr);
}

#endif
