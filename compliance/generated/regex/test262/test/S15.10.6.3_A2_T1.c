#include "compliance/generated/regex_core_helpers.h"

#define SUITE "regex"
#define CASE_NAME "test262/test/S15.10.6.3_A2_T1"

int
main(void)
{
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t receiver;
	jsmethod_error_t error;
	int test_result = 0;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_object_new(&region, 0, &receiver) == 0,
			SUITE, CASE_NAME, "receiver build failed");
	errno = 0;
	GENERATED_TEST_ASSERT(jsval_regexp_test(&region, receiver,
			jsval_number(0.0), &test_result, &error) < 0,
			SUITE, CASE_NAME, "expected non-RegExp receiver failure");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_TYPE,
			SUITE, CASE_NAME, "expected TypeError boundary");
	GENERATED_TEST_ASSERT(errno == EINVAL,
			SUITE, CASE_NAME, "expected EINVAL");
	return generated_test_pass(SUITE, CASE_NAME);
}
