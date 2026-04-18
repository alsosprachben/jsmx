#include <errno.h>
#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "bigint"
#define CASE_NAME "jsmx/bigint-construct-utf8-parse"

static int
expect_bigint_utf8(jsval_region_t *region, jsval_t value,
		const char *expected, const char *label)
{
	size_t len = 0;
	uint8_t buf[32];
	size_t expected_len = strlen(expected);

	if (value.kind != JSVAL_KIND_BIGINT) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: kind != BIGINT", label);
	}
	if (jsval_bigint_copy_utf8(region, value, NULL, 0, &len) != 0 ||
			len != expected_len || len >= sizeof(buf)) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: len %zu != %zu", label, len, expected_len);
	}
	if (jsval_bigint_copy_utf8(region, value, buf, len, NULL) != 0 ||
			memcmp(buf, expected, len) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: bytes mismatch", label);
	}
	return GENERATED_TEST_PASS;
}

int
main(void)
{
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t zero_plain;
	jsval_t zero_from_minus;
	jsval_t zero_from_string;
	jsval_t big_positive;
	jsval_t big_positive_copy;
	jsval_t big_negative;
	jsval_t big_negative_via_minus;
	double number = 0.0;

	jsval_region_init(&region, storage, sizeof(storage));

	/* Zero normalizes across construction paths: i64(0),
	 * parse("0"), and parse("-0000") are all strict-equal. */
	GENERATED_TEST_ASSERT(
			jsval_bigint_new_i64(&region, 0, &zero_plain) == 0 &&
			jsval_bigint_new_utf8(&region,
				(const uint8_t *)"0", 1, &zero_from_string) == 0 &&
			jsval_bigint_new_utf8(&region,
				(const uint8_t *)"-0000", 5, &zero_from_minus)
				== 0,
			SUITE, CASE_NAME, "zero construction paths");
	GENERATED_TEST_ASSERT(
			jsval_strict_eq(&region, zero_plain, zero_from_string) == 1 &&
			jsval_strict_eq(&region, zero_plain, zero_from_minus) == 1,
			SUITE, CASE_NAME, "zero normalization");
	GENERATED_TEST_ASSERT(
			expect_bigint_utf8(&region, zero_from_minus, "0",
				"-0000 round-trip") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "zero serialization");

	/* Parse past 2^63 (20 digits). */
	GENERATED_TEST_ASSERT(
			jsval_bigint_new_utf8(&region,
				(const uint8_t *)"12345678901234567890", 20,
				&big_positive) == 0 &&
			expect_bigint_utf8(&region, big_positive,
				"12345678901234567890", "big +")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "big positive round-trip");

	/* Negative counterpart round-trips, and unary_minus of the
	 * positive yields the negative form (strict-equal). */
	GENERATED_TEST_ASSERT(
			jsval_bigint_new_utf8(&region,
				(const uint8_t *)"-12345678901234567890", 21,
				&big_negative) == 0 &&
			expect_bigint_utf8(&region, big_negative,
				"-12345678901234567890", "big -")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "big negative round-trip");
	GENERATED_TEST_ASSERT(
			jsval_unary_minus(&region, big_positive,
				&big_negative_via_minus) == 0 &&
			jsval_strict_eq(&region, big_negative,
				big_negative_via_minus) == 1,
			SUITE, CASE_NAME, "unary_minus equals parsed negative");

	/* Parsed copy is strict-equal to original (re-parse of
	 * same decimal string yields same normalized BigInt). */
	GENERATED_TEST_ASSERT(
			jsval_bigint_new_utf8(&region,
				(const uint8_t *)"12345678901234567890", 20,
				&big_positive_copy) == 0 &&
			jsval_strict_eq(&region, big_positive, big_positive_copy)
				== 1,
			SUITE, CASE_NAME, "reparse strict-eq");

	/* BigInt does NOT implicitly coerce to Number:
	 * jsval_to_number returns -1 with errno == ENOTSUP. */
	errno = 0;
	GENERATED_TEST_ASSERT(
			jsval_to_number(&region, zero_plain, &number) < 0 &&
			errno == ENOTSUP,
			SUITE, CASE_NAME, "BigInt to_number should ENOTSUP");

	return generated_test_pass(SUITE, CASE_NAME);
}
