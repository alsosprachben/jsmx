#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "values"
#define CASE_NAME "jsmx/date-utc-parity"

static int
expect_number(jsval_t value, double expected, const char *label)
{
	if (value.kind != JSVAL_KIND_NUMBER || value.as.number != expected) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: expected %.17g", label, expected);
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
	jsval_t args[7];

	jsval_region_init(&region, storage, sizeof(storage));
	args[0] = jsval_number(99.0);
	args[1] = jsval_number(0.0);
	args[2] = jsval_number(2.0);
	args[3] = jsval_number(3.0);
	args[4] = jsval_number(4.0);
	args[5] = jsval_number(5.0);
	args[6] = jsval_number(6.0);

	GENERATED_TEST_ASSERT(jsval_date_utc(&region, 7, args, &result, NULL) == 0,
			SUITE, CASE_NAME, "failed to compute Date.UTC");
	GENERATED_TEST_ASSERT(expect_number(result, 915246245006.0, "Date.UTC")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected Date.UTC result");
	GENERATED_TEST_ASSERT(jsval_date_new_utc_fields(&region, 7, args, &date,
			NULL) == 0, SUITE, CASE_NAME,
			"failed to allocate UTC-fields date");
	GENERATED_TEST_ASSERT(jsval_date_get_utc_full_year(&region, date, &result)
			== 0, SUITE, CASE_NAME, "failed to read UTC full year");
	GENERATED_TEST_ASSERT(expect_number(result, 1999.0, "getUTCFullYear")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected UTC full year");
	GENERATED_TEST_ASSERT(jsval_date_get_utc_month(&region, date, &result) == 0,
			SUITE, CASE_NAME, "failed to read UTC month");
	GENERATED_TEST_ASSERT(expect_number(result, 0.0, "getUTCMonth")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected UTC month");
	GENERATED_TEST_ASSERT(jsval_date_get_utc_date(&region, date, &result) == 0,
			SUITE, CASE_NAME, "failed to read UTC date");
	GENERATED_TEST_ASSERT(expect_number(result, 2.0, "getUTCDate")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected UTC date");
	GENERATED_TEST_ASSERT(jsval_date_get_utc_day(&region, date, &result) == 0,
			SUITE, CASE_NAME, "failed to read UTC day");
	GENERATED_TEST_ASSERT(expect_number(result, 6.0, "getUTCDay")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected UTC day");

	GENERATED_TEST_ASSERT(jsval_date_set_utc_month(&region, date,
			jsval_number(1.0), &result) == 0,
			SUITE, CASE_NAME, "failed to set UTC month");
	GENERATED_TEST_ASSERT(jsval_date_get_utc_month(&region, date, &result) == 0,
			SUITE, CASE_NAME, "failed to re-read UTC month");
	GENERATED_TEST_ASSERT(expect_number(result, 1.0, "getUTCMonth(after set)")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected UTC month after set");
	GENERATED_TEST_ASSERT(jsval_date_set_utc_date(&region, date,
			jsval_number(29.0), &result) == 0,
			SUITE, CASE_NAME, "failed to set UTC date");
	GENERATED_TEST_ASSERT(jsval_date_get_utc_month(&region, date, &result) == 0,
			SUITE, CASE_NAME, "failed to read normalized UTC month");
	GENERATED_TEST_ASSERT(expect_number(result, 2.0, "normalized UTC month")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected normalized UTC month");
	GENERATED_TEST_ASSERT(jsval_date_get_utc_date(&region, date, &result) == 0,
			SUITE, CASE_NAME, "failed to read normalized UTC date");
	GENERATED_TEST_ASSERT(expect_number(result, 1.0, "normalized UTC date")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected normalized UTC date");

	return generated_test_pass(SUITE, CASE_NAME);
}
