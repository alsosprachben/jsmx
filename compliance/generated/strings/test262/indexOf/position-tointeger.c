#include <math.h>
#include <stdint.h>

#include "compliance/generated/string_search_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/indexOf/position-tointeger"

#define CHECK_POS(POS_VALUE, EXPECTED, LABEL) do { \
	GENERATED_TEST_ASSERT(jsmethod_string_index_of(&index, \
			jsmethod_value_string_utf8((const uint8_t *)"aaaa", 4), \
			jsmethod_value_string_utf8((const uint8_t *)"aa", 2), \
			1, (POS_VALUE), &error) == 0, SUITE, CASE_NAME, \
			"failed to lower %s", (LABEL)); \
	GENERATED_TEST_ASSERT(generated_expect_search_index(index, (EXPECTED), SUITE, \
			CASE_NAME, (LABEL)) == GENERATED_TEST_PASS, SUITE, CASE_NAME, \
			"unexpected result for %s", (LABEL)); \
} while (0)

int
main(void)
{
	jsmethod_error_t error;
	ssize_t index;

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/indexOf/position-tointeger.js
	 *
	 * This is an idiomatic slow-path translation. The upstream object-literal
	 * position cases (`[0]`, `["1"]`, `{}`, `[]`) are lowered through
	 * translator-emitted primitive results before reaching jsmethod.
	 */
	CHECK_POS(jsmethod_value_number(0.0), 0, "position 0");
	CHECK_POS(jsmethod_value_number(1.0), 1, "position 1");
	CHECK_POS(jsmethod_value_number(-0.9), 0, "position -0.9");
	CHECK_POS(jsmethod_value_number(0.9), 0, "position 0.9");
	CHECK_POS(jsmethod_value_number(1.9), 1, "position 1.9");
	CHECK_POS(jsmethod_value_number(NAN), 0, "position NaN");
	CHECK_POS(jsmethod_value_number(INFINITY), -1, "position Infinity");
	CHECK_POS(jsmethod_value_undefined(), 0, "position undefined");
	CHECK_POS(jsmethod_value_null(), 0, "position null");
	CHECK_POS(jsmethod_value_bool(0), 0, "position false");
	CHECK_POS(jsmethod_value_bool(1), 1, "position true");
	CHECK_POS(jsmethod_value_string_utf8((const uint8_t *)"0", 1), 0,
			"position \"0\"");
	CHECK_POS(jsmethod_value_string_utf8((const uint8_t *)"1.9", 3), 1,
			"position \"1.9\"");
	CHECK_POS(jsmethod_value_string_utf8((const uint8_t *)"Infinity", 8), -1,
			"position \"Infinity\"");
	CHECK_POS(jsmethod_value_string_utf8((const uint8_t *)"", 0), 0,
			"position \"\"");
	CHECK_POS(jsmethod_value_string_utf8((const uint8_t *)"foo", 3), 0,
			"position \"foo\"");
	CHECK_POS(jsmethod_value_string_utf8((const uint8_t *)"true", 4), 0,
			"position \"true\"");
	CHECK_POS(jsmethod_value_number(2.0), 2, "position 2");
	CHECK_POS(jsmethod_value_string_utf8((const uint8_t *)"2", 1), 2,
			"position \"2\"");
	CHECK_POS(jsmethod_value_number(2.9), 2, "position 2.9");
	CHECK_POS(jsmethod_value_string_utf8((const uint8_t *)"2.9", 3), 2,
			"position \"2.9\"");
	CHECK_POS(jsmethod_value_string_utf8((const uint8_t *)"0", 1), 0,
			"position [0]");
	CHECK_POS(jsmethod_value_string_utf8((const uint8_t *)"1", 1), 1,
			"position [\"1\"]");
	CHECK_POS(jsmethod_value_string_utf8((const uint8_t *)"[object Object]", 15),
			0, "position {}");
	CHECK_POS(jsmethod_value_string_utf8((const uint8_t *)"", 0), 0,
			"position []");
	return generated_test_pass(SUITE, CASE_NAME);
}

#undef CHECK_POS
