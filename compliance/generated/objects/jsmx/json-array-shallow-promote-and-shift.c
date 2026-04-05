#include <errno.h>
#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "objects"
#define CASE_NAME "jsmx/json-array-shallow-promote-and-shift"

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
	static const uint8_t input[] = "[4,5,6]";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t array;
	jsval_t got;
	size_t bytes;

	/*
	 * Generated from repo-authored source:
	 * compliance/js/objects/jsmx/json-array-shallow-promote-and-shift.js
	 *
	 * This idiomatic flattened translation keeps the parsed array JSON-backed
	 * for the initial read, preserves the ENOTSUP mutation boundary before
	 * promotion, then shallow-promotes exactly once before the dense shift.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_json_parse(&region, input, sizeof(input) - 1, 8,
			&array) == 0, SUITE, CASE_NAME,
			"failed to parse input JSON array");
	GENERATED_TEST_ASSERT(jsval_is_json_backed(array) == 1, SUITE, CASE_NAME,
			"expected parsed array to stay JSON-backed before promotion");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, array, 0, &got) == 0,
			SUITE, CASE_NAME, "failed to read parsed arr[0]");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, got, jsval_number(4.0)) == 1,
			SUITE, CASE_NAME, "expected parsed arr[0] to read back as 4");
	errno = 0;
	GENERATED_TEST_ASSERT(jsval_array_shift(&region, array, &got) < 0,
			SUITE, CASE_NAME, "expected JSON-backed shift to fail before promotion");
	GENERATED_TEST_ASSERT(errno == ENOTSUP, SUITE, CASE_NAME,
			"expected JSON-backed shift to fail with ENOTSUP");

	GENERATED_TEST_ASSERT(jsval_promote_array_shallow_measure(&region, array, 3,
			&bytes) == 0, SUITE, CASE_NAME,
			"failed to measure shallow array promotion");
	GENERATED_TEST_ASSERT(jsval_promote_array_shallow_in_place(&region, &array, 3)
			== 0, SUITE, CASE_NAME,
			"failed to shallow-promote parsed array");
	GENERATED_TEST_ASSERT(jsval_is_native(array) == 1, SUITE, CASE_NAME,
			"expected promoted array to become native");
	GENERATED_TEST_ASSERT(jsval_region_set_root(&region, array) == 0,
			SUITE, CASE_NAME, "failed to persist promoted array root");
	GENERATED_TEST_ASSERT(jsval_array_shift(&region, array, &got) == 0,
			SUITE, CASE_NAME, "failed to shift promoted dense head");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, got, jsval_number(4.0)) == 1,
			SUITE, CASE_NAME, "expected promoted shift to return 4");
	GENERATED_TEST_ASSERT(expect_json(&region, array, "[5,6]")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected emitted JSON after shallow-promoted shift");

	return generated_test_pass(SUITE, CASE_NAME);
}
