#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "dates"
#define CASE_NAME "jsmx/date-construct-parity"

int
main(void)
{
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t date;
	jsval_t result;
	jsval_t iso_input;
	jsval_t fields[7];
	jsmethod_error_t error;
	int is_valid;

	jsval_region_init(&region, storage, sizeof(storage));

	/* Unix epoch via time value. */
	GENERATED_TEST_ASSERT(
			jsval_date_new_time(&region, jsval_number(0.0), &date) == 0,
			SUITE, CASE_NAME, "new_time(0) failed");
	GENERATED_TEST_ASSERT(
			jsval_date_is_valid(&region, date, &is_valid) == 0 &&
			is_valid == 1,
			SUITE, CASE_NAME, "epoch-via-time not valid");
	GENERATED_TEST_ASSERT(
			jsval_date_get_time(&region, date, &result) == 0 &&
			result.kind == JSVAL_KIND_NUMBER &&
			result.as.number == 0.0,
			SUITE, CASE_NAME, "epoch-via-time: time != 0");

	/* Unix epoch via ISO. */
	GENERATED_TEST_ASSERT(
			jsval_string_new_utf8(&region,
				(const uint8_t *)"1970-01-01T00:00:00.000Z", 24,
				&iso_input) == 0,
			SUITE, CASE_NAME, "ISO input setup");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(
			jsval_date_new_iso(&region, iso_input, &date, &error) == 0,
			SUITE, CASE_NAME, "new_iso epoch failed");
	GENERATED_TEST_ASSERT(
			jsval_date_is_valid(&region, date, &is_valid) == 0 &&
			is_valid == 1,
			SUITE, CASE_NAME, "epoch-via-iso not valid");
	GENERATED_TEST_ASSERT(
			jsval_date_get_time(&region, date, &result) == 0 &&
			result.as.number == 0.0,
			SUITE, CASE_NAME, "epoch-via-iso: time != 0");

	/* UTC fields: 2024-01-01T12:30:45.500Z */
	fields[0] = jsval_number(2024.0);
	fields[1] = jsval_number(0.0);    /* month (Jan = 0) */
	fields[2] = jsval_number(1.0);    /* day */
	fields[3] = jsval_number(12.0);   /* hours */
	fields[4] = jsval_number(30.0);   /* minutes */
	fields[5] = jsval_number(45.0);   /* seconds */
	fields[6] = jsval_number(500.0);  /* ms */
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(
			jsval_date_new_utc_fields(&region, 7, fields, &date,
				&error) == 0,
			SUITE, CASE_NAME, "new_utc_fields failed");
	GENERATED_TEST_ASSERT(
			jsval_date_is_valid(&region, date, &is_valid) == 0 &&
			is_valid == 1,
			SUITE, CASE_NAME, "fields Date not valid");
	GENERATED_TEST_ASSERT(
			jsval_date_get_time(&region, date, &result) == 0 &&
			result.kind == JSVAL_KIND_NUMBER &&
			result.as.number == 1704112245500.0,
			SUITE, CASE_NAME,
			"fields Date: time != 1704112245500 (got %.0f)",
			result.as.number);

	return generated_test_pass(SUITE, CASE_NAME);
}
