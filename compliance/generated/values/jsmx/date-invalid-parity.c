#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "values"
#define CASE_NAME "jsmx/date-invalid-parity"

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
	jsval_t result;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_date_new_time(&region, jsval_number(NAN), &date)
			== 0, SUITE, CASE_NAME, "failed to allocate invalid date");
	GENERATED_TEST_ASSERT(jsval_date_get_time(&region, date, &result) == 0,
			SUITE, CASE_NAME, "failed to read invalid date time");
	GENERATED_TEST_ASSERT(result.kind == JSVAL_KIND_NUMBER
			&& isnan(result.as.number), SUITE, CASE_NAME,
			"expected invalid date getTime to stay NaN");
	GENERATED_TEST_ASSERT(jsval_date_value_of(&region, date, &result) == 0,
			SUITE, CASE_NAME, "failed to read invalid date valueOf");
	GENERATED_TEST_ASSERT(result.kind == JSVAL_KIND_NUMBER
			&& isnan(result.as.number), SUITE, CASE_NAME,
			"expected invalid date valueOf to stay NaN");
	GENERATED_TEST_ASSERT(jsval_date_to_string(&region, date, &result) == 0,
			SUITE, CASE_NAME, "failed to stringify invalid local date");
	GENERATED_TEST_ASSERT(expect_string(&region, result,
			(const uint8_t *)"Invalid Date",
			sizeof("Invalid Date") - 1, "toString")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected invalid local date string");
	GENERATED_TEST_ASSERT(jsval_date_to_utc_string(&region, date, &result) == 0,
			SUITE, CASE_NAME, "failed to stringify invalid UTC date");
	GENERATED_TEST_ASSERT(expect_string(&region, result,
			(const uint8_t *)"Invalid Date",
			sizeof("Invalid Date") - 1, "toUTCString")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected invalid UTC date string");
	GENERATED_TEST_ASSERT(jsval_date_to_json(&region, date, &result, NULL) == 0,
			SUITE, CASE_NAME, "failed to stringify invalid Date JSON");
	GENERATED_TEST_ASSERT(result.kind == JSVAL_KIND_NULL, SUITE, CASE_NAME,
			"expected invalid Date JSON result null");
	memset(&error, 0, sizeof(error));
	errno = 0;
	GENERATED_TEST_ASSERT(jsval_date_to_iso_string(&region, date, &result, &error)
			< 0 && errno == EINVAL && error.kind == JSMETHOD_ERROR_RANGE,
			SUITE, CASE_NAME,
			"expected invalid Date toISOString to fail with range error");

	return generated_test_pass(SUITE, CASE_NAME);
}
