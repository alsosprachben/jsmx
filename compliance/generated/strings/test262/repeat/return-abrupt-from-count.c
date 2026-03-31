#include <stdint.h>

#include "compliance/generated/string_trim_repeat_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/repeat/return-abrupt-from-count"

int
main(void)
{
	generated_string_callback_ctx_t ctx = {1, NULL, 0};
	uint16_t buf[4];
	jsmethod_error_t error;
	jsstr16_t out;

	jsstr16_init_from_buf(&out, (char *)buf, sizeof(buf));
	GENERATED_TEST_ASSERT(jsmethod_string_repeat(&out,
			jsmethod_value_string_utf8((const uint8_t *)"", 0), 1,
			jsmethod_value_coercible(&ctx, generated_string_callback_to_string),
			&error) == -1, SUITE, CASE_NAME,
			"expected abrupt count coercion to fail");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_ABRUPT,
			SUITE, CASE_NAME, "expected ABRUPT count coercion, got %d",
			(int)error.kind);
	return generated_test_pass(SUITE, CASE_NAME);
}
