#include <math.h>

#include "compliance/generated/string_split_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/split/separator-undef-limit-zero"

int
main(void)
{
	static const char *input = "undefined is not a function";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t text;
	jsval_t result;
	jsmethod_error_t error;

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/split/separator-undef-limit-zero.js
	 *
	 * This idiomatic flattened translation preserves the primitive ToUint32(limit)
	 * zero cases. The upstream object valueOf callback cases are intentionally
	 * omitted because they cross the current flattened boundary and would need
	 * an explicit translator slow path.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)input, strlen(input), &text) == 0,
			SUITE, CASE_NAME, "failed to create input string");

	GENERATED_TEST_ASSERT(jsval_method_string_split(&region, text, 1,
			jsval_undefined(), 1, jsval_number(0.0), &result, &error) == 0,
			SUITE, CASE_NAME, "split(undefined, 0) failed");
	GENERATED_TEST_ASSERT(generated_expect_split_array_strings(&region, result,
			NULL, 0, SUITE, CASE_NAME, "#1") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "#1 result mismatch");

	GENERATED_TEST_ASSERT(jsval_method_string_split(&region, text, 1,
			jsval_undefined(), 1, jsval_bool(0), &result, &error) == 0,
			SUITE, CASE_NAME, "split(undefined, false) failed");
	GENERATED_TEST_ASSERT(generated_expect_split_array_strings(&region, result,
			NULL, 0, SUITE, CASE_NAME, "#2") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "#2 result mismatch");

	GENERATED_TEST_ASSERT(jsval_method_string_split(&region, text, 1,
			jsval_undefined(), 1, jsval_null(), &result, &error) == 0,
			SUITE, CASE_NAME, "split(undefined, null) failed");
	GENERATED_TEST_ASSERT(generated_expect_split_array_strings(&region, result,
			NULL, 0, SUITE, CASE_NAME, "#3") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "#3 result mismatch");

	GENERATED_TEST_ASSERT(jsval_method_string_split(&region, text, 1,
			jsval_undefined(), 1, jsval_number(NAN), &result, &error) == 0,
			SUITE, CASE_NAME, "split(undefined, NaN) failed");
	GENERATED_TEST_ASSERT(generated_expect_split_array_strings(&region, result,
			NULL, 0, SUITE, CASE_NAME, "#4") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "#4 result mismatch");

	GENERATED_TEST_ASSERT(jsval_method_string_split(&region, text, 1,
			jsval_undefined(), 1, jsval_number(4294967296.0), &result, &error) == 0,
			SUITE, CASE_NAME, "split(undefined, 2**32) failed");
	GENERATED_TEST_ASSERT(generated_expect_split_array_strings(&region, result,
			NULL, 0, SUITE, CASE_NAME, "#5") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "#5 result mismatch");

	GENERATED_TEST_ASSERT(jsval_method_string_split(&region, text, 1,
			jsval_undefined(), 1, jsval_number(8589934592.0), &result, &error) == 0,
			SUITE, CASE_NAME, "split(undefined, 2**33) failed");
	GENERATED_TEST_ASSERT(generated_expect_split_array_strings(&region, result,
			NULL, 0, SUITE, CASE_NAME, "#6") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "#6 result mismatch");

	return generated_test_pass(SUITE, CASE_NAME);
}
