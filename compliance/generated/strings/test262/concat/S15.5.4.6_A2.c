#include <stdint.h>
#include <string.h>

#include "compliance/generated/string_concat_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/concat/S15.5.4.6_A2"

int
main(void)
{
	static const char expected[] =
		"001234567891011121314150123456789101112131415"
		"01234567891011121314150123456789101112131415"
		"01234567891011121314150123456789101112131415"
		"01234567891011121314150123456789101112131415";
	uint16_t storage[256];
	jsmethod_value_t args[128];
	jsmethod_string_concat_sizes_t sizes;
	jsmethod_error_t error;
	jsstr16_t out;
	size_t i;

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/concat/S15.5.4.6_A2.js
	 */

	for (i = 0; i < 128; i++) {
		args[i] = jsmethod_value_number((double)(i & 0x0f));
	}
	GENERATED_TEST_ASSERT(jsmethod_string_concat_measure(
			jsmethod_value_number(0.0), 128, args, &sizes, &error) == 0,
			SUITE, CASE_NAME, "failed 128-argument concat measure");
	GENERATED_TEST_ASSERT(sizes.result_len == strlen(expected), SUITE, CASE_NAME,
			"expected result_len %zu, got %zu", strlen(expected),
			sizes.result_len);
	GENERATED_TEST_ASSERT(generated_measure_and_concat(&out, storage,
			GENERATED_LEN(storage), jsmethod_value_number(0.0), 128, args,
			&error) == 0, SUITE, CASE_NAME,
			"failed 128-argument concat");
	GENERATED_TEST_ASSERT(generated_expect_utf8_string(&out, expected, SUITE,
			CASE_NAME, "128-argument concat") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected 128-argument concat result");
	return generated_test_pass(SUITE, CASE_NAME);
}
