#include <string.h>
#include <stdint.h>

#include "compliance/generated/regex_core_helpers.h"
#include "compliance/generated/string_replace_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/replace/S15.5.4.11_A1_T14"

int
main(void)
{
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t text;
	jsval_t regex;
	jsval_t result;
	jsmethod_error_t error;
	jsmethod_string_replace_sizes_t sizes;

	jsval_region_init(&region, storage, sizeof(storage));

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/replace/S15.5.4.11_A1_T14.js
	 */

	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"ABBABABAB77BBAA", 15, &text) == 0,
			SUITE, CASE_NAME, "failed to build input string");
	GENERATED_TEST_ASSERT(generated_regexp_new_utf8(&region, "77", NULL, &regex,
			&error) == 0, SUITE, CASE_NAME, "failed to build regex");
	GENERATED_TEST_ASSERT(jsval_method_string_replace_measure(&region, text,
			regex, jsval_number(1.0), &sizes, &error) == 0,
			SUITE, CASE_NAME, "replace measure failed");
	GENERATED_TEST_ASSERT(sizes.result_len == strlen("ABBABABAB1BBAA"),
			SUITE, CASE_NAME, "expected result_len %zu, got %zu",
			strlen("ABBABABAB1BBAA"), sizes.result_len);
	GENERATED_TEST_ASSERT(jsval_method_string_replace(&region, text, regex,
			jsval_number(1.0), &result, &error) == 0,
			SUITE, CASE_NAME, "replace execution failed");
	GENERATED_TEST_ASSERT(generated_expect_jsval_utf8_string(&region, result,
			"ABBABABAB1BBAA", SUITE, CASE_NAME,
			"\"ABBABABAB77BBAA\".replace(/77/, 1)") ==
			GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected regex replace result");
	return generated_test_pass(SUITE, CASE_NAME);
}
