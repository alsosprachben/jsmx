#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "strings"
#define CASE_NAME "test262/match/this-value-not-obj-coercible"

int
main(void)
{
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t pattern;
	jsval_t regex;
	jsval_t result;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)".", 1, &pattern) == 0,
			SUITE, CASE_NAME, "pattern build failed");
	GENERATED_TEST_ASSERT(jsval_regexp_new(&region, pattern, 0,
			jsval_undefined(), &regex, &error) == 0,
			SUITE, CASE_NAME, "regex build failed");
	GENERATED_TEST_ASSERT(jsval_method_string_match(&region, jsval_undefined(),
			1, regex, &result, &error) == -1,
			SUITE, CASE_NAME, "undefined receiver should fail");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_TYPE,
			SUITE, CASE_NAME, "expected TYPE error for undefined");
	GENERATED_TEST_ASSERT(jsval_method_string_match(&region, jsval_null(), 1,
			regex, &result, &error) == -1,
			SUITE, CASE_NAME, "null receiver should fail");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_TYPE,
			SUITE, CASE_NAME, "expected TYPE error for null");
	return generated_test_pass(SUITE, CASE_NAME);
}
