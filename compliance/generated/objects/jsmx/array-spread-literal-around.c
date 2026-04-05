#include <errno.h>
#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "objects"
#define CASE_NAME "jsmx/array-spread-literal-around"

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

static int
append_array(jsval_region_t *region, jsval_t dst, jsval_t src)
{
	size_t len;
	size_t i;

	if (region == NULL) {
		errno = EINVAL;
		return -1;
	}
	len = jsval_array_length(region, src);
	for (i = 0; i < len; i++) {
		jsval_t value;

		if (jsval_array_get(region, src, i, &value) < 0) {
			return -1;
		}
		if (jsval_array_push(region, dst, value) < 0) {
			return -1;
		}
	}
	return 0;
}

int
main(void)
{
	static const uint8_t json_source[] = "[1,2]";
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t src;
	jsval_t out;

	/*
	 * Generated from repo-authored source:
	 * compliance/js/objects/jsmx/array-spread-literal-around.js
	 *
	 * This idiomatic flattened translation exercises literal elements around
	 * an array spread segment through explicit push plus array append.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_json_parse(&region, json_source,
			sizeof(json_source) - 1, 8, &src) == 0,
			SUITE, CASE_NAME, "failed to parse spread source array");
	GENERATED_TEST_ASSERT(jsval_array_new(&region, 4, &out) == 0,
			SUITE, CASE_NAME, "failed to allocate output array");
	GENERATED_TEST_ASSERT(jsval_array_push(&region, out, jsval_number(0.0)) == 0,
			SUITE, CASE_NAME, "failed to push leading literal element");
	GENERATED_TEST_ASSERT(append_array(&region, out, src) == 0,
			SUITE, CASE_NAME, "failed to append spread source array");
	GENERATED_TEST_ASSERT(jsval_array_push(&region, out, jsval_number(9.0)) == 0,
			SUITE, CASE_NAME, "failed to push trailing literal element");
	GENERATED_TEST_ASSERT(expect_json(&region, out, "[0,1,2,9]")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"expected literal elements around array spread to preserve order");

	return generated_test_pass(SUITE, CASE_NAME);
}
