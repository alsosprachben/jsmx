#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "objects"
#define CASE_NAME "jsmx/array-unshift-observable"

static int
expect_json(jsval_region_t *region, jsval_t value, const char *expected)
{
	size_t expected_len = strlen(expected);
	size_t actual_len = 0;
	uint8_t buf[expected_len ? expected_len : 1];

	if (jsval_copy_json(region, value, NULL, 0, &actual_len) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"failed to measure emitted JSON");
	}
	if (actual_len != expected_len) {
		return generated_test_fail(SUITE, CASE_NAME,
				"expected %zu JSON bytes, got %zu",
				expected_len, actual_len);
	}
	if (jsval_copy_json(region, value, buf, actual_len, NULL) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"failed to copy emitted JSON");
	}
	if (memcmp(buf, expected, expected_len) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"emitted JSON did not match expected output");
	}
	return GENERATED_TEST_PASS;
}

int
main(void)
{
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t array;

	/*
	 * Generated from repo-authored source:
	 * compliance/js/objects/jsmx/array-unshift-observable.js
	 *
	 * This idiomatic flattened translation exercises dense native array front
	 * insertion through unshift.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_array_new(&region, 4, &array) == 0,
			SUITE, CASE_NAME, "failed to allocate native array");
	GENERATED_TEST_ASSERT(jsval_array_push(&region, array, jsval_number(1.0)) == 0,
			SUITE, CASE_NAME, "failed to push initial array value");
	GENERATED_TEST_ASSERT(jsval_array_unshift(&region, array, jsval_number(9.0)) == 0,
			SUITE, CASE_NAME, "failed to unshift first dense head");
	GENERATED_TEST_ASSERT(jsval_array_unshift(&region, array, jsval_number(7.0)) == 0,
			SUITE, CASE_NAME, "failed to unshift second dense head");
	GENERATED_TEST_ASSERT(expect_json(&region, array, "[7,9,1]")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected emitted JSON after dense unshift operations");

	return generated_test_pass(SUITE, CASE_NAME);
}
