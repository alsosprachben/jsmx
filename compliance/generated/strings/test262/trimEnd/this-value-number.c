#include <stdint.h>

#include "compliance/generated/string_trim_repeat_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/trimEnd/this-value-number"

static int
expect_trim_end_number(double value, const uint16_t *expected,
		size_t expected_len, const char *label)
{
	uint16_t buf[16];
	jsmethod_error_t error;
	jsstr16_t out;

	jsstr16_init_from_buf(&out, (char *)buf, sizeof(buf));
	if (jsmethod_string_trim_end(&out, jsmethod_value_number(value), &error) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: trimEnd failed", label);
	}
	return generated_expect_accessor_string(&out, expected, expected_len,
			SUITE, CASE_NAME, label);
}

int
main(void)
{
	static const uint16_t nan_expected[] = {'N', 'a', 'N'};
	static const uint16_t inf_expected[] = {'I', 'n', 'f', 'i', 'n', 'i',
		't', 'y'};
	static const uint16_t zero_expected[] = {'0'};
	static const uint16_t one_expected[] = {'1'};
	static const uint16_t neg_one_expected[] = {'-', '1'};

	GENERATED_TEST_ASSERT(expect_trim_end_number(NAN, nan_expected,
			GENERATED_LEN(nan_expected), "trimEnd(NaN)") ==
			GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected trimEnd(NaN)");
	GENERATED_TEST_ASSERT(expect_trim_end_number(INFINITY, inf_expected,
			GENERATED_LEN(inf_expected), "trimEnd(Infinity)") ==
			GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected trimEnd(Infinity)");
	GENERATED_TEST_ASSERT(expect_trim_end_number(-0.0, zero_expected,
			GENERATED_LEN(zero_expected), "trimEnd(-0)") ==
			GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected trimEnd(-0)");
	GENERATED_TEST_ASSERT(expect_trim_end_number(1.0, one_expected,
			GENERATED_LEN(one_expected), "trimEnd(1)") ==
			GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected trimEnd(1)");
	GENERATED_TEST_ASSERT(expect_trim_end_number(-1.0, neg_one_expected,
			GENERATED_LEN(neg_one_expected), "trimEnd(-1)") ==
			GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected trimEnd(-1)");
	return generated_test_pass(SUITE, CASE_NAME);
}
