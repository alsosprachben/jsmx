#include <stdint.h>

#include "compliance/generated/string_replace_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/replace/S15.5.4.11_A1_T7"

int
main(void)
{
	uint16_t storage[32];
	jsmethod_error_t error;
	jsstr16_t out;

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/replace/S15.5.4.11_A1_T7.js
	 */

	GENERATED_TEST_ASSERT(generated_measure_and_replace(&out, storage,
			GENERATED_LEN(storage),
			jsmethod_value_string_utf8((const uint8_t *)"undefined", 9),
			jsmethod_value_string_utf8((const uint8_t *)"e", 1),
			jsmethod_value_undefined(), &error) == 0,
			SUITE, CASE_NAME, "failed primitive string replace(undefined)");
	GENERATED_TEST_ASSERT(generated_expect_utf8_string(&out,
			"undundefinedfined", SUITE, CASE_NAME,
			"String(void 0).replace(\"e\", undefined)") ==
			GENERATED_TEST_PASS,
			SUITE, CASE_NAME,
			"unexpected primitive string replace(undefined) result");
	return generated_test_pass(SUITE, CASE_NAME);
}
