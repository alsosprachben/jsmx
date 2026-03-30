#include <stdint.h>

#include "compliance/generated/string_accessor_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/codePointAt/return-abrupt-from-this"

int
main(void)
{
	generated_string_callback_ctx_t ctx = {1, NULL, 0};
	jsmethod_error_t error;
	int has_value;
	double value;

	GENERATED_TEST_ASSERT(jsmethod_string_code_point_at(&has_value, &value,
			jsmethod_value_coercible(&ctx, generated_string_callback_to_string),
			1, jsmethod_value_number(1.0), &error) == -1, SUITE, CASE_NAME,
			"expected abrupt receiver coercion to fail");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_ABRUPT, SUITE, CASE_NAME,
			"expected ABRUPT receiver coercion, got %d", (int)error.kind);
	return generated_test_pass(SUITE, CASE_NAME);
}
