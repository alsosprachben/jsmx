#include <math.h>
#include <stdint.h>

#include "compliance/generated/string_search_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/lastIndexOf/S15.5.4.8_A1_T10"

int
main(void)
{
	jsmethod_error_t error;
	ssize_t index;

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/lastIndexOf/S15.5.4.8_A1_T10.js
	 *
	 * This is an idiomatic slow-path translation. The object searchString and
	 * position operands are lowered by the translator to the primitive results
	 * of their user-defined coercions before the thin jsmethod layer runs.
	 */
	GENERATED_TEST_ASSERT(jsmethod_string_last_index_of(&index,
			jsmethod_value_string_utf8((const uint8_t *)"ABBABABAB", 9),
			jsmethod_value_string_utf8((const uint8_t *)"AB", 2),
			1, jsmethod_value_number(NAN), &error) == 0,
			SUITE, CASE_NAME, "failed to lower object-argument lastIndexOf");
	GENERATED_TEST_ASSERT(generated_expect_search_index(index, 7, SUITE,
			CASE_NAME, "\"ABBABABAB\".lastIndexOf(__obj, __obj2)")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for object-argument lastIndexOf");
	return generated_test_pass(SUITE, CASE_NAME);
}
