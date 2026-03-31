#include "compliance/generated/regex_core_helpers.h"

#define SUITE "regex"
#define CASE_NAME "test262/test/S15.10.6.3_A1_T15"

static int
generated_object_to_string_false(jsval_region_t *region, jsval_t *value_ptr)
{
	return jsval_string_new_utf8(region, (const uint8_t *)"false", 5,
			value_ptr);
}

int
main(void)
{
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t regex;
	jsval_t input;
	jsval_t exec_result;
	jsmethod_error_t error;
	int test_result = 0;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(generated_regexp_new_utf8(&region, "LS", "i",
			&regex, &error) == 0, SUITE, CASE_NAME, "regex build failed");
	GENERATED_TEST_ASSERT(generated_object_to_string_false(&region, &input) == 0,
			SUITE, CASE_NAME, "slow-path input build failed");
	GENERATED_TEST_ASSERT(jsval_regexp_test(&region, regex, input,
			&test_result, &error) == 0, SUITE, CASE_NAME, "test failed");
	GENERATED_TEST_ASSERT(jsval_regexp_exec(&region, regex, input,
			&exec_result, &error) == 0, SUITE, CASE_NAME, "exec failed");
	GENERATED_TEST_ASSERT(test_result == (exec_result.kind != JSVAL_KIND_NULL),
			SUITE, CASE_NAME, "test/exec equivalence failed");
	return generated_test_pass(SUITE, CASE_NAME);
}
