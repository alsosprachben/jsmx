#include "compliance/generated/regex_match_all_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/matchAll/this-val-non-obj-coercible"

int
main(void)
{
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t iterator;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));

	errno = 0;
	GENERATED_TEST_ASSERT(jsval_method_string_match_all(&region,
			jsval_undefined(), 0, jsval_undefined(), &iterator, &error) == -1,
			SUITE, CASE_NAME, "expected undefined receiver failure");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_TYPE, SUITE,
			CASE_NAME, "expected TYPE for undefined receiver, got %d",
			(int)error.kind);

	errno = 0;
	GENERATED_TEST_ASSERT(jsval_method_string_match_all(&region,
			jsval_null(), 0, jsval_undefined(), &iterator, &error) == -1,
			SUITE, CASE_NAME, "expected null receiver failure");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_TYPE, SUITE,
			CASE_NAME, "expected TYPE for null receiver, got %d",
			(int)error.kind);
	return generated_test_pass(SUITE, CASE_NAME);
}
