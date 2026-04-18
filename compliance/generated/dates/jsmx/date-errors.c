#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "dates"
#define CASE_NAME "jsmx/date-errors"

int
main(void)
{
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t bad_iso_input;
	jsval_t bad_date;
	jsval_t out;
	jsmethod_error_t error;
	int is_valid;

	jsval_region_init(&region, storage, sizeof(storage));

	/* Malformed ISO fails construction with -1 (jsmx's ISO parser
	 * is strict; spec allows either throw or Invalid Date — jsmx
	 * takes the former in its C API). */
	GENERATED_TEST_ASSERT(
			jsval_string_new_utf8(&region,
				(const uint8_t *)"not a date", 10,
				&bad_iso_input) == 0,
			SUITE, CASE_NAME, "bad ISO input setup");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(
			jsval_date_new_iso(&region, bad_iso_input, &bad_date,
				&error) < 0,
			SUITE, CASE_NAME,
			"new_iso on malformed should fail");

	/* NaN time value → invalid Date. */
	GENERATED_TEST_ASSERT(
			jsval_date_new_time(&region, jsval_number(0.0 / 0.0),
				&bad_date) == 0,
			SUITE, CASE_NAME, "new_time(NaN) should succeed");
	GENERATED_TEST_ASSERT(
			jsval_date_is_valid(&region, bad_date, &is_valid) == 0 &&
			is_valid == 0,
			SUITE, CASE_NAME,
			"NaN-time Date should have is_valid == 0");

	/* toISOString on invalid Date fails (spec: RangeError;
	 * jsmx: -1). */
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(
			jsval_date_to_iso_string(&region, bad_date, &out,
				&error) < 0,
			SUITE, CASE_NAME,
			"to_iso_string on invalid Date should fail");

	return generated_test_pass(SUITE, CASE_NAME);
}
