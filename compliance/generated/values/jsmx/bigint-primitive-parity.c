#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "values"
#define CASE_NAME "jsmx/bigint-primitive-parity"

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
	jsval_t zero;
	jsval_t answer;
	jsval_t duplicate;
	jsval_t text_42;
	jsval_t text_42_5;
	jsval_t result;
	int equal = 0;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_bigint_new_i64(&region, 0, &zero) == 0,
			SUITE, CASE_NAME, "failed to allocate zero bigint");
	GENERATED_TEST_ASSERT(jsval_bigint_new_i64(&region, 42, &answer) == 0,
			SUITE, CASE_NAME, "failed to allocate answer bigint");
	GENERATED_TEST_ASSERT(jsval_bigint_new_utf8(&region,
			(const uint8_t *)"42", 2, &duplicate) == 0,
			SUITE, CASE_NAME, "failed to allocate duplicate bigint");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"42", 2, &text_42) == 0,
			SUITE, CASE_NAME, "failed to allocate decimal string");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"42.5", 4, &text_42_5) == 0,
			SUITE, CASE_NAME, "failed to allocate fractional string");

	GENERATED_TEST_ASSERT(jsval_typeof(&region, zero, &result) == 0,
			SUITE, CASE_NAME, "failed to typeof zero bigint");
	GENERATED_TEST_ASSERT(expect_string(&region, result,
			(const uint8_t *)"bigint", 6, "typeof 0n")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected typeof bigint result");
	GENERATED_TEST_ASSERT(jsval_truthy(&region, zero) == 0,
			SUITE, CASE_NAME, "expected 0n to be falsy");
	GENERATED_TEST_ASSERT(jsval_truthy(&region, answer) == 1,
			SUITE, CASE_NAME, "expected nonzero bigint to be truthy");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, answer, duplicate) == 1,
			SUITE, CASE_NAME,
			"expected equal bigints to be strict-equal by numeric value");
	GENERATED_TEST_ASSERT(jsval_abstract_eq(&region, answer, text_42,
			&equal) == 0 && equal == 1,
			SUITE, CASE_NAME,
			"expected bigint to stay abstract-equal to an integral decimal string");
	GENERATED_TEST_ASSERT(jsval_abstract_eq(&region, answer, text_42_5,
			&equal) == 0 && equal == 0,
			SUITE, CASE_NAME,
			"expected bigint to stay abstract-inequal to a fractional string");
	GENERATED_TEST_ASSERT(jsval_abstract_eq(&region, answer, jsval_null(),
			&equal) == 0 && equal == 0,
			SUITE, CASE_NAME,
			"expected bigint to stay abstract-inequal to null");

	return generated_test_pass(SUITE, CASE_NAME);
}
