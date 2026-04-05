#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "objects"
#define CASE_NAME "jsmx/array-shift-empty-undefined"

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
	jsval_t got;

	/*
	 * Generated from repo-authored source:
	 * compliance/js/objects/jsmx/array-shift-empty-undefined.js
	 *
	 * This idiomatic flattened translation exercises empty shift plus shifts
	 * from dense undefined front slots after an explicit length grow.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_array_new(&region, 2, &array) == 0,
			SUITE, CASE_NAME, "failed to allocate native array");
	GENERATED_TEST_ASSERT(jsval_array_shift(&region, array, &got) == 0,
			SUITE, CASE_NAME, "failed to shift empty array");
	GENERATED_TEST_ASSERT(got.kind == JSVAL_KIND_UNDEFINED, SUITE, CASE_NAME,
			"expected empty shift to return undefined");
	GENERATED_TEST_ASSERT(jsval_array_set_length(&region, array, 2) == 0,
			SUITE, CASE_NAME, "failed to grow array length");
	GENERATED_TEST_ASSERT(jsval_array_shift(&region, array, &got) == 0,
			SUITE, CASE_NAME, "failed to shift grown undefined head");
	GENERATED_TEST_ASSERT(got.kind == JSVAL_KIND_UNDEFINED, SUITE, CASE_NAME,
			"expected first shifted slot to return undefined");
	GENERATED_TEST_ASSERT(jsval_array_shift(&region, array, &got) == 0,
			SUITE, CASE_NAME, "failed to shift final undefined slot");
	GENERATED_TEST_ASSERT(got.kind == JSVAL_KIND_UNDEFINED, SUITE, CASE_NAME,
			"expected final shifted slot to return undefined");
	GENERATED_TEST_ASSERT(jsval_array_length(&region, array) == 0,
			SUITE, CASE_NAME, "expected array to end empty after repeated shifts");
	GENERATED_TEST_ASSERT(expect_json(&region, array, "[]")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected emitted JSON after repeated undefined shifts");

	return generated_test_pass(SUITE, CASE_NAME);
}
