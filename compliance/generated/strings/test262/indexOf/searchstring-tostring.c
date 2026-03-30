#include <math.h>
#include <stdint.h>

#include "compliance/generated/string_search_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/indexOf/searchstring-tostring"

#define CHECK_INDEX(HAY_BYTES, HAY_LEN, NEEDLE_VALUE, EXPECTED, LABEL) do { \
	GENERATED_TEST_ASSERT(jsmethod_string_index_of(&index, \
			jsmethod_value_string_utf8((const uint8_t *)(HAY_BYTES), (HAY_LEN)), \
			(NEEDLE_VALUE), 0, jsmethod_value_undefined(), &error) == 0, \
			SUITE, CASE_NAME, "failed to lower %s", (LABEL)); \
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
	 * test/built-ins/String/prototype/indexOf/searchstring-tostring.js
	 *
	 * This is an idiomatic slow-path translation. The upstream object-literal
	 * searchString cases (`[]`, `["foo","bar"]`, `{}`) are lowered through
	 * translator-known primitive ToString results outside jsmx, then dispatched
	 * through the thin jsmethod search layer.
	 */
	CHECK_INDEX("foo", 3, jsmethod_value_string_utf8((const uint8_t *)"", 0),
			0, "\"foo\".indexOf(\"\")");
	CHECK_INDEX("__foo__", 7,
			jsmethod_value_string_utf8((const uint8_t *)"foo", 3),
			2, "\"__foo__\".indexOf(\"foo\")");
	CHECK_INDEX("__undefined__", 13, jsmethod_value_undefined(),
			2, "\"__undefined__\".indexOf(undefined)");
	CHECK_INDEX("__null__", 8, jsmethod_value_null(),
			2, "\"__null__\".indexOf(null)");
	CHECK_INDEX("__true__", 8, jsmethod_value_bool(1),
			2, "\"__true__\".indexOf(true)");
	CHECK_INDEX("__false__", 9, jsmethod_value_bool(0),
			2, "\"__false__\".indexOf(false)");
	CHECK_INDEX("__0__", 5, jsmethod_value_number(0.0),
			2, "\"__0__\".indexOf(0)");
	CHECK_INDEX("__0__", 5, jsmethod_value_number(-0.0),
			2, "\"__0__\".indexOf(-0)");
	CHECK_INDEX("__Infinity__", 12, jsmethod_value_number(INFINITY),
			2, "\"__Infinity__\".indexOf(Infinity)");
	CHECK_INDEX("__-Infinity__", 13, jsmethod_value_number(-INFINITY),
			2, "\"__-Infinity__\".indexOf(-Infinity)");
	CHECK_INDEX("__NaN__", 7, jsmethod_value_number(NAN),
			2, "\"__NaN__\".indexOf(NaN)");
	CHECK_INDEX("__123.456__", 11, jsmethod_value_number(123.456),
			2, "\"__123.456__\".indexOf(123.456)");
	CHECK_INDEX("__-123.456__", 12, jsmethod_value_number(-123.456),
			2, "\"__-123.456__\".indexOf(-123.456)");
	CHECK_INDEX("foo", 3, jsmethod_value_string_utf8((const uint8_t *)"", 0),
			0, "\"foo\".indexOf([])");
	CHECK_INDEX("__foo,bar__", 11,
			jsmethod_value_string_utf8((const uint8_t *)"foo,bar", 7),
			2, "\"__foo,bar__\".indexOf([\"foo\",\"bar\"])");
	CHECK_INDEX("__[object Object]__", 19,
			jsmethod_value_string_utf8(
				(const uint8_t *)"[object Object]", 15),
			2, "\"__[object Object]__\".indexOf({})");
	return generated_test_pass(SUITE, CASE_NAME);
}

#undef CHECK_INDEX
