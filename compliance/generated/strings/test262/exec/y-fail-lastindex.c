#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "strings"
#define CASE_NAME "test262/exec/y-fail-lastindex"

int
main(void)
{
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t pattern;
	jsval_t flags;
	jsval_t regex;
	jsval_t input;
	jsval_t result;
	size_t last_index;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"c", 1, &pattern) == 0,
			SUITE, CASE_NAME, "pattern build failed");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"y", 1, &flags) == 0,
			SUITE, CASE_NAME, "flags build failed");
	GENERATED_TEST_ASSERT(jsval_regexp_new(&region, pattern, 1, flags, &regex,
			&error) == 0, SUITE, CASE_NAME, "regex build failed");
	GENERATED_TEST_ASSERT(jsval_regexp_set_last_index(&region, regex, 1) == 0,
			SUITE, CASE_NAME, "lastIndex set failed");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"abc", 3, &input) == 0,
			SUITE, CASE_NAME, "input build failed");
	GENERATED_TEST_ASSERT(jsval_regexp_exec(&region, regex, input, &result,
			&error) == 0, SUITE, CASE_NAME, "exec failed");
	GENERATED_TEST_ASSERT(result.kind == JSVAL_KIND_NULL,
			SUITE, CASE_NAME, "expected null sticky miss");
	GENERATED_TEST_ASSERT(jsval_regexp_get_last_index(&region, regex,
			&last_index) == 0 && last_index == 0,
			SUITE, CASE_NAME, "expected lastIndex reset to 0");
	return generated_test_pass(SUITE, CASE_NAME);
}
