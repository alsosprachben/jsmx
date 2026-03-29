#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsmethod.h"

#define SUITE "strings"
#define CASE_NAME "test262/this-value-tostring-throws-toprimitive"

typedef struct primitive_fail_ctx_s {
	int should_fail;
} primitive_fail_ctx_t;

static int
receiver_to_string(void *opaque, jsstr16_t *out, jsmethod_error_t *error)
{
	primitive_fail_ctx_t *ctx = (primitive_fail_ctx_t *)opaque;

	(void)out;
	if (ctx->should_fail) {
		error->kind = JSMETHOD_ERROR_TYPE;
		error->message = "cannot convert object to primitive string";
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
	primitive_fail_ctx_t ctx = {1};

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/toLowerCase/this-value-tostring-throws-toprimitive.js
	 *
	 * This is an idiomatic slow-path translation. The dynamic receiver
	 * `ToPrimitive` failure is modeled through an explicit translator callback
	 * rather than being flattened into `jsmx`.
	 */
	jsstr16_init_from_buf(&out, (const char *)storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsmethod_string_to_lower_case(&out,
			jsmethod_value_coercible(&ctx, receiver_to_string),
			&error) == -1, SUITE, CASE_NAME,
			"expected receiver ToPrimitive failure to reject lowercasing");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_TYPE,
			SUITE, CASE_NAME,
			"expected TypeError-style failure, got %d", (int)error.kind);

	return generated_test_pass(SUITE, CASE_NAME);
}
