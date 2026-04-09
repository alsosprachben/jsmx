#include <errno.h>
#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "values"
#define CASE_NAME "jsmx/bigint-arithmetic-parity"

static int
expect_bigint(jsval_region_t *region, jsval_t value,
		const uint8_t *expected, size_t expected_len, const char *label)
{
	size_t actual_len = 0;
	uint8_t buf[expected_len ? expected_len : 1];

	if (value.kind != JSVAL_KIND_BIGINT) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: expected bigint result", label);
	}
	if (jsval_bigint_copy_utf8(region, value, NULL, 0, &actual_len) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: failed to measure bigint result", label);
	}
	if (actual_len != expected_len) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: expected %zu bytes, got %zu",
				label, expected_len, actual_len);
	}
	if (jsval_bigint_copy_utf8(region, value, buf, actual_len, NULL) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: failed to copy bigint result", label);
	}
	if (memcmp(buf, expected, expected_len) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: bigint result did not match expected output", label);
	}
	return GENERATED_TEST_PASS;
}

int
main(void)
{
	uint8_t storage[8192];
	jsval_region_t region;
	jsval_t left;
	jsval_t right;
	jsval_t result;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_bigint_new_utf8(&region,
			(const uint8_t *)"12345678901234567890", 20, &left) == 0,
			SUITE, CASE_NAME, "failed to allocate left bigint");
	GENERATED_TEST_ASSERT(jsval_bigint_new_utf8(&region,
			(const uint8_t *)"9876543210", 10, &right) == 0,
			SUITE, CASE_NAME, "failed to allocate right bigint");

	GENERATED_TEST_ASSERT(jsval_add(&region, left, right, &result) == 0,
			SUITE, CASE_NAME, "bigint addition failed");
	GENERATED_TEST_ASSERT(expect_bigint(&region, result,
			(const uint8_t *)"12345678911111111100", 20, "left + right")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected bigint addition result");
	GENERATED_TEST_ASSERT(jsval_subtract(&region, left, right, &result) == 0,
			SUITE, CASE_NAME, "bigint subtraction failed");
	GENERATED_TEST_ASSERT(expect_bigint(&region, result,
			(const uint8_t *)"12345678891358024680", 20, "left - right")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected bigint subtraction result");
	GENERATED_TEST_ASSERT(jsval_multiply(&region, left, right, &result) == 0,
			SUITE, CASE_NAME, "bigint multiplication failed");
	GENERATED_TEST_ASSERT(expect_bigint(&region, result,
			(const uint8_t *)"121932631124828532111263526900", 30,
			"left * right") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected bigint multiplication result");
	GENERATED_TEST_ASSERT(jsval_unary_minus(&region, left, &result) == 0,
			SUITE, CASE_NAME, "bigint unary negation failed");
	GENERATED_TEST_ASSERT(expect_bigint(&region, result,
			(const uint8_t *)"-12345678901234567890", 21, "-left")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected bigint unary negation result");

	errno = 0;
	GENERATED_TEST_ASSERT(jsval_add(&region, left, jsval_number(1.0),
			&result) == -1 && errno == ENOTSUP,
			SUITE, CASE_NAME,
			"expected mixed Number/BigInt add to fail with ENOTSUP");

	return generated_test_pass(SUITE, CASE_NAME);
}
