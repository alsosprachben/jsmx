#include "compliance/generated/regex_core_helpers.h"

#define SUITE "regex"
#define CASE_NAME "test262/source/value"

int
main(void)
{
	static const char *const matches[] = {
		"abbc",
		"abbbc",
		"abbbbc",
		"xabbc",
		"xabbbc",
		"xabbbbc"
	};
	static const char *const misses[] = {
		"ac",
		"abc",
		"abbcx",
		"bbc",
		"abb",
		"abbbbbc"
	};
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t regex;
	jsval_t clone_regex;
	jsval_t source_value;
	jsval_t input_value;
	jsmethod_error_t error;
	size_t i;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(generated_regexp_new_utf8(&region, "ab{2,4}c$", NULL,
			&regex, &error) == 0, SUITE, CASE_NAME, "regex build failed");
	GENERATED_TEST_ASSERT(generated_expect_regexp_source(&region, regex,
			"ab{2,4}c$", SUITE, CASE_NAME, "source") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "source mismatch");
	GENERATED_TEST_ASSERT(jsval_regexp_source(&region, regex, &source_value) == 0,
			SUITE, CASE_NAME, "source lookup failed");
	GENERATED_TEST_ASSERT(jsval_regexp_new(&region, source_value, 0,
			jsval_undefined(), &clone_regex, &error) == 0,
			SUITE, CASE_NAME, "clone regex failed");

	for (i = 0; i < sizeof(matches) / sizeof(matches[0]); i++) {
		GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
				(const uint8_t *)matches[i], strlen(matches[i]), &input_value) == 0,
				SUITE, CASE_NAME, "positive input build failed");
		GENERATED_TEST_ASSERT(generated_expect_regexp_test(&region, clone_regex,
				input_value, 1, SUITE, CASE_NAME, matches[i])
				== GENERATED_TEST_PASS,
				SUITE, CASE_NAME, "positive match mismatch");
	}
	for (i = 0; i < sizeof(misses) / sizeof(misses[0]); i++) {
		GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
				(const uint8_t *)misses[i], strlen(misses[i]), &input_value) == 0,
				SUITE, CASE_NAME, "negative input build failed");
		GENERATED_TEST_ASSERT(generated_expect_regexp_test(&region, clone_regex,
				input_value, 0, SUITE, CASE_NAME, misses[i])
				== GENERATED_TEST_PASS,
				SUITE, CASE_NAME, "negative match mismatch");
	}
	return generated_test_pass(SUITE, CASE_NAME);
}
