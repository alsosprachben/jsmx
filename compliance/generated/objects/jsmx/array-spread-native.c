#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "objects"
#define CASE_NAME "jsmx/array-spread-native"

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
	 * compliance/js/objects/jsmx/array-spread-native.js
	 *
	 * This idiomatic flattened translation exercises fresh array spread from
	 * a native dense source through jsval_array_clone_dense().
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_array_new(&region, 2, &src) == 0,
			SUITE, CASE_NAME, "failed to allocate source array");
	GENERATED_TEST_ASSERT(jsval_array_push(&region, src, jsval_number(4.0)) == 0
			&& jsval_array_push(&region, src, jsval_number(5.0)) == 0,
			SUITE, CASE_NAME, "failed to initialize source array");
	GENERATED_TEST_ASSERT(jsval_array_clone_dense(&region, src, 2, &clone) == 0,
			SUITE, CASE_NAME, "failed to clone spread source array");
	GENERATED_TEST_ASSERT(jsval_array_set(&region, clone, 0,
			jsval_number(9.0)) == 0, SUITE, CASE_NAME,
			"failed to overwrite cloned spread array element");
	GENERATED_TEST_ASSERT(expect_json(&region, src, "[4,5]")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"expected native array spread source to remain unchanged");
	GENERATED_TEST_ASSERT(expect_json(&region, clone, "[9,5]")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"expected native array spread clone to support independent overwrite");

	return generated_test_pass(SUITE, CASE_NAME);
}
