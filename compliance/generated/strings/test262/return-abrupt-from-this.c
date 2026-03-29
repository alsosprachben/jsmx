#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsmethod.h"

#define SUITE "strings"
#define CASE_NAME "test262/return-abrupt-from-this"

typedef struct throw_ctx_s {
	int should_throw;
} throw_ctx_t;

static int
throwing_to_string(void *opaque, jsstr16_t *out, jsmethod_error_t *error)
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
	uint32_t workspace[32];
	jsstr16_t out;
	jsmethod_error_t error;
	throw_ctx_t throw_ctx = {1};

	jsstr16_init_from_buf(&out, (const char *)storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsmethod_string_normalize(&out,
			jsmethod_value_coercible(&throw_ctx, throwing_to_string),
			0, jsmethod_value_undefined(),
			NULL, 0,
			workspace, sizeof(workspace) / sizeof(workspace[0]),
			&error) == -1, SUITE, CASE_NAME,
			"expected abrupt receiver coercion to fail");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_ABRUPT,
			SUITE, CASE_NAME,
			"expected abrupt receiver coercion to surface as ABRUPT, got %d",
			(int)error.kind);

	return generated_test_pass(SUITE, CASE_NAME);
}
