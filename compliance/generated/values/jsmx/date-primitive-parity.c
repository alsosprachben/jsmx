#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "values"
#define CASE_NAME "jsmx/date-primitive-parity"

static int
expect_string(jsval_region_t *region, jsval_t value,
		const uint8_t *expected, size_t expected_len, const char *label)
{
	size_t actual_len = 0;
	uint8_t buf[expected_len ? expected_len : 1];

	if (jsval_string_copy_utf8(region, value, NULL, 0, &actual_len) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: failed to measure string", label);
	}
	if (actual_len != expected_len) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: expected %zu bytes, got %zu",
				label, expected_len, actual_len);
	}
	if (jsval_string_copy_utf8(region, value, buf, actual_len, NULL) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: failed to copy string", label);
	}
	if (memcmp(buf, expected, expected_len) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: unexpected string result", label);
	}
	return GENERATED_TEST_PASS;
}

int
main(void)
{
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t date;
	jsval_t alias;
	jsval_t other;
	jsval_t result;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_date_new_time(&region,
			jsval_number(1577934245006.0), &date) == 0,
			SUITE, CASE_NAME, "failed to allocate date");
	alias = date;
	GENERATED_TEST_ASSERT(jsval_date_new_time(&region,
			jsval_number(1577934245006.0), &other) == 0,
			SUITE, CASE_NAME, "failed to allocate other date");

	GENERATED_TEST_ASSERT(jsval_typeof(&region, date, &result) == 0,
			SUITE, CASE_NAME, "failed to typeof date");
	GENERATED_TEST_ASSERT(expect_string(&region, result,
			(const uint8_t *)"object", 6, "typeof date")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected typeof date result");
	GENERATED_TEST_ASSERT(jsval_truthy(&region, date) == 1,
			SUITE, CASE_NAME, "expected date to be truthy");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, date, alias) == 1,
			SUITE, CASE_NAME, "expected alias to preserve date identity");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, date, other) == 0,
			SUITE, CASE_NAME, "expected distinct dates to stay distinct");

	GENERATED_TEST_ASSERT(jsval_date_get_time(&region, date, &result) == 0,
			SUITE, CASE_NAME, "failed to get date time");
	GENERATED_TEST_ASSERT(result.kind == JSVAL_KIND_NUMBER
			&& result.as.number == 1577934245006.0,
			SUITE, CASE_NAME, "unexpected getTime result");
	GENERATED_TEST_ASSERT(jsval_date_value_of(&region, date, &result) == 0,
			SUITE, CASE_NAME, "failed to read date valueOf");
	GENERATED_TEST_ASSERT(result.kind == JSVAL_KIND_NUMBER
			&& result.as.number == 1577934245006.0,
			SUITE, CASE_NAME, "unexpected valueOf result");

	return generated_test_pass(SUITE, CASE_NAME);
}
