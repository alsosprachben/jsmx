#include <stdint.h>

#include "compliance/generated/string_accessor_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/slice/this-value-tostring-throws-toprimitive"

int
main(void)
{
	generated_string_error_ctx_t ctx = {JSMETHOD_ERROR_TYPE, "TypeError"};
	uint16_t buf[4];
	jsmethod_error_t error;
	jsstr16_t out;

	jsstr16_init_from_buf(&out, (char *)buf, sizeof(buf));
	GENERATED_TEST_ASSERT(jsmethod_string_slice(&out,
			jsmethod_value_coercible(&ctx, generated_string_error_to_string), 0,
			jsmethod_value_undefined(), 0, jsmethod_value_undefined(),
			&error) == -1, SUITE, CASE_NAME,
			"expected receiver ToPrimitive failure");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_TYPE, SUITE, CASE_NAME,
			"expected TYPE receiver failure, got %d", (int)error.kind);
	return generated_test_pass(SUITE, CASE_NAME);
}
