#include <stdint.h>

#include "compliance/generated/string_concat_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/concat/S15.5.4.6_A1_T5"

int
main(void)
{
	uint16_t storage[16];
	jsmethod_value_t args[1];
	jsmethod_error_t error;
	jsstr16_t out;

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/concat/S15.5.4.6_A1_T5.js
	 */

	args[0] = jsmethod_value_null();
	GENERATED_TEST_ASSERT(generated_measure_and_concat(&out, storage,
			GENERATED_LEN(storage),
			jsmethod_value_string_utf8((const uint8_t *)"lego", 4),
			1, args, &error) == 0, SUITE, CASE_NAME,
			"failed concat(null)");
	GENERATED_TEST_ASSERT(generated_expect_utf8_string(&out, "legonull",
			SUITE, CASE_NAME, "\"lego\".concat(null)")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected concat(null) result");
	return generated_test_pass(SUITE, CASE_NAME);
}
