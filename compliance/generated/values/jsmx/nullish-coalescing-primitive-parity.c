#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "values"
#define CASE_NAME "jsmx/nullish-coalescing-primitive-parity"

static int
expect_string(jsval_region_t *region, jsval_t value,
		const uint8_t *expected, size_t expected_len, const char *label)
{
	size_t actual_len = 0;
	uint8_t buf[expected_len ? expected_len : 1];

	if (jsval_string_copy_utf8(region, value, NULL, 0, &actual_len) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: failed to measure string result", label);
	}
	if (actual_len != expected_len) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: expected %zu bytes, got %zu",
				label, expected_len, actual_len);
	}
	if (jsval_string_copy_utf8(region, value, buf, actual_len, NULL) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: failed to copy string result", label);
	}
	if (memcmp(buf, expected, expected_len) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: string result did not match expected output", label);
	}
	return GENERATED_TEST_PASS;
}

int
main(void)
{
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t fallback;
	jsval_t text;
	jsval_t left;
	jsval_t result;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"fallback", 8, &fallback) == 0, SUITE, CASE_NAME,
			"failed to construct fallback string");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region, (const uint8_t *)"x", 1,
			&text) == 0, SUITE, CASE_NAME,
			"failed to construct text string");

	left = jsval_undefined();
	if (jsval_is_nullish(left)) {
		result = fallback;
	} else {
		result = left;
	}
	GENERATED_TEST_ASSERT(expect_string(&region, result,
			(const uint8_t *)"fallback", 8,
			"undefined ?? fallback") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected undefined ?? fallback result");

	left = jsval_null();
	if (jsval_is_nullish(left)) {
		result = fallback;
	} else {
		result = left;
	}
	GENERATED_TEST_ASSERT(expect_string(&region, result,
			(const uint8_t *)"fallback", 8,
			"null ?? fallback") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected null ?? fallback result");

	left = jsval_bool(0);
	if (jsval_is_nullish(left)) {
		result = fallback;
	} else {
		result = left;
	}
	GENERATED_TEST_ASSERT(result.kind == JSVAL_KIND_BOOL && result.as.boolean == 0,
			SUITE, CASE_NAME, "expected false to survive nullish coalescing");

	left = jsval_number(0.0);
	if (jsval_is_nullish(left)) {
		result = fallback;
	} else {
		result = left;
	}
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, result, jsval_number(0.0)) == 1,
			SUITE, CASE_NAME, "expected 0 to survive nullish coalescing");

	left = jsval_undefined();
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region, (const uint8_t *)"", 0,
			&left) == 0, SUITE, CASE_NAME,
			"failed to construct empty string");
	if (jsval_is_nullish(left)) {
		result = fallback;
	} else {
		result = left;
	}
	GENERATED_TEST_ASSERT(expect_string(&region, result, (const uint8_t *)"", 0,
			"empty string ?? fallback") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected empty string ?? fallback result");

	left = text;
	if (jsval_is_nullish(left)) {
		result = fallback;
	} else {
		result = left;
	}
	GENERATED_TEST_ASSERT(expect_string(&region, result, (const uint8_t *)"x", 1,
			"string ?? fallback") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected string ?? fallback result");

	return generated_test_pass(SUITE, CASE_NAME);
}
