#include "compliance/generated/regex_core_helpers.h"

#define SUITE "regex"
#define CASE_NAME "test262/flags/this-val-regexp"

int
main(void)
{
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t regex;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	/*
	 * Generated from test262:
	 * test/built-ins/RegExp/prototype/flags/this-val-regexp.js
	 *
	 * Direct-lowered: this fixture preserves the current semantic flags surface
	 * for the supported gimsuy subset. The upstream d/v checks stay outside the
	 * current runtime boundary and are documented as unsupported in the regex
	 * compatibility guide and manifest notes.
	 */
	GENERATED_TEST_ASSERT(generated_regexp_new_utf8(&region, ".", NULL,
			&regex, &error) == 0, SUITE, CASE_NAME, "regex build failed");
	GENERATED_TEST_ASSERT(generated_expect_regexp_flags(&region, regex, "",
			SUITE, CASE_NAME, "flags() none") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "flags() none mismatch");
	GENERATED_TEST_ASSERT(generated_regexp_new_utf8(&region, ".", "g",
			&regex, &error) == 0, SUITE, CASE_NAME, "g regex build failed");
	GENERATED_TEST_ASSERT(generated_expect_regexp_flags(&region, regex, "g",
			SUITE, CASE_NAME, "flags() global") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "flags() global mismatch");
	GENERATED_TEST_ASSERT(generated_regexp_new_utf8(&region, ".", "i",
			&regex, &error) == 0, SUITE, CASE_NAME, "i regex build failed");
	GENERATED_TEST_ASSERT(generated_expect_regexp_flags(&region, regex, "i",
			SUITE, CASE_NAME, "flags() ignoreCase") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "flags() ignoreCase mismatch");
	GENERATED_TEST_ASSERT(generated_regexp_new_utf8(&region, ".", "m",
			&regex, &error) == 0, SUITE, CASE_NAME, "m regex build failed");
	GENERATED_TEST_ASSERT(generated_expect_regexp_flags(&region, regex, "m",
			SUITE, CASE_NAME, "flags() multiline") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "flags() multiline mismatch");
	GENERATED_TEST_ASSERT(generated_regexp_new_utf8(&region, ".", "s",
			&regex, &error) == 0, SUITE, CASE_NAME, "s regex build failed");
	GENERATED_TEST_ASSERT(generated_expect_regexp_flags(&region, regex, "s",
			SUITE, CASE_NAME, "flags() dotAll") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "flags() dotAll mismatch");
	GENERATED_TEST_ASSERT(generated_regexp_new_utf8(&region, ".", "u",
			&regex, &error) == 0, SUITE, CASE_NAME, "u regex build failed");
	GENERATED_TEST_ASSERT(generated_expect_regexp_flags(&region, regex, "u",
			SUITE, CASE_NAME, "flags() unicode") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "flags() unicode mismatch");
	GENERATED_TEST_ASSERT(generated_regexp_new_utf8(&region, ".", "y",
			&regex, &error) == 0, SUITE, CASE_NAME, "y regex build failed");
	GENERATED_TEST_ASSERT(generated_expect_regexp_flags(&region, regex, "y",
			SUITE, CASE_NAME, "flags() sticky") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "flags() sticky mismatch");
	return generated_test_pass(SUITE, CASE_NAME);
}
