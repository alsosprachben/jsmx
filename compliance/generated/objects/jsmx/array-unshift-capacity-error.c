#include <errno.h>
#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "objects"
#define CASE_NAME "jsmx/array-unshift-capacity-error"

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
	 * compliance/js/objects/jsmx/array-unshift-capacity-error.js
	 *
	 * This idiomatic flattened translation exercises the dense-array capacity
	 * contract for unshift on a fully occupied native array.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_array_new(&region, 1, &array) == 0,
			SUITE, CASE_NAME, "failed to allocate native array");
	GENERATED_TEST_ASSERT(jsval_array_push(&region, array, jsval_number(5.0)) == 0,
			SUITE, CASE_NAME, "failed to push initial dense value");
	errno = 0;
	GENERATED_TEST_ASSERT(jsval_array_unshift(&region, array, jsval_number(4.0))
			== -1, SUITE, CASE_NAME,
			"expected unshift past planned capacity to fail");
	GENERATED_TEST_ASSERT(errno == ENOBUFS, SUITE, CASE_NAME,
			"expected ENOBUFS from unshift past planned capacity");
	GENERATED_TEST_ASSERT(expect_json(&region, array, "[5]")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected emitted JSON after failed unshift");

	return generated_test_pass(SUITE, CASE_NAME);
}
