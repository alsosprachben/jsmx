#include <math.h>
        #include <stdint.h>

        #include "compliance/generated/string_pad_helpers.h"

        #define SUITE "strings"
        #define CASE_NAME "test262/padStart/fill-string-non-strings"

        int
        main(void)
        {
        	uint16_t storage[16];
        	jsmethod_error_t error;
        	jsstr16_t out;

        	GENERATED_TEST_ASSERT(generated_measure_and_pad(&out, storage,
			GENERATED_LEN(storage),
			jsmethod_value_string_utf8((const uint8_t *)"abc", 3), 1,
			jsmethod_value_number(10.0), 1, jsmethod_value_bool(0),
			jsmethod_string_pad_start_measure, jsmethod_string_pad_start, &error) == 0,
			SUITE, CASE_NAME, "failed fillString false");
	GENERATED_TEST_ASSERT(generated_expect_utf8_string(&out,
			"falsefaabc", SUITE, CASE_NAME,
			"fillString false") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected fillString false result");

	GENERATED_TEST_ASSERT(generated_measure_and_pad(&out, storage,
			GENERATED_LEN(storage),
			jsmethod_value_string_utf8((const uint8_t *)"abc", 3), 1,
			jsmethod_value_number(10.0), 1, jsmethod_value_bool(1),
			jsmethod_string_pad_start_measure, jsmethod_string_pad_start, &error) == 0,
			SUITE, CASE_NAME, "failed fillString true");
	GENERATED_TEST_ASSERT(generated_expect_utf8_string(&out,
			"truetruabc", SUITE, CASE_NAME,
			"fillString true") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected fillString true result");

	GENERATED_TEST_ASSERT(generated_measure_and_pad(&out, storage,
			GENERATED_LEN(storage),
			jsmethod_value_string_utf8((const uint8_t *)"abc", 3), 1,
			jsmethod_value_number(10.0), 1, jsmethod_value_null(),
			jsmethod_string_pad_start_measure, jsmethod_string_pad_start, &error) == 0,
			SUITE, CASE_NAME, "failed fillString null");
	GENERATED_TEST_ASSERT(generated_expect_utf8_string(&out,
			"nullnulabc", SUITE, CASE_NAME,
			"fillString null") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected fillString null result");

	GENERATED_TEST_ASSERT(generated_measure_and_pad(&out, storage,
			GENERATED_LEN(storage),
			jsmethod_value_string_utf8((const uint8_t *)"abc", 3), 1,
			jsmethod_value_number(10.0), 1, jsmethod_value_number(0.0),
			jsmethod_string_pad_start_measure, jsmethod_string_pad_start, &error) == 0,
			SUITE, CASE_NAME, "failed fillString 0");
	GENERATED_TEST_ASSERT(generated_expect_utf8_string(&out,
			"0000000abc", SUITE, CASE_NAME,
			"fillString 0") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected fillString 0 result");

	GENERATED_TEST_ASSERT(generated_measure_and_pad(&out, storage,
			GENERATED_LEN(storage),
			jsmethod_value_string_utf8((const uint8_t *)"abc", 3), 1,
			jsmethod_value_number(10.0), 1, jsmethod_value_number(-0.0),
			jsmethod_string_pad_start_measure, jsmethod_string_pad_start, &error) == 0,
			SUITE, CASE_NAME, "failed fillString -0");
	GENERATED_TEST_ASSERT(generated_expect_utf8_string(&out,
			"0000000abc", SUITE, CASE_NAME,
			"fillString -0") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected fillString -0 result");

	GENERATED_TEST_ASSERT(generated_measure_and_pad(&out, storage,
			GENERATED_LEN(storage),
			jsmethod_value_string_utf8((const uint8_t *)"abc", 3), 1,
			jsmethod_value_number(10.0), 1, jsmethod_value_number(NAN),
			jsmethod_string_pad_start_measure, jsmethod_string_pad_start, &error) == 0,
			SUITE, CASE_NAME, "failed fillString NaN");
	GENERATED_TEST_ASSERT(generated_expect_utf8_string(&out,
			"NaNNaNNabc", SUITE, CASE_NAME,
			"fillString NaN") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected fillString NaN result");
        	return generated_test_pass(SUITE, CASE_NAME);
        }
