#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "wintertc"
#define CASE_NAME "jsmx/queuemicrotask-parity"

static int callback_invocations = 0;

static int
parity_callback(jsval_region_t *region, size_t argc, const jsval_t *argv,
		jsval_t *result_ptr, jsmethod_error_t *error)
{
	(void)region;
	(void)argc;
	(void)argv;
	(void)error;
	if (result_ptr != NULL) {
		*result_ptr = jsval_undefined();
	}
	callback_invocations++;
	return 0;
}

int
main(void)
{
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t fn;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(
			jsval_function_new(&region, parity_callback, 0, 0,
				jsval_undefined(), &fn) == 0,
			SUITE, CASE_NAME, "failed to create callback");

	/* Pre-condition: callback has not run. */
	GENERATED_TEST_ASSERT(callback_invocations == 0,
			SUITE, CASE_NAME, "counter should start at 0");

	/* queueMicrotask(fn): schedule on the microtask queue. */
	GENERATED_TEST_ASSERT(jsval_queue_microtask(&region, fn) == 0,
			SUITE, CASE_NAME, "jsval_queue_microtask returned -1");

	/* Before draining, callback still hasn't run. */
	GENERATED_TEST_ASSERT(callback_invocations == 0,
			SUITE, CASE_NAME, "callback ran before drain");

	/* Drain the microtask queue; callback runs exactly once. */
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0,
			SUITE, CASE_NAME, "microtask drain failed");
	GENERATED_TEST_ASSERT(callback_invocations == 1,
			SUITE, CASE_NAME,
			"callback should run exactly once (got %d)",
			callback_invocations);

	/* Draining again with no new enqueues is a no-op. */
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0,
			SUITE, CASE_NAME, "second drain failed");
	GENERATED_TEST_ASSERT(callback_invocations == 1,
			SUITE, CASE_NAME,
			"callback should not re-run on empty drain");

	return generated_test_pass(SUITE, CASE_NAME);
}
