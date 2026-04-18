#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "wintertc"
#define CASE_NAME "jsmx/queuemicrotask-errors"

int
main(void)
{
	uint8_t storage[8192];
	jsval_region_t region;

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(
			jsval_queue_microtask(&region, jsval_number(42.0)) < 0,
			SUITE, CASE_NAME,
			"expected queue_microtask to fail on non-function (number)");

	GENERATED_TEST_ASSERT(
			jsval_queue_microtask(&region, jsval_undefined()) < 0,
			SUITE, CASE_NAME,
			"expected queue_microtask to fail on undefined");

	return generated_test_pass(SUITE, CASE_NAME);
}
