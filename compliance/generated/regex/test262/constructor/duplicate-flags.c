#include "compliance/generated/regex_core_helpers.h"

#define SUITE "regex"
#define CASE_NAME "test262/constructor/duplicate-flags"

#define EXPECT_DUPLICATE(flag_text, label) \
	do { \
		errno = 0; \
		GENERATED_TEST_ASSERT(generated_regexp_new_utf8(&region, "", flag_text, \
				&regex, &error) < 0, SUITE, CASE_NAME, label " should fail"); \
		GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_SYNTAX, \
				SUITE, CASE_NAME, label " expected syntax error"); \
	} while (0)

int
main(void)
{
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t regex;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(generated_regexp_new_utf8(&region, "", "mig",
			&regex, &error) == 0, SUITE, CASE_NAME, "single g should pass");
	EXPECT_DUPLICATE("migg", "duplicate g");
	GENERATED_TEST_ASSERT(generated_regexp_new_utf8(&region, "", "i",
			&regex, &error) == 0, SUITE, CASE_NAME, "single i should pass");
	EXPECT_DUPLICATE("ii", "duplicate i");
	GENERATED_TEST_ASSERT(generated_regexp_new_utf8(&region, "", "m",
			&regex, &error) == 0, SUITE, CASE_NAME, "single m should pass");
	EXPECT_DUPLICATE("mm", "duplicate m");
	GENERATED_TEST_ASSERT(generated_regexp_new_utf8(&region, "", "s",
			&regex, &error) == 0, SUITE, CASE_NAME, "single s should pass");
	EXPECT_DUPLICATE("ss", "duplicate s");
	GENERATED_TEST_ASSERT(generated_regexp_new_utf8(&region, "", "u",
			&regex, &error) == 0, SUITE, CASE_NAME, "single u should pass");
	EXPECT_DUPLICATE("uu", "duplicate u");
	GENERATED_TEST_ASSERT(generated_regexp_new_utf8(&region, "", "y",
			&regex, &error) == 0, SUITE, CASE_NAME, "single y should pass");
	EXPECT_DUPLICATE("yy", "duplicate y");
	return generated_test_pass(SUITE, CASE_NAME);
}

#undef EXPECT_DUPLICATE
