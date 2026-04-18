#include <errno.h>
#include <math.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "json"
#define CASE_NAME "jsmx/json-errors"

int
main(void)
{
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t parsed;
	jsval_t cycle_object;

	jsval_region_init(&region, storage, sizeof(storage));

	/* Parse malformed input: "{" (open brace, unterminated). */
	GENERATED_TEST_ASSERT(
			jsval_json_parse(&region, (const uint8_t *)"{", 1, 8,
				&parsed) < 0,
			SUITE, CASE_NAME,
			"expected parse('{') to fail");

	/* Parse unterminated string: "\"hello" (missing close quote). */
	GENERATED_TEST_ASSERT(
			jsval_json_parse(&region, (const uint8_t *)"\"hello", 6, 8,
				&parsed) < 0,
			SUITE, CASE_NAME,
			"expected parse('\"hello') to fail");

	/* Serialize undefined: ENOTSUP. */
	errno = 0;
	GENERATED_TEST_ASSERT(
			jsval_copy_json(&region, jsval_undefined(), NULL, 0,
				NULL) < 0,
			SUITE, CASE_NAME,
			"expected serialize(undefined) to fail");
	GENERATED_TEST_ASSERT(errno == ENOTSUP,
			SUITE, CASE_NAME,
			"expected errno ENOTSUP for undefined, got %d", errno);

	/* Serialize NaN: ENOTSUP. */
	errno = 0;
	GENERATED_TEST_ASSERT(
			jsval_copy_json(&region, jsval_number(NAN), NULL, 0,
				NULL) < 0,
			SUITE, CASE_NAME, "expected serialize(NaN) to fail");
	GENERATED_TEST_ASSERT(errno == ENOTSUP,
			SUITE, CASE_NAME,
			"expected errno ENOTSUP for NaN, got %d", errno);

	/* Serialize circular reference: ELOOP. */
	GENERATED_TEST_ASSERT(
			jsval_object_new(&region, 1, &cycle_object) == 0 &&
			jsval_object_set_utf8(&region, cycle_object,
				(const uint8_t *)"self", 4, cycle_object) == 0,
			SUITE, CASE_NAME, "build cycle object");
	errno = 0;
	GENERATED_TEST_ASSERT(
			jsval_copy_json(&region, cycle_object, NULL, 0, NULL) < 0,
			SUITE, CASE_NAME, "expected serialize(cycle) to fail");
	GENERATED_TEST_ASSERT(errno == ELOOP,
			SUITE, CASE_NAME,
			"expected errno ELOOP for cycle, got %d", errno);

	return generated_test_pass(SUITE, CASE_NAME);
}
