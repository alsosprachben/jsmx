#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "strings"
#define CASE_NAME "test262/exec/u-lastindex-adv"

int
main(void)
{
	uint8_t storage[16384];
	jsval_region_t region;
	uint16_t pattern_units[] = {0xDF06};
	uint16_t input_units[] = {0xD834, 0xDF06};
	jsval_t pattern;
	jsval_t flags;
	jsval_t regex;
	jsval_t input;
	jsval_t result;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, pattern_units, 1,
			&pattern) == 0, SUITE, CASE_NAME, "pattern build failed");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"u", 1, &flags) == 0,
			SUITE, CASE_NAME, "flags build failed");
	GENERATED_TEST_ASSERT(jsval_regexp_new(&region, pattern, 1, flags, &regex,
			&error) == 0, SUITE, CASE_NAME, "regex build failed");
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, input_units, 2,
			&input) == 0, SUITE, CASE_NAME, "input build failed");
	GENERATED_TEST_ASSERT(jsval_regexp_exec(&region, regex, input, &result,
			&error) == 0, SUITE, CASE_NAME, "exec failed");
	GENERATED_TEST_ASSERT(result.kind == JSVAL_KIND_NULL,
			SUITE, CASE_NAME, "expected null result");
	return generated_test_pass(SUITE, CASE_NAME);
}
