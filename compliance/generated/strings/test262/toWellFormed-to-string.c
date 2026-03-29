#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsmethod.h"

#define SUITE "strings"
#define CASE_NAME "test262/toWellFormed-to-string"

typedef struct throw_ctx_s {
	int should_throw;
} throw_ctx_t;

static int
receiver_to_string(void *opaque, jsstr16_t *out, jsmethod_error_t *error)
{
	throw_ctx_t *ctx = (throw_ctx_t *)opaque;

	(void)out;
	if (ctx->should_throw) {
		error->kind = JSMETHOD_ERROR_ABRUPT;
		error->message = "Test262Error";
		return -1;
	}
	return 0;
}

int
main(void)
{
	uint16_t storage[16];
	jsstr16_t out;
	jsmethod_error_t error;
	throw_ctx_t ctx = {1};

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/toWellFormed/to-string.js
	 *
	 * This is an idiomatic slow-path translation. The receiver object's
	 * user-defined `toString` hook is modeled explicitly through a translator
	 * callback instead of being flattened into `jsmx`.
	 */
	jsstr16_init_from_buf(&out, (const char *)storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsmethod_string_to_well_formed(&out,
			jsmethod_value_coercible(&ctx, receiver_to_string),
			&error) == -1, SUITE, CASE_NAME,
			"expected receiver coercion to surface the callback throw");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_ABRUPT,
			SUITE, CASE_NAME,
			"expected abrupt receiver coercion, got %d", (int)error.kind);
	GENERATED_TEST_ASSERT(error.message != NULL,
			SUITE, CASE_NAME,
			"expected abrupt receiver coercion message");

	return generated_test_pass(SUITE, CASE_NAME);
}
