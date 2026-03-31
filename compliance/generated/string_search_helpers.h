#ifndef JSMX_COMPLIANCE_STRING_SEARCH_HELPERS_H
#define JSMX_COMPLIANCE_STRING_SEARCH_HELPERS_H

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsmethod.h"

typedef struct generated_string_callback_ctx_s {
	int should_throw;
	const uint16_t *text;
	size_t len;
} generated_string_callback_ctx_t;

typedef struct generated_string_error_ctx_s {
	jsmethod_error_kind_t kind;
	const char *message;
} generated_string_error_ctx_t;

static inline int
generated_string_callback_to_string(void *opaque, jsstr16_t *out,
		jsmethod_error_t *error)
{
	generated_string_callback_ctx_t *ctx =
			(generated_string_callback_ctx_t *)opaque;

	if (ctx->should_throw) {
		error->kind = JSMETHOD_ERROR_ABRUPT;
		error->message = "Test262Error";
		return -1;
	}
	if (jsstr16_set_from_utf16(out, ctx->text, ctx->len) != ctx->len) {
		errno = ENOBUFS;
		return -1;
	}
	return 0;
}

static inline int
generated_string_error_to_string(void *opaque, jsstr16_t *out,
		jsmethod_error_t *error)
{
	generated_string_error_ctx_t *ctx =
			(generated_string_error_ctx_t *)opaque;

	(void)out;
	error->kind = ctx->kind;
	error->message = ctx->message;
	return -1;
}

static inline int
generated_expect_search_index(ssize_t actual, ssize_t expected,
		const char *suite, const char *case_name, const char *label)
{
	if (actual != expected) {
		return generated_test_fail(suite, case_name,
				"%s: expected %zd, got %zd", label, expected, actual);
	}
	return GENERATED_TEST_PASS;
}

static inline int
generated_expect_search_bool(int actual, int expected, const char *suite,
		const char *case_name, const char *label)
{
	if (!!actual != !!expected) {
		return generated_test_fail(suite, case_name,
				"%s: expected %s, got %s", label,
				expected ? "true" : "false",
				actual ? "true" : "false");
	}
	return GENERATED_TEST_PASS;
}

#endif
