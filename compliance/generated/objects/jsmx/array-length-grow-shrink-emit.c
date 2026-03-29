#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "objects"
#define CASE_NAME "jsmx/array-length-grow-shrink-emit"

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
	static const char expected[] = "[1,2]";
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t array;
	jsval_t got;

	/*
	 * Generated from repo-authored source:
	 * compliance/js/objects/jsmx/array-length-grow-shrink-emit.js
	 *
	 * This idiomatic flattened translation exercises dense native length
	 * growth, writes into the grown slots, and a later shrink that truncates
	 * the visible dense prefix for reads and JSON emission.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_array_new(&region, 4, &array) == 0,
			SUITE, CASE_NAME, "failed to allocate native array");
	GENERATED_TEST_ASSERT(jsval_array_set(&region, array, 0, jsval_number(1.0))
			== 0, SUITE, CASE_NAME, "failed to set arr[0]");
	GENERATED_TEST_ASSERT(jsval_array_set(&region, array, 1, jsval_number(2.0))
			== 0, SUITE, CASE_NAME, "failed to set arr[1]");
	GENERATED_TEST_ASSERT(jsval_array_set_length(&region, array, 4) == 0,
			SUITE, CASE_NAME, "failed to grow dense array length");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, array, 2, &got) == 0,
			SUITE, CASE_NAME, "failed to read grown arr[2]");
	GENERATED_TEST_ASSERT(got.kind == JSVAL_KIND_UNDEFINED, SUITE, CASE_NAME,
			"expected grown arr[2] to read as undefined");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, array, 3, &got) == 0,
			SUITE, CASE_NAME, "failed to read grown arr[3]");
	GENERATED_TEST_ASSERT(got.kind == JSVAL_KIND_UNDEFINED, SUITE, CASE_NAME,
			"expected grown arr[3] to read as undefined");

	GENERATED_TEST_ASSERT(jsval_array_set(&region, array, 2, jsval_number(7.0))
			== 0, SUITE, CASE_NAME, "failed to write arr[2]");
	GENERATED_TEST_ASSERT(jsval_array_set(&region, array, 3, jsval_number(8.0))
			== 0, SUITE, CASE_NAME, "failed to write arr[3]");
	GENERATED_TEST_ASSERT(jsval_array_set_length(&region, array, 2) == 0,
			SUITE, CASE_NAME, "failed to shrink dense array length");
	GENERATED_TEST_ASSERT(jsval_array_length(&region, array) == 2,
			SUITE, CASE_NAME, "expected shrunk dense array length to be 2");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, array, 2, &got) == 0,
			SUITE, CASE_NAME, "failed to read truncated arr[2]");
	GENERATED_TEST_ASSERT(got.kind == JSVAL_KIND_UNDEFINED, SUITE, CASE_NAME,
			"expected truncated arr[2] to read as undefined");

	GENERATED_TEST_ASSERT(expect_json(&region, array, expected)
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected emitted JSON after dense array shrink");

	return generated_test_pass(SUITE, CASE_NAME);
}
