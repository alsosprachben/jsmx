#include <stdint.h>

#include "compliance/generated/string_accessor_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/substr/start-to-int-err"

typedef struct generated_count_ctx_s {
	int calls;
} generated_count_ctx_t;

static int
generated_count_to_string(void *opaque, jsstr16_t *out,
		jsmethod_error_t *error)
{
	generated_count_ctx_t *ctx = (generated_count_ctx_t *)opaque;

	(void)error;
	ctx->calls += 1;
	out->len = 0;
	return 0;
}

int
main(void)
{
	generated_string_callback_ctx_t throw_ctx = {1, NULL, 0};
	generated_count_ctx_t length_ctx = {0};
	uint16_t buf[4];
	jsmethod_error_t error;
	jsstr16_t out;

	jsstr16_init_from_buf(&out, (char *)buf, sizeof(buf));

	/*
	 * Generated from test262:
	 * test/annexB/built-ins/String/prototype/substr/start-to-int-err.js
	 *
	 * This is an idiomatic slow-path translation. The upstream start and
	 * length objects are lowered as explicit coercion callbacks so the whole
	 * abrupt-completion ordering contract stays visible outside jsmethod.
	 */
	GENERATED_TEST_ASSERT(jsmethod_string_substr(&out,
			jsmethod_value_string_utf8((const uint8_t *)"", 0), 1,
			jsmethod_value_coercible(&throw_ctx,
				generated_string_callback_to_string), 1,
			jsmethod_value_coercible(&length_ctx, generated_count_to_string),
			&error) == -1, SUITE, CASE_NAME,
			"expected abrupt start coercion to fail");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_ABRUPT,
			SUITE, CASE_NAME, "expected ABRUPT start coercion, got %d",
			(int)error.kind);
	GENERATED_TEST_ASSERT(length_ctx.calls == 0, SUITE, CASE_NAME,
			"expected length coercion to stay unevaluated, got %d calls",
			length_ctx.calls);

	GENERATED_TEST_ASSERT(jsmethod_string_substr(&out,
			jsmethod_value_string_utf8((const uint8_t *)"", 0), 1,
			jsmethod_value_symbol(), 1,
			jsmethod_value_coercible(&length_ctx, generated_count_to_string),
			&error) == -1, SUITE, CASE_NAME,
			"expected Symbol start coercion to fail");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_TYPE,
			SUITE, CASE_NAME, "expected TYPE for Symbol start, got %d",
			(int)error.kind);
	GENERATED_TEST_ASSERT(length_ctx.calls == 0, SUITE, CASE_NAME,
			"expected length coercion to remain unevaluated after Symbol start");
	return generated_test_pass(SUITE, CASE_NAME);
}
