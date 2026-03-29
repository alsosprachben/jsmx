#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "objects"
#define CASE_NAME "jsmx/array-index-overwrite-and-emit"

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
	static const char expected[] = "[1,9,4]";
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t array;
	jsval_t got;
	size_t len_before;

	/*
	 * Generated from repo-authored source:
	 * compliance/js/objects/jsmx/array-index-overwrite-and-emit.js
	 *
	 * This idiomatic flattened translation exercises dense native array index
	 * overwrites and proves they preserve logical length while updating the
	 * emitted dense sequence.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_array_new(&region, 3, &array) == 0,
			SUITE, CASE_NAME, "failed to allocate native array");
	GENERATED_TEST_ASSERT(jsval_array_set(&region, array, 0, jsval_number(1.0))
			== 0, SUITE, CASE_NAME, "failed to set arr[0]");
	GENERATED_TEST_ASSERT(jsval_array_set(&region, array, 1, jsval_number(2.0))
			== 0, SUITE, CASE_NAME, "failed to set arr[1]");
	GENERATED_TEST_ASSERT(jsval_array_set(&region, array, 2, jsval_number(3.0))
			== 0, SUITE, CASE_NAME, "failed to set arr[2]");

	len_before = jsval_array_length(&region, array);
	GENERATED_TEST_ASSERT(jsval_array_set(&region, array, 1, jsval_number(9.0))
			== 0, SUITE, CASE_NAME, "failed to overwrite arr[1]");
	GENERATED_TEST_ASSERT(jsval_array_set(&region, array, 2, jsval_number(4.0))
			== 0, SUITE, CASE_NAME, "failed to overwrite arr[2]");
	GENERATED_TEST_ASSERT(jsval_array_length(&region, array) == len_before,
			SUITE, CASE_NAME,
			"dense index overwrites unexpectedly changed logical length");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, array, 1, &got) == 0,
			SUITE, CASE_NAME, "failed to read overwritten arr[1]");
	GENERATED_TEST_ASSERT(got.kind == JSVAL_KIND_NUMBER && got.as.number == 9.0,
			SUITE, CASE_NAME, "expected arr[1] to read back as 9");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, array, 2, &got) == 0,
			SUITE, CASE_NAME, "failed to read overwritten arr[2]");
	GENERATED_TEST_ASSERT(got.kind == JSVAL_KIND_NUMBER && got.as.number == 4.0,
			SUITE, CASE_NAME, "expected arr[2] to read back as 4");

	GENERATED_TEST_ASSERT(expect_json(&region, array, expected)
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected emitted JSON after dense index overwrites");

	return generated_test_pass(SUITE, CASE_NAME);
}
