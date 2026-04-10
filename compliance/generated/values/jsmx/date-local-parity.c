#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "values"
#define CASE_NAME "jsmx/date-local-parity"

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

	GENERATED_TEST_ASSERT(jsval_date_new_local_fields(&region, 7, args, &date,
			NULL) == 0, SUITE, CASE_NAME,
			"failed to allocate local-fields date");
	GENERATED_TEST_ASSERT(jsval_date_get_full_year(&region, date, &result) == 0,
			SUITE, CASE_NAME, "failed to read local full year");
	GENERATED_TEST_ASSERT(expect_number(result, 1999.0, "getFullYear")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected local full year");
	GENERATED_TEST_ASSERT(jsval_date_get_month(&region, date, &result) == 0,
			SUITE, CASE_NAME, "failed to read local month");
	GENERATED_TEST_ASSERT(expect_number(result, 0.0, "getMonth")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected local month");
	GENERATED_TEST_ASSERT(jsval_date_get_date(&region, date, &result) == 0,
			SUITE, CASE_NAME, "failed to read local date");
	GENERATED_TEST_ASSERT(expect_number(result, 2.0, "getDate")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected local date");
	GENERATED_TEST_ASSERT(jsval_date_get_hours(&region, date, &result) == 0,
			SUITE, CASE_NAME, "failed to read local hours");
	GENERATED_TEST_ASSERT(expect_number(result, 3.0, "getHours")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected local hours");
	GENERATED_TEST_ASSERT(jsval_date_get_minutes(&region, date, &result) == 0,
			SUITE, CASE_NAME, "failed to read local minutes");
	GENERATED_TEST_ASSERT(expect_number(result, 4.0, "getMinutes")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected local minutes");
	GENERATED_TEST_ASSERT(jsval_date_get_seconds(&region, date, &result) == 0,
			SUITE, CASE_NAME, "failed to read local seconds");
	GENERATED_TEST_ASSERT(expect_number(result, 5.0, "getSeconds")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected local seconds");
	GENERATED_TEST_ASSERT(jsval_date_get_milliseconds(&region, date, &result)
			== 0, SUITE, CASE_NAME, "failed to read local milliseconds");
	GENERATED_TEST_ASSERT(expect_number(result, 6.0, "getMilliseconds")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected local milliseconds");

	GENERATED_TEST_ASSERT(jsval_date_set_month(&region, date, jsval_number(1.0),
			&result) == 0, SUITE, CASE_NAME,
			"failed to set local month");
	GENERATED_TEST_ASSERT(jsval_date_set_hours(&region, date, jsval_number(23.0),
			&result) == 0, SUITE, CASE_NAME,
			"failed to set local hours");
	GENERATED_TEST_ASSERT(jsval_date_get_month(&region, date, &result) == 0,
			SUITE, CASE_NAME, "failed to re-read local month");
	GENERATED_TEST_ASSERT(expect_number(result, 1.0, "getMonth(after set)")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected local month after set");
	GENERATED_TEST_ASSERT(jsval_date_get_hours(&region, date, &result) == 0,
			SUITE, CASE_NAME, "failed to re-read local hours");
	GENERATED_TEST_ASSERT(expect_number(result, 23.0, "getHours(after set)")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected local hours after set");

	return generated_test_pass(SUITE, CASE_NAME);
}
