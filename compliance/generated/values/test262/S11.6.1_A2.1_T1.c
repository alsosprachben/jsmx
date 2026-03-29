#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "values"
#define CASE_NAME "test262/S11.6.1_A2.1_T1"

static int
expect_number(jsval_t value, double expected, const char *label)
{
	if (value.kind != JSVAL_KIND_NUMBER || value.as.number != expected) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: expected numeric result %.17g", label, expected);
	}
	return GENERATED_TEST_PASS;
}

int
main(void)
{
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t sum;
	jsval_t x = jsval_number(1.0);
	jsval_t y = jsval_number(1.0);

	/*
	 * Generated from test262:
	 * test/language/expressions/addition/S11.6.1_A2.1_T1.js
	 *
	 * The original JS file is about GetValue on references. At the flattened
	 * `jsval` layer, those reference reads have already collapsed to numbers,
	 * so the preserved semantics are the numeric addition results themselves.
	 * The object-property reference case is intentionally omitted at this layer.
	 */

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(jsval_add(&region, jsval_number(1.0), jsval_number(1.0),
			&sum) == 0, SUITE, CASE_NAME,
			"1 + 1 should lower through jsval_add");
	GENERATED_TEST_ASSERT(expect_number(sum, 2.0, "1 + 1") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result for 1 + 1");

	GENERATED_TEST_ASSERT(jsval_add(&region, x, jsval_number(1.0), &sum) == 0,
			SUITE, CASE_NAME,
			"x + 1 should lower through jsval_add after the reference is read");
	GENERATED_TEST_ASSERT(expect_number(sum, 2.0, "x + 1") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result for x + 1");

	GENERATED_TEST_ASSERT(jsval_add(&region, jsval_number(1.0), y, &sum) == 0,
			SUITE, CASE_NAME,
			"1 + y should lower through jsval_add after the reference is read");
	GENERATED_TEST_ASSERT(expect_number(sum, 2.0, "1 + y") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result for 1 + y");

	GENERATED_TEST_ASSERT(jsval_add(&region, x, y, &sum) == 0, SUITE, CASE_NAME,
			"x + y should lower through jsval_add after both references are read");
	GENERATED_TEST_ASSERT(expect_number(sum, 2.0, "x + y") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result for x + y");

	return generated_test_pass(SUITE, CASE_NAME);
}
