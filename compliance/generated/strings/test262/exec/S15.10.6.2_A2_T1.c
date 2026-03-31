#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "strings"
#define CASE_NAME "test262/exec/S15.10.6.2_A2_T1"

int
main(void)
{
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t fake_receiver;
	jsval_t input;
	jsval_t result;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"message to investigate", 22, &fake_receiver) == 0,
			SUITE, CASE_NAME, "receiver build failed");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"message to investigate", 22, &input) == 0,
			SUITE, CASE_NAME, "input build failed");
	GENERATED_TEST_ASSERT(jsval_regexp_exec(&region, fake_receiver, input,
			&result, &error) == -1, SUITE, CASE_NAME,
			"expected non-RegExp receiver to fail");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_TYPE,
			SUITE, CASE_NAME, "expected TYPE error, got %d", (int)error.kind);
	return generated_test_pass(SUITE, CASE_NAME);
}
