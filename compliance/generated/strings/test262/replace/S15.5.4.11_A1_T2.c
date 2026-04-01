#include <stdint.h>

#include "compliance/generated/string_replace_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/replace/S15.5.4.11_A1_T2"

int
main(void)
{
	uint16_t storage[24];
	jsmethod_error_t error;
	jsstr16_t out;

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/replace/S15.5.4.11_A1_T2.js
	 *
	 * This is an idiomatic slow-path translation. The boxed Boolean receiver is
	 * lowered outside jsmx to its primitive "false" string value before replace
	 * dispatch.
	 */

	GENERATED_TEST_ASSERT(generated_measure_and_replace(&out, storage,
			GENERATED_LEN(storage),
			jsmethod_value_string_utf8((const uint8_t *)"false", 5),
			jsmethod_value_bool(0), jsmethod_value_undefined(), &error) == 0,
			SUITE, CASE_NAME, "failed boxed-Boolean replace");
	GENERATED_TEST_ASSERT(generated_expect_utf8_string(&out, "undefined",
			SUITE, CASE_NAME,
			"__instance.replace(function(){return false;}(), x)") ==
			GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected boxed-Boolean replace result");
	return generated_test_pass(SUITE, CASE_NAME);
}
