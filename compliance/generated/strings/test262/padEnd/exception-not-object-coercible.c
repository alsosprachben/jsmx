#include <stdint.h>

#include "compliance/generated/string_pad_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/padEnd/exception-not-object-coercible"

int
main(void)
{
	generated_string_error_ctx_t throw_ctx = {
		JSMETHOD_ERROR_ABRUPT, "Test262Error"
	};
	uint16_t storage[16];
	jsmethod_error_t error;
	jsstr16_t out;

	jsstr16_init_from_buf(&out, (char *)storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsmethod_string_pad_end(&out, jsmethod_value_null(), 0,
			jsmethod_value_undefined(), 0, jsmethod_value_undefined(),
			&error) == -1, SUITE, CASE_NAME,
			"expected null receiver to fail");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_TYPE,
			SUITE, CASE_NAME, "expected TYPE for null receiver, got %d",
			(int)error.kind);

	GENERATED_TEST_ASSERT(jsmethod_string_pad_end(&out, jsmethod_value_undefined(), 0,
			jsmethod_value_undefined(), 0, jsmethod_value_undefined(),
			&error) == -1, SUITE, CASE_NAME,
			"expected undefined receiver to fail");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_TYPE,
			SUITE, CASE_NAME,
			"expected TYPE for undefined receiver, got %d",
			(int)error.kind);

	GENERATED_TEST_ASSERT(jsmethod_string_pad_end(&out,
			jsmethod_value_coercible(&throw_ctx,
				generated_string_error_to_string), 0,
			jsmethod_value_undefined(), 0, jsmethod_value_undefined(),
			&error) == -1, SUITE, CASE_NAME,
			"expected abrupt receiver coercion to fail");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_ABRUPT,
			SUITE, CASE_NAME, "expected ABRUPT receiver coercion, got %d",
			(int)error.kind);
	return generated_test_pass(SUITE, CASE_NAME);
}
