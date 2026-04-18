#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "wintertc"
#define CASE_NAME "jsmx/text-errors"

int
main(void)
{
	uint8_t storage[8192];
	jsval_region_t region;
	jsval_t out;

	jsval_region_init(&region, storage, sizeof(storage));

	/* encode with a non-string (number) returns -1. */
	GENERATED_TEST_ASSERT(
			jsval_text_encode_utf8(&region, jsval_number(42.0),
				&out) < 0,
			SUITE, CASE_NAME,
			"expected encode to fail on non-string input");

	/* decode with a non-buffer (number) returns -1. */
	GENERATED_TEST_ASSERT(
			jsval_text_decode_utf8(&region, jsval_number(42.0),
				&out) < 0,
			SUITE, CASE_NAME,
			"expected decode to fail on non-buffer input");

	/* decode with undefined returns -1. */
	GENERATED_TEST_ASSERT(
			jsval_text_decode_utf8(&region, jsval_undefined(),
				&out) < 0,
			SUITE, CASE_NAME,
			"expected decode to fail on undefined input");

	return generated_test_pass(SUITE, CASE_NAME);
}
