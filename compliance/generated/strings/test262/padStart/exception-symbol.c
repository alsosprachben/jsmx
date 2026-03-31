#include <stdint.h>

#include "compliance/generated/string_pad_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/padStart/exception-symbol"

int
main(void)
{
	uint16_t storage[16];
	jsmethod_error_t error;
	jsstr16_t out;

	jsstr16_init_from_buf(&out, (char *)storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsmethod_string_pad_start(&out, jsmethod_value_symbol(), 1,
			jsmethod_value_number(10.0), 0, jsmethod_value_undefined(),
			&error) == -1, SUITE, CASE_NAME,
			"expected Symbol receiver to fail");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_TYPE,
			SUITE, CASE_NAME, "expected TYPE error, got %d",
			(int)error.kind);
	return generated_test_pass(SUITE, CASE_NAME);
}
