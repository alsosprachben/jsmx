#include <stdint.h>

#include "compliance/generated/string_trim_repeat_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/trimEnd/this-value-not-obj-coercible"

int
main(void)
{
	uint16_t buf[4];
	jsmethod_error_t error;
	jsstr16_t out;

	jsstr16_init_from_buf(&out, (char *)buf, sizeof(buf));
	GENERATED_TEST_ASSERT(jsmethod_string_trim_end(&out,
			jsmethod_value_undefined(), &error) == -1,
			SUITE, CASE_NAME, "expected undefined receiver to fail");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_TYPE, SUITE, CASE_NAME,
			"expected TYPE for undefined receiver, got %d",
			(int)error.kind);
	GENERATED_TEST_ASSERT(jsmethod_string_trim_end(&out,
			jsmethod_value_null(), &error) == -1,
			SUITE, CASE_NAME, "expected null receiver to fail");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_TYPE, SUITE, CASE_NAME,
			"expected TYPE for null receiver, got %d", (int)error.kind);
	return generated_test_pass(SUITE, CASE_NAME);
}
