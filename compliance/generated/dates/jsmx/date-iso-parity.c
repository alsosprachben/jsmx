#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "dates"
#define CASE_NAME "jsmx/date-iso-parity"

static int
expect_string(jsval_region_t *region, jsval_t value, const char *expected,
		const char *label)
{
	size_t len = 0;
	size_t expected_len = strlen(expected);
	uint8_t buf[64];

	if (value.kind != JSVAL_KIND_STRING) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: not a string", label);
	}
	if (jsval_string_copy_utf8(region, value, NULL, 0, &len) != 0 ||
			len != expected_len || len >= sizeof(buf)) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: len %zu != %zu", label, len, expected_len);
	}
	if (jsval_string_copy_utf8(region, value, buf, len, NULL) != 0 ||
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
	jsval_t iso_input;
	jsval_t date;
	jsval_t serialized;
	jsmethod_error_t error;
	const char *canonical = "2024-01-01T12:30:45.500Z";

	jsval_region_init(&region, storage, sizeof(storage));

	/* Construct from canonical ISO string. */
	GENERATED_TEST_ASSERT(
			jsval_string_new_utf8(&region,
				(const uint8_t *)canonical, strlen(canonical),
				&iso_input) == 0,
			SUITE, CASE_NAME, "canonical input setup");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(
			jsval_date_new_iso(&region, iso_input, &date, &error) == 0,
			SUITE, CASE_NAME, "new_iso failed");

	/* toISOString round-trips the canonical form. */
	GENERATED_TEST_ASSERT(
			jsval_date_to_iso_string(&region, date, &serialized, &error) == 0,
			SUITE, CASE_NAME, "to_iso_string failed");
	GENERATED_TEST_ASSERT(
			expect_string(&region, serialized, canonical,
				"toISOString from ISO") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "ISO round-trip mismatch");

	/* toJSON is equivalent to toISOString per spec. */
	GENERATED_TEST_ASSERT(
			jsval_date_to_json(&region, date, &serialized, &error) == 0,
			SUITE, CASE_NAME, "to_json failed");
	GENERATED_TEST_ASSERT(
			expect_string(&region, serialized, canonical,
				"toJSON") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "toJSON mismatch");

	/* Construct from time value; toISOString matches canonical. */
	GENERATED_TEST_ASSERT(
			jsval_date_new_time(&region,
				jsval_number(1704112245500.0), &date) == 0,
			SUITE, CASE_NAME, "new_time failed");
	GENERATED_TEST_ASSERT(
			jsval_date_to_iso_string(&region, date, &serialized, &error) == 0,
			SUITE, CASE_NAME, "to_iso_string after new_time failed");
	GENERATED_TEST_ASSERT(
			expect_string(&region, serialized, canonical,
				"toISOString from time") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "time-value to ISO mismatch");

	return generated_test_pass(SUITE, CASE_NAME);
}
