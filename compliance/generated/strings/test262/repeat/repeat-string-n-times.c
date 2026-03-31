#include <stdint.h>

#include "compliance/generated/string_trim_repeat_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/repeat/repeat-string-n-times"

int
main(void)
{
	static const uint16_t abc[] = {'a', 'b', 'c'};
	static const uint16_t abcabcabc[] = {'a', 'b', 'c', 'a', 'b', 'c',
		'a', 'b', 'c'};
	uint16_t small_buf[16];
	uint16_t large_buf[10000];
	jsmethod_error_t error;
	jsstr16_t out;
	size_t i;

	GENERATED_TEST_ASSERT(generated_measure_and_repeat(&out, small_buf,
			GENERATED_LEN(small_buf),
			jsmethod_value_string_utf8((const uint8_t *)"abc", 3), 1,
			jsmethod_value_number(1.0), &error) == 0,
			SUITE, CASE_NAME, "repeat(1) failed");
	GENERATED_TEST_ASSERT(generated_expect_accessor_string(&out, abc,
			GENERATED_LEN(abc), SUITE, CASE_NAME, "repeat(1)") ==
			GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected repeat(1)");

	GENERATED_TEST_ASSERT(generated_measure_and_repeat(&out, small_buf,
			GENERATED_LEN(small_buf),
			jsmethod_value_string_utf8((const uint8_t *)"abc", 3), 1,
			jsmethod_value_number(3.0), &error) == 0,
			SUITE, CASE_NAME, "repeat(3) failed");
	GENERATED_TEST_ASSERT(generated_expect_accessor_string(&out, abcabcabc,
			GENERATED_LEN(abcabcabc), SUITE, CASE_NAME, "repeat(3)") ==
			GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected repeat(3)");

	GENERATED_TEST_ASSERT(generated_measure_and_repeat(&out, large_buf,
			GENERATED_LEN(large_buf),
			jsmethod_value_string_utf8((const uint8_t *)".", 1), 1,
			jsmethod_value_number(10000.0), &error) == 0,
			SUITE, CASE_NAME, "repeat(10000) failed");
	GENERATED_TEST_ASSERT(jsstr16_get_utf16len(&out) == 10000,
			SUITE, CASE_NAME, "expected length 10000, got %zu",
			jsstr16_get_utf16len(&out));
	for (i = 0; i < 10000; i++) {
		GENERATED_TEST_ASSERT(out.codeunits[i] == '.',
				SUITE, CASE_NAME,
				"expected '.' at index %zu, got 0x%04x", i,
				(unsigned)out.codeunits[i]);
	}
	return generated_test_pass(SUITE, CASE_NAME);
}
