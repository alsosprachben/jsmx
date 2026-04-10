#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "values"
#define CASE_NAME "jsmx/date-iso-parse-stringify-parity"

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
	jsval_t input;
	jsval_t date;
	jsval_t result;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"2020-01-02T03:04:05.006Z",
			sizeof("2020-01-02T03:04:05.006Z") - 1, &input) == 0,
			SUITE, CASE_NAME, "failed to allocate ISO input");
	GENERATED_TEST_ASSERT(jsval_date_parse_iso(&region, input, &result, NULL)
			== 0, SUITE, CASE_NAME, "failed to parse ISO text");
	GENERATED_TEST_ASSERT(result.kind == JSVAL_KIND_NUMBER
			&& result.as.number == 1577934245006.0,
			SUITE, CASE_NAME, "unexpected Date.parse result");
	GENERATED_TEST_ASSERT(jsval_date_new_iso(&region, input, &date, NULL) == 0,
			SUITE, CASE_NAME, "failed to allocate ISO date");
	GENERATED_TEST_ASSERT(jsval_date_get_time(&region, date, &result) == 0,
			SUITE, CASE_NAME, "failed to read ISO date time");
	GENERATED_TEST_ASSERT(result.kind == JSVAL_KIND_NUMBER
			&& result.as.number == 1577934245006.0,
			SUITE, CASE_NAME, "unexpected ISO date time");
	GENERATED_TEST_ASSERT(jsval_date_to_iso_string(&region, date, &result, NULL)
			== 0, SUITE, CASE_NAME, "failed to stringify ISO date");
	GENERATED_TEST_ASSERT(expect_string(&region, result,
			(const uint8_t *)"2020-01-02T03:04:05.006Z",
			sizeof("2020-01-02T03:04:05.006Z") - 1, "toISOString")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected ISO string result");
	GENERATED_TEST_ASSERT(jsval_date_to_json(&region, date, &result, NULL) == 0,
			SUITE, CASE_NAME, "failed to stringify Date JSON");
	GENERATED_TEST_ASSERT(expect_string(&region, result,
			(const uint8_t *)"2020-01-02T03:04:05.006Z",
			sizeof("2020-01-02T03:04:05.006Z") - 1, "toJSON")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected Date JSON result");
	GENERATED_TEST_ASSERT(jsval_date_to_utc_string(&region, date, &result) == 0,
			SUITE, CASE_NAME, "failed to stringify UTC string");
	GENERATED_TEST_ASSERT(expect_string(&region, result,
			(const uint8_t *)"Thu, 02 Jan 2020 03:04:05 GMT",
			sizeof("Thu, 02 Jan 2020 03:04:05 GMT") - 1, "toUTCString")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected UTC string result");

	return generated_test_pass(SUITE, CASE_NAME);
}
