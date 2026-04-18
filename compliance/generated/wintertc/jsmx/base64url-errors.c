#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "wintertc"
#define CASE_NAME "jsmx/base64url-errors"

int
main(void)
{
	uint8_t storage[8192];
	jsval_region_t region;
	jsval_t s;
	jsval_t out;

	jsval_region_init(&region, storage, sizeof(storage));

	/* Standard-alphabet "+" rejected by base64url decoder. */
	GENERATED_TEST_ASSERT(
			jsval_string_new_utf8(&region,
				(const uint8_t *)"ab+d", 4, &s) == 0,
			SUITE, CASE_NAME, "setup '+' input");
	GENERATED_TEST_ASSERT(
			jsval_base64url_decode(&region, s, &out) < 0,
			SUITE, CASE_NAME, "expected '+' rejection");

	/* Standard-alphabet "/" rejected by base64url decoder. */
	GENERATED_TEST_ASSERT(
			jsval_string_new_utf8(&region,
				(const uint8_t *)"ab/d", 4, &s) == 0,
			SUITE, CASE_NAME, "setup '/' input");
	GENERATED_TEST_ASSERT(
			jsval_base64url_decode(&region, s, &out) < 0,
			SUITE, CASE_NAME, "expected '/' rejection");

	/* Length-mod-4 == 1 is never a valid base64 encoding. */
	GENERATED_TEST_ASSERT(
			jsval_string_new_utf8(&region,
				(const uint8_t *)"a", 1, &s) == 0,
			SUITE, CASE_NAME, "setup 1-char input");
	GENERATED_TEST_ASSERT(
			jsval_base64url_decode(&region, s, &out) < 0,
			SUITE, CASE_NAME, "expected 1-char rejection");

	/* Non-string input rejected. */
	GENERATED_TEST_ASSERT(
			jsval_base64url_decode(&region, jsval_number(42.0), &out) < 0,
			SUITE, CASE_NAME, "expected non-string rejection");

	/* Non-buffer input to encode rejected. */
	GENERATED_TEST_ASSERT(
			jsval_base64url_encode(&region, jsval_number(42.0), &out) < 0,
			SUITE, CASE_NAME, "expected non-buffer encode rejection");

	return generated_test_pass(SUITE, CASE_NAME);
}
