#include <math.h>
        #include <stdint.h>

        #include "compliance/generated/string_pad_helpers.h"

        #define SUITE "strings"
        #define CASE_NAME "test262/padStart/max-length-not-greater-than-string"

        int
        main(void)
        {
        	uint16_t storage[8];
        	jsmethod_error_t error;
        	jsstr16_t out;

        	GENERATED_TEST_ASSERT(generated_measure_and_pad(&out, storage,
			GENERATED_LEN(storage),
			jsmethod_value_string_utf8((const uint8_t *)"abc", 3), 1,
			jsmethod_value_undefined(), 1,
			jsmethod_value_string_utf8((const uint8_t *)"def", 3),
			jsmethod_string_pad_start_measure, jsmethod_string_pad_start, &error) == 0,
			SUITE, CASE_NAME, "failed maxLength undefined");
	GENERATED_TEST_ASSERT(generated_expect_utf8_string(&out,
			"abc", SUITE, CASE_NAME,
			"maxLength undefined") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected maxLength undefined result");

	GENERATED_TEST_ASSERT(generated_measure_and_pad(&out, storage,
			GENERATED_LEN(storage),
			jsmethod_value_string_utf8((const uint8_t *)"abc", 3), 1,
			jsmethod_value_null(), 1,
			jsmethod_value_string_utf8((const uint8_t *)"def", 3),
			jsmethod_string_pad_start_measure, jsmethod_string_pad_start, &error) == 0,
			SUITE, CASE_NAME, "failed maxLength null");
	GENERATED_TEST_ASSERT(generated_expect_utf8_string(&out,
			"abc", SUITE, CASE_NAME,
			"maxLength null") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected maxLength null result");

	GENERATED_TEST_ASSERT(generated_measure_and_pad(&out, storage,
			GENERATED_LEN(storage),
			jsmethod_value_string_utf8((const uint8_t *)"abc", 3), 1,
			jsmethod_value_number(NAN), 1,
			jsmethod_value_string_utf8((const uint8_t *)"def", 3),
			jsmethod_string_pad_start_measure, jsmethod_string_pad_start, &error) == 0,
			SUITE, CASE_NAME, "failed maxLength NaN");
	GENERATED_TEST_ASSERT(generated_expect_utf8_string(&out,
			"abc", SUITE, CASE_NAME,
			"maxLength NaN") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected maxLength NaN result");

	GENERATED_TEST_ASSERT(generated_measure_and_pad(&out, storage,
			GENERATED_LEN(storage),
			jsmethod_value_string_utf8((const uint8_t *)"abc", 3), 1,
			jsmethod_value_number(-INFINITY), 1,
			jsmethod_value_string_utf8((const uint8_t *)"def", 3),
			jsmethod_string_pad_start_measure, jsmethod_string_pad_start, &error) == 0,
			SUITE, CASE_NAME, "failed maxLength -Infinity");
	GENERATED_TEST_ASSERT(generated_expect_utf8_string(&out,
			"abc", SUITE, CASE_NAME,
			"maxLength -Infinity") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected maxLength -Infinity result");

	GENERATED_TEST_ASSERT(generated_measure_and_pad(&out, storage,
			GENERATED_LEN(storage),
			jsmethod_value_string_utf8((const uint8_t *)"abc", 3), 1,
			jsmethod_value_number(0.0), 1,
			jsmethod_value_string_utf8((const uint8_t *)"def", 3),
			jsmethod_string_pad_start_measure, jsmethod_string_pad_start, &error) == 0,
			SUITE, CASE_NAME, "failed maxLength 0");
	GENERATED_TEST_ASSERT(generated_expect_utf8_string(&out,
			"abc", SUITE, CASE_NAME,
			"maxLength 0") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected maxLength 0 result");

	GENERATED_TEST_ASSERT(generated_measure_and_pad(&out, storage,
			GENERATED_LEN(storage),
			jsmethod_value_string_utf8((const uint8_t *)"abc", 3), 1,
			jsmethod_value_number(-1.0), 1,
			jsmethod_value_string_utf8((const uint8_t *)"def", 3),
			jsmethod_string_pad_start_measure, jsmethod_string_pad_start, &error) == 0,
			SUITE, CASE_NAME, "failed maxLength -1");
	GENERATED_TEST_ASSERT(generated_expect_utf8_string(&out,
			"abc", SUITE, CASE_NAME,
			"maxLength -1") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected maxLength -1 result");

	GENERATED_TEST_ASSERT(generated_measure_and_pad(&out, storage,
			GENERATED_LEN(storage),
			jsmethod_value_string_utf8((const uint8_t *)"abc", 3), 1,
			jsmethod_value_number(3.0), 1,
			jsmethod_value_string_utf8((const uint8_t *)"def", 3),
			jsmethod_string_pad_start_measure, jsmethod_string_pad_start, &error) == 0,
			SUITE, CASE_NAME, "failed maxLength 3");
	GENERATED_TEST_ASSERT(generated_expect_utf8_string(&out,
			"abc", SUITE, CASE_NAME,
			"maxLength 3") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected maxLength 3 result");

	GENERATED_TEST_ASSERT(generated_measure_and_pad(&out, storage,
			GENERATED_LEN(storage),
			jsmethod_value_string_utf8((const uint8_t *)"abc", 3), 1,
			jsmethod_value_number(3.9999), 1,
			jsmethod_value_string_utf8((const uint8_t *)"def", 3),
			jsmethod_string_pad_start_measure, jsmethod_string_pad_start, &error) == 0,
			SUITE, CASE_NAME, "failed maxLength 3.9999");
	GENERATED_TEST_ASSERT(generated_expect_utf8_string(&out,
			"abc", SUITE, CASE_NAME,
			"maxLength 3.9999") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected maxLength 3.9999 result");
        	return generated_test_pass(SUITE, CASE_NAME);
        }
