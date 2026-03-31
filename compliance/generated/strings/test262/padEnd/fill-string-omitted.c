#include <stdint.h>

#include "compliance/generated/string_pad_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/padEnd/fill-string-omitted"

int
main(void)
{
	uint16_t storage[8];
	jsmethod_error_t error;
	jsstr16_t out;

	GENERATED_TEST_ASSERT(generated_measure_and_pad(&out, storage,
			GENERATED_LEN(storage),
			jsmethod_value_string_utf8((const uint8_t *)"abc", 3), 1,
			jsmethod_value_number(5.0), 0, jsmethod_value_undefined(),
			jsmethod_string_pad_end_measure, jsmethod_string_pad_end, &error) == 0,
			SUITE, CASE_NAME, "failed omitted fillString");
	GENERATED_TEST_ASSERT(generated_expect_utf8_string(&out,
			"abc  ", SUITE, CASE_NAME,
			"omitted fillString") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected omitted fillString result");

	GENERATED_TEST_ASSERT(generated_measure_and_pad(&out, storage,
			GENERATED_LEN(storage),
			jsmethod_value_string_utf8((const uint8_t *)"abc", 3), 1,
			jsmethod_value_number(5.0), 1, jsmethod_value_undefined(),
			jsmethod_string_pad_end_measure, jsmethod_string_pad_end, &error) == 0,
			SUITE, CASE_NAME, "failed explicit undefined fillString");
	GENERATED_TEST_ASSERT(generated_expect_utf8_string(&out,
			"abc  ", SUITE, CASE_NAME,
			"undefined fillString") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected undefined fillString result");
	return generated_test_pass(SUITE, CASE_NAME);
}
