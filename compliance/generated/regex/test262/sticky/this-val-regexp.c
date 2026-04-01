#include "compliance/generated/regex_core_helpers.h"

#define SUITE "regex"
#define CASE_NAME "test262/sticky/this-val-regexp"

#define CHECK_STICKY(flag_text, expected_value, label) \
	do { \
		GENERATED_TEST_ASSERT(generated_regexp_new_utf8(&region, ".", flag_text, \
				&regex, &error) == 0, SUITE, CASE_NAME, label " build failed"); \
		GENERATED_TEST_ASSERT(generated_expect_regexp_boolean(&region, regex, \
				jsval_regexp_sticky, expected_value, SUITE, CASE_NAME, label) \
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
	CHECK_STICKY(NULL, 0, "sticky none");
	CHECK_STICKY("i", 0, "sticky i");
	CHECK_STICKY("g", 0, "sticky g");
	CHECK_STICKY("gi", 0, "sticky gi");
	CHECK_STICKY("y", 1, "sticky y");
	CHECK_STICKY("iy", 1, "sticky iy");
	CHECK_STICKY("yg", 1, "sticky yg");
	CHECK_STICKY("iyg", 1, "sticky iyg");
	return generated_test_pass(SUITE, CASE_NAME);
}

#undef CHECK_STICKY
