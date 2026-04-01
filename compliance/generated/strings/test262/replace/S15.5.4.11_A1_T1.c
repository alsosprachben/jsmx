#include <stdint.h>

#include "compliance/generated/string_replace_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/replace/S15.5.4.11_A1_T1"

int
main(void)
{
	uint16_t storage[8];
	jsmethod_error_t error;
	jsstr16_t out;

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/replace/S15.5.4.11_A1_T1.js
	 *
	 * This is an idiomatic slow-path translation. The upstream Object(true)
	 * receiver is lowered outside jsmx to its primitive "true" string value,
	 * then dispatched through the thin jsmethod replace layer.
	 */

	GENERATED_TEST_ASSERT(generated_measure_and_replace(&out, storage,
			GENERATED_LEN(storage),
			jsmethod_value_string_utf8((const uint8_t *)"true", 4),
			jsmethod_value_bool(1), jsmethod_value_number(1.0), &error) == 0,
			SUITE, CASE_NAME, "failed boxed-object replace");
	GENERATED_TEST_ASSERT(generated_expect_utf8_string(&out, "1", SUITE,
			CASE_NAME, "__instance.replace(true, 1)") ==
			GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected boxed-object replace result");
	return generated_test_pass(SUITE, CASE_NAME);
}
