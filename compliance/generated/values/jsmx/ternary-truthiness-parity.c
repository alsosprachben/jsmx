#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "values"
#define CASE_NAME "jsmx/ternary-truthiness-parity"

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
	jsval_t empty;
	jsval_t text;
	jsval_t then_value;
	jsval_t else_value;
	jsval_t result;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region, (const uint8_t *)"", 0,
			&empty) == 0, SUITE, CASE_NAME,
			"failed to construct empty string");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region, (const uint8_t *)"x", 1,
			&text) == 0, SUITE, CASE_NAME,
			"failed to construct text string");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region, (const uint8_t *)"then",
			4, &then_value) == 0, SUITE, CASE_NAME,
			"failed to construct then string");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region, (const uint8_t *)"else",
			4, &else_value) == 0, SUITE, CASE_NAME,
			"failed to construct else string");

	if (jsval_truthy(&region, jsval_undefined())) {
		result = then_value;
	} else {
		result = else_value;
	}
	GENERATED_TEST_ASSERT(expect_string(&region, result,
			(const uint8_t *)"else", 4,
			"undefined ternary") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected undefined ternary result");

	if (jsval_truthy(&region, jsval_null())) {
		result = then_value;
	} else {
		result = else_value;
	}
	GENERATED_TEST_ASSERT(expect_string(&region, result,
			(const uint8_t *)"else", 4,
			"null ternary") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected null ternary result");

	if (jsval_truthy(&region, jsval_bool(0))) {
		result = then_value;
	} else {
		result = else_value;
	}
	GENERATED_TEST_ASSERT(expect_string(&region, result,
			(const uint8_t *)"else", 4,
			"false ternary") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected false ternary result");

	if (jsval_truthy(&region, jsval_number(0.0))) {
		result = then_value;
	} else {
		result = else_value;
	}
	GENERATED_TEST_ASSERT(expect_string(&region, result,
			(const uint8_t *)"else", 4,
			"0 ternary") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected zero ternary result");

	if (jsval_truthy(&region, empty)) {
		result = then_value;
	} else {
		result = else_value;
	}
	GENERATED_TEST_ASSERT(expect_string(&region, result,
			(const uint8_t *)"else", 4,
			"empty string ternary") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected empty-string ternary result");

	if (jsval_truthy(&region, text)) {
		result = then_value;
	} else {
		result = else_value;
	}
	GENERATED_TEST_ASSERT(expect_string(&region, result,
			(const uint8_t *)"then", 4,
			"string ternary") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected string ternary result");

	if (jsval_truthy(&region, jsval_number(1.0))) {
		result = then_value;
	} else {
		result = else_value;
	}
	GENERATED_TEST_ASSERT(expect_string(&region, result,
			(const uint8_t *)"then", 4,
			"number ternary") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected number ternary result");

	return generated_test_pass(SUITE, CASE_NAME);
}
