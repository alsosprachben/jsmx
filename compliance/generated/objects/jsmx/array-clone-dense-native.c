#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "objects"
#define CASE_NAME "jsmx/array-clone-dense-native"

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
	jsval_t src;
	jsval_t clone;

	/*
	 * Generated from repo-authored source:
	 * compliance/js/objects/jsmx/array-clone-dense-native.js
	 *
	 * This idiomatic flattened translation exercises fresh dense-array cloning
	 * from one native array into a new native array through
	 * jsval_array_clone_dense().
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_array_new(&region, 2, &src) == 0,
			SUITE, CASE_NAME, "failed to allocate source array");
	GENERATED_TEST_ASSERT(jsval_array_push(&region, src, jsval_number(1.0)) == 0,
			SUITE, CASE_NAME, "failed to push first source element");
	GENERATED_TEST_ASSERT(jsval_array_push(&region, src, jsval_number(2.0)) == 0,
			SUITE, CASE_NAME, "failed to push second source element");
	GENERATED_TEST_ASSERT(jsval_array_clone_dense(&region, src, 3, &clone) == 0,
			SUITE, CASE_NAME, "failed to clone native dense array");
	GENERATED_TEST_ASSERT(jsval_array_push(&region, clone, jsval_number(3.0)) == 0,
			SUITE, CASE_NAME, "failed to append to cloned dense array");
	GENERATED_TEST_ASSERT(expect_json(&region, src, "[1,2]")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"expected native source to remain unchanged after fresh dense-array clone mutation");
	GENERATED_TEST_ASSERT(expect_json(&region, clone, "[1,2,3]")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"expected fresh native dense-array clone to preserve element order and allow independent append");

	return generated_test_pass(SUITE, CASE_NAME);
}
