#include <stdint.h>

#include "compliance/generated/string_pad_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/padEnd/normal-operation"

int
main(void)
{
	static const uint16_t surrogate_expected[] = {'a', 'b', 'c', 0xD83D, 0xDCA9, 0xD83D};
	uint16_t storage[16];
	jsmethod_error_t error;
	jsstr16_t out;

	GENERATED_TEST_ASSERT(generated_measure_and_pad(&out, storage,
			GENERATED_LEN(storage),
			jsmethod_value_string_utf8((const uint8_t *)"abc", 3), 1,
			jsmethod_value_number(7.0), 1,
			jsmethod_value_string_utf8((const uint8_t *)"def", 3),
			jsmethod_string_pad_end_measure, jsmethod_string_pad_end, &error) == 0,
			SUITE, CASE_NAME, "failed padEnd(7, 'def')");
	GENERATED_TEST_ASSERT(generated_expect_utf8_string(&out,
			"abcdefd", SUITE, CASE_NAME,
			"padEnd(7, 'def')") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected padEnd(7, 'def')");

	GENERATED_TEST_ASSERT(generated_measure_and_pad(&out, storage,
			GENERATED_LEN(storage),
			jsmethod_value_string_utf8((const uint8_t *)"abc", 3), 1,
			jsmethod_value_number(5.0), 1,
			jsmethod_value_string_utf8((const uint8_t *)"*", 1),
			jsmethod_string_pad_end_measure, jsmethod_string_pad_end, &error) == 0,
			SUITE, CASE_NAME, "failed padEnd(5, '*')");
	GENERATED_TEST_ASSERT(generated_expect_utf8_string(&out,
			"abc**", SUITE, CASE_NAME,
			"padEnd(5, '*')") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected padEnd(5, '*')");

	GENERATED_TEST_ASSERT(generated_measure_and_pad(&out, storage,
			GENERATED_LEN(storage),
			jsmethod_value_string_utf8((const uint8_t *)"abc", 3), 1,
			jsmethod_value_number(6.0), 1,
			jsmethod_value_string_utf16((const uint16_t[]){0xD83D, 0xDCA9}, 2),
			jsmethod_string_pad_end_measure, jsmethod_string_pad_end, &error) == 0,
			SUITE, CASE_NAME, "failed surrogate padEnd");
	GENERATED_TEST_ASSERT(generated_expect_accessor_string(&out,
			surrogate_expected, GENERATED_LEN(surrogate_expected), SUITE,
			CASE_NAME, "surrogate padEnd") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected surrogate padEnd");
	return generated_test_pass(SUITE, CASE_NAME);
}
