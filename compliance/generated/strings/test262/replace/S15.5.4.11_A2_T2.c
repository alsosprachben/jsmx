#include <string.h>
#include <stdint.h>

#include "compliance/generated/regex_core_helpers.h"
#include "compliance/generated/string_replace_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/replace/S15.5.4.11_A2_T2"

int
main(void)
{
	static const char expected[] = "She sells sea$schells by the sea$schore.";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t text;
	jsval_t replacement;
	jsval_t regex;
	jsval_t result;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/replace/S15.5.4.11_A2_T2.js
	 */

	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"She sells seashells by the seashore.", 36,
			&text) == 0, SUITE, CASE_NAME, "failed to build input string");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"$$sch", 5, &replacement) == 0,
			SUITE, CASE_NAME, "failed to build replacement");
	GENERATED_TEST_ASSERT(generated_regexp_new_utf8(&region, "sh", "g", &regex,
			&error) == 0, SUITE, CASE_NAME, "failed to build regex");
	GENERATED_TEST_ASSERT(jsval_method_string_replace(&region, text, regex,
			replacement, &result, &error) == 0, SUITE, CASE_NAME,
			"replace execution failed");
	GENERATED_TEST_ASSERT(generated_expect_jsval_utf8_string(&region, result,
			expected, SUITE, CASE_NAME, "$$ replacement") ==
			GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected $$ replacement result");
	return generated_test_pass(SUITE, CASE_NAME);
}
