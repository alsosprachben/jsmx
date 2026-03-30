#include <math.h>
#include <stdint.h>

#include "compliance/generated/string_search_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/includes/coerced-values-of-position"

#define CHECK_BOOL(POS_VALUE, EXPECTED, LABEL) do { \
	GENERATED_TEST_ASSERT(jsmethod_string_includes(&result, \
			jsmethod_value_string_utf8((const uint8_t *)str, sizeof(str) - 1), \
			jsmethod_value_string_utf8((const uint8_t *)"The future", 10), \
			1, (POS_VALUE), &error) == 0, SUITE, CASE_NAME, \
			"failed to lower %s", (LABEL)); \
	GENERATED_TEST_ASSERT(generated_expect_search_bool(result, (EXPECTED), SUITE, \
			CASE_NAME, (LABEL)) == GENERATED_TEST_PASS, SUITE, CASE_NAME, \
			"unexpected result for %s", (LABEL)); \
} while (0)

int
main(void)
{
	jsmethod_error_t error;
	int result;
	static const char str[] = "The future is cool!";

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/includes/coerced-values-of-position.js
	 */
	CHECK_BOOL(jsmethod_value_number(NAN), 1, "position NaN");
	CHECK_BOOL(jsmethod_value_null(), 1, "position null");
	CHECK_BOOL(jsmethod_value_bool(0), 1, "position false");
	CHECK_BOOL(jsmethod_value_string_utf8((const uint8_t *)"", 0), 1,
			"position \"\"");
	CHECK_BOOL(jsmethod_value_string_utf8((const uint8_t *)"0", 1), 1,
			"position \"0\"");
	CHECK_BOOL(jsmethod_value_undefined(), 1, "position undefined");
	CHECK_BOOL(jsmethod_value_number(0.4), 1, "position 0.4");
	CHECK_BOOL(jsmethod_value_number(-1.0), 1, "position -1");
	CHECK_BOOL(jsmethod_value_bool(1), 0, "position true");
	CHECK_BOOL(jsmethod_value_string_utf8((const uint8_t *)"1", 1), 0,
			"position \"1\"");
	CHECK_BOOL(jsmethod_value_number(1.4), 0, "position 1.4");
	return generated_test_pass(SUITE, CASE_NAME);
}

#undef CHECK_BOOL
