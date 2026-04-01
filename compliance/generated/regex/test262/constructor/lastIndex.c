#include "compliance/generated/regex_core_helpers.h"

#define SUITE "regex"
#define CASE_NAME "test262/constructor/lastIndex"

int
main(void)
{
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t regex;
	jsmethod_error_t error;
	size_t last_index = SIZE_MAX;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(generated_regexp_new_utf8(&region, "", NULL,
			&regex, &error) == 0, SUITE, CASE_NAME, "regex build failed");
	GENERATED_TEST_ASSERT(jsval_regexp_get_last_index(&region, regex,
			&last_index) == 0, SUITE, CASE_NAME, "lastIndex lookup failed");
	GENERATED_TEST_ASSERT(last_index == 0, SUITE, CASE_NAME,
			"expected lastIndex 0 after construction");
	return generated_test_pass(SUITE, CASE_NAME);
}
