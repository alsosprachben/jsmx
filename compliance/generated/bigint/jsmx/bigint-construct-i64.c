#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "bigint"
#define CASE_NAME "jsmx/bigint-construct-i64"

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

static int
expect_typeof_bigint(jsval_region_t *region, jsval_t value,
		const char *label)
{
	jsval_t type_value;
	size_t len = 0;
	uint8_t buf[16];

	if (jsval_typeof(region, value, &type_value) != 0 ||
			type_value.kind != JSVAL_KIND_STRING) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: typeof not a string", label);
	}
	if (jsval_string_copy_utf8(region, type_value, NULL, 0, &len) != 0 ||
			len != 6 || len >= sizeof(buf)) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: typeof len != 6", label);
	}
	if (jsval_string_copy_utf8(region, type_value, buf, len, NULL) != 0 ||
			memcmp(buf, "bigint", 6) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: typeof != 'bigint'", label);
	}
	return GENERATED_TEST_PASS;
}

int
main(void)
{
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t zero;
	jsval_t forty_two;
	jsval_t i64_min;
	jsval_t u64_max;

	jsval_region_init(&region, storage, sizeof(storage));

	/* Zero: falsy, kind == BIGINT, serializes to "0",
	 * typeof "bigint". */
	GENERATED_TEST_ASSERT(
			jsval_bigint_new_i64(&region, 0, &zero) == 0,
			SUITE, CASE_NAME, "zero construct");
	GENERATED_TEST_ASSERT(jsval_truthy(&region, zero) == 0,
			SUITE, CASE_NAME, "zero should be falsy");
	GENERATED_TEST_ASSERT(
			expect_bigint_utf8(&region, zero, "0", "zero")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "zero serialization");
	GENERATED_TEST_ASSERT(
			expect_typeof_bigint(&region, zero, "zero")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "zero typeof");

	/* Positive small. */
	GENERATED_TEST_ASSERT(
			jsval_bigint_new_i64(&region, 42, &forty_two) == 0 &&
			expect_bigint_utf8(&region, forty_two, "42", "42")
				== GENERATED_TEST_PASS &&
			jsval_truthy(&region, forty_two) == 1,
			SUITE, CASE_NAME, "42 construct");

	/* INT64_MIN: full 64-bit signed, beyond Number safe range. */
	GENERATED_TEST_ASSERT(
			jsval_bigint_new_i64(&region, INT64_MIN, &i64_min) == 0 &&
			expect_bigint_utf8(&region, i64_min,
				"-9223372036854775808", "INT64_MIN")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "INT64_MIN construct");

	/* UINT64_MAX: 20-digit decimal, well beyond int64 range. */
	GENERATED_TEST_ASSERT(
			jsval_bigint_new_u64(&region, UINT64_MAX, &u64_max) == 0 &&
			expect_bigint_utf8(&region, u64_max,
				"18446744073709551615", "UINT64_MAX")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "UINT64_MAX construct");

	return generated_test_pass(SUITE, CASE_NAME);
}
