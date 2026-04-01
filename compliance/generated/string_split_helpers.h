#ifndef JSMX_COMPLIANCE_STRING_SPLIT_HELPERS_H
#define JSMX_COMPLIANCE_STRING_SPLIT_HELPERS_H

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "compliance/generated/string_search_helpers.h"
#include "jsval.h"

#define GENERATED_LEN(a) (sizeof(a) / sizeof((a)[0]))

typedef struct generated_split_collect_ctx_s {
	size_t count;
	struct {
		uint16_t codeunits[64];
		size_t len;
	} segments[16];
} generated_split_collect_ctx_t;

static inline int
generated_split_collect_emit(void *opaque, const uint16_t *segment,
		size_t segment_len)
{
	generated_split_collect_ctx_t *ctx =
			(generated_split_collect_ctx_t *)opaque;

	if (ctx == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (ctx->count >= (sizeof(ctx->segments) / sizeof(ctx->segments[0]))
			|| segment_len > (sizeof(ctx->segments[0].codeunits) /
			sizeof(ctx->segments[0].codeunits[0]))) {
		errno = ENOBUFS;
		return -1;
	}
	if (segment_len > 0) {
		memcpy(ctx->segments[ctx->count].codeunits, segment,
				segment_len * sizeof(segment[0]));
	}
	ctx->segments[ctx->count].len = segment_len;
	ctx->count++;
	return 0;
}

static inline int
generated_expect_split_segment_utf8(generated_split_collect_ctx_t *ctx,
		size_t index, const char *expected, const char *suite,
		const char *case_name, const char *label)
{
	size_t i;
	size_t expected_len = strlen(expected);

	if (ctx == NULL || index >= ctx->count) {
		return generated_test_fail(suite, case_name,
				"%s[%zu]: missing collected split segment", label, index);
	}
	if (ctx->segments[index].len != expected_len) {
		return generated_test_fail(suite, case_name,
				"%s[%zu]: expected len %zu, got %zu", label, index,
				expected_len, ctx->segments[index].len);
	}
	for (i = 0; i < expected_len; i++) {
		if (ctx->segments[index].codeunits[i] != (uint8_t)expected[i]) {
			return generated_test_fail(suite, case_name,
					"%s[%zu]: byte %zu mismatch", label, index, i);
		}
	}
	return GENERATED_TEST_PASS;
}

static inline int
generated_expect_split_array_strings(jsval_region_t *region, jsval_t array,
		const char *const *expected, size_t expected_len,
		const char *suite, const char *case_name, const char *label)
{
	size_t i;

	if (array.kind != JSVAL_KIND_ARRAY) {
		return generated_test_fail(suite, case_name,
				"%s: expected array result", label);
	}
	if (jsval_array_length(region, array) != expected_len) {
		return generated_test_fail(suite, case_name,
				"%s: expected length %zu, got %zu", label,
				expected_len, jsval_array_length(region, array));
	}
	for (i = 0; i < expected_len; i++) {
		jsval_t value;

		if (jsval_array_get(region, array, i, &value) < 0) {
			return generated_test_fail(suite, case_name,
					"%s[%zu]: array get failed: %s", label, i,
					strerror(errno));
		}
		if (expected[i] == NULL) {
			if (value.kind != JSVAL_KIND_UNDEFINED) {
				return generated_test_fail(suite, case_name,
						"%s[%zu]: expected undefined element", label, i);
			}
			continue;
		}
		if (value.kind != JSVAL_KIND_STRING) {
			return generated_test_fail(suite, case_name,
					"%s[%zu]: expected string element", label, i);
		}
		{
			size_t expected_item_len = strlen(expected[i]);
			size_t actual_len = 0;
			uint8_t buf[expected_item_len ? expected_item_len : 1];

			if (jsval_string_copy_utf8(region, value, NULL, 0, &actual_len) < 0) {
				return generated_test_fail(suite, case_name,
						"%s[%zu]: string length failed: %s", label, i,
						strerror(errno));
			}
			if (actual_len != expected_item_len) {
				return generated_test_fail(suite, case_name,
						"%s[%zu]: expected %zu bytes, got %zu", label, i,
						expected_item_len, actual_len);
			}
			if (jsval_string_copy_utf8(region, value, buf, actual_len, NULL) < 0) {
				return generated_test_fail(suite, case_name,
						"%s[%zu]: string copy failed: %s", label, i,
						strerror(errno));
			}
			if (memcmp(buf, expected[i], expected_item_len) != 0) {
				return generated_test_fail(suite, case_name,
						"%s[%zu]: string bytes did not match expected output",
						label, i);
			}
		}
	}
	return GENERATED_TEST_PASS;
}

#endif
