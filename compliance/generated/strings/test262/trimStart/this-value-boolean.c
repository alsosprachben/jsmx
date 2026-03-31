#include <stdint.h>

#include "compliance/generated/string_trim_repeat_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/trimStart/this-value-boolean"

int
main(void)
{
	static const uint16_t true_expected[] = {'t', 'r', 'u', 'e'};
	static const uint16_t false_expected[] = {'f', 'a', 'l', 's', 'e'};
	uint16_t buf[8];
	jsmethod_error_t error;
	jsstr16_t out;

	jsstr16_init_from_buf(&out, (char *)buf, sizeof(buf));
	GENERATED_TEST_ASSERT(jsmethod_string_trim_start(&out,
			jsmethod_value_bool(1), &error) == 0, SUITE, CASE_NAME,
			"failed trimStart(true)");
	GENERATED_TEST_ASSERT(generated_expect_accessor_string(&out, true_expected,
			GENERATED_LEN(true_expected), SUITE, CASE_NAME,
			"trimStart(true)") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected trimStart(true)");
	GENERATED_TEST_ASSERT(jsmethod_string_trim_start(&out,
			jsmethod_value_bool(0), &error) == 0, SUITE, CASE_NAME,
			"failed trimStart(false)");
	GENERATED_TEST_ASSERT(generated_expect_accessor_string(&out, false_expected,
			GENERATED_LEN(false_expected), SUITE, CASE_NAME,
			"trimStart(false)") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected trimStart(false)");
	return generated_test_pass(SUITE, CASE_NAME);
}
