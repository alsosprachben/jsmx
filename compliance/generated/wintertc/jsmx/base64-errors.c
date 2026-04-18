#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "wintertc"
#define CASE_NAME "jsmx/base64-errors"

int
main(void)
{
	uint8_t storage[8192];
	jsval_region_t region;
	jsval_t s;
	jsval_t out;

	jsval_region_init(&region, storage, sizeof(storage));

	/* Base64url character "-" rejected by standard decoder. */
	GENERATED_TEST_ASSERT(
			jsval_string_new_utf8(&region,
				(const uint8_t *)"ab-d", 4, &s) == 0,
			SUITE, CASE_NAME, "setup '-' input");
	GENERATED_TEST_ASSERT(
			jsval_base64_decode(&region, s, &out) < 0,
			SUITE, CASE_NAME, "expected '-' rejection");

	/* Base64url character "_" rejected by standard decoder. */
	GENERATED_TEST_ASSERT(
			jsval_string_new_utf8(&region,
				(const uint8_t *)"ab_d", 4, &s) == 0,
			SUITE, CASE_NAME, "setup '_' input");
	GENERATED_TEST_ASSERT(
			jsval_base64_decode(&region, s, &out) < 0,
			SUITE, CASE_NAME, "expected '_' rejection");

	/* Length not a multiple of 4 rejected. */
	GENERATED_TEST_ASSERT(
			jsval_string_new_utf8(&region,
				(const uint8_t *)"Zm9", 3, &s) == 0,
			SUITE, CASE_NAME, "setup short input");
	GENERATED_TEST_ASSERT(
			jsval_base64_decode(&region, s, &out) < 0,
			SUITE, CASE_NAME, "expected short-input rejection");

	/* Non-string input to decode is rejected. */
	GENERATED_TEST_ASSERT(
			jsval_base64_decode(&region, jsval_number(42.0), &out) < 0,
			SUITE, CASE_NAME, "expected non-string decode rejection");

	/* Non-buffer input to encode is rejected. */
	GENERATED_TEST_ASSERT(
			jsval_base64_encode(&region, jsval_number(42.0), &out) < 0,
			SUITE, CASE_NAME, "expected non-buffer encode rejection");

	return generated_test_pass(SUITE, CASE_NAME);
}
