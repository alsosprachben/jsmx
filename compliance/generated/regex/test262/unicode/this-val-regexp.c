#include "compliance/generated/regex_core_helpers.h"

#define SUITE "regex"
#define CASE_NAME "test262/unicode/this-val-regexp"

#define CHECK_UNICODE(flag_text, expected_value, label) \
	do { \
		GENERATED_TEST_ASSERT(generated_regexp_new_utf8(&region, ".", flag_text, \
				&regex, &error) == 0, SUITE, CASE_NAME, label " build failed"); \
		GENERATED_TEST_ASSERT(generated_expect_regexp_boolean(&region, regex, \
				jsval_regexp_unicode, expected_value, SUITE, CASE_NAME, label) \
				== GENERATED_TEST_PASS, SUITE, CASE_NAME, label " mismatch"); \
	} while (0)

int
main(void)
{
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t regex;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	CHECK_UNICODE(NULL, 0, "unicode none");
	CHECK_UNICODE("i", 0, "unicode i");
	CHECK_UNICODE("g", 0, "unicode g");
	CHECK_UNICODE("gi", 0, "unicode gi");
	CHECK_UNICODE("u", 1, "unicode u");
	CHECK_UNICODE("iu", 1, "unicode iu");
	CHECK_UNICODE("ug", 1, "unicode ug");
	CHECK_UNICODE("iug", 1, "unicode iug");
	return generated_test_pass(SUITE, CASE_NAME);
}

#undef CHECK_UNICODE
