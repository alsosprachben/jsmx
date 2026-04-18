#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "dates"
#define CASE_NAME "jsmx/date-accessors-parity"

static int
expect_number(jsval_region_t *region, int (*getter)(jsval_region_t *,
		jsval_t, jsval_t *), jsval_t date, double expected,
		const char *label)
{
	jsval_t out;

	if (getter(region, date, &out) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: getter returned -1", label);
	}
	if (out.kind != JSVAL_KIND_NUMBER || out.as.number != expected) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: got %.0f, want %.0f", label,
				out.as.number, expected);
	}
	return GENERATED_TEST_PASS;
}

int
main(void)
{
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t date;
	jsval_t roundtrip;
	jsval_t year_setter_out;

	jsval_region_init(&region, storage, sizeof(storage));

	/* 2024-01-01T12:30:45.500Z */
	GENERATED_TEST_ASSERT(
			jsval_date_new_time(&region,
				jsval_number(1704112245500.0), &date) == 0,
			SUITE, CASE_NAME, "new_time failed");

	GENERATED_TEST_ASSERT(
			expect_number(&region, jsval_date_get_utc_full_year,
				date, 2024.0, "utc_full_year")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "utc_full_year check");
	GENERATED_TEST_ASSERT(
			expect_number(&region, jsval_date_get_utc_month,
				date, 0.0, "utc_month") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "utc_month check");
	GENERATED_TEST_ASSERT(
			expect_number(&region, jsval_date_get_utc_date,
				date, 1.0, "utc_date") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "utc_date check");
	GENERATED_TEST_ASSERT(
			expect_number(&region, jsval_date_get_utc_day,
				date, 1.0, "utc_day") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "utc_day check (Monday)");
	GENERATED_TEST_ASSERT(
			expect_number(&region, jsval_date_get_utc_hours,
				date, 12.0, "utc_hours") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "utc_hours check");
	GENERATED_TEST_ASSERT(
			expect_number(&region, jsval_date_get_utc_minutes,
				date, 30.0, "utc_minutes") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "utc_minutes check");
	GENERATED_TEST_ASSERT(
			expect_number(&region, jsval_date_get_utc_seconds,
				date, 45.0, "utc_seconds") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "utc_seconds check");
	GENERATED_TEST_ASSERT(
			expect_number(&region, jsval_date_get_utc_milliseconds,
				date, 500.0, "utc_milliseconds")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "utc_milliseconds check");

	/* Setter round-trip: setUTCFullYear(2025) → getUTCFullYear() == 2025. */
	GENERATED_TEST_ASSERT(
			jsval_date_set_utc_full_year(&region, date,
				jsval_number(2025.0), &year_setter_out) == 0,
			SUITE, CASE_NAME, "set_utc_full_year failed");
	GENERATED_TEST_ASSERT(
			jsval_date_get_utc_full_year(&region, date,
				&roundtrip) == 0 &&
			roundtrip.kind == JSVAL_KIND_NUMBER &&
			roundtrip.as.number == 2025.0,
			SUITE, CASE_NAME,
			"post-set year != 2025 (got %.0f)", roundtrip.as.number);

	return generated_test_pass(SUITE, CASE_NAME);
}
