#include <stdint.h>

#include "compliance/generated/string_search_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/lastIndexOf/S15.5.4.8_A1_T6"

int
main(void)
{
	jsmethod_error_t error;
	ssize_t index;

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/lastIndexOf/S15.5.4.8_A1_T6.js
	 *
	 * This idiomatic slow-path translation collapses the boxed String receiver
	 * to its primitive string value before dispatching into jsmethod.
	 */
	GENERATED_TEST_ASSERT(jsmethod_string_last_index_of(&index,
			jsmethod_value_string_utf8((const uint8_t *)"undefined", 9),
			jsmethod_value_undefined(),
			0, jsmethod_value_undefined(), &error) == 0, SUITE, CASE_NAME,
			"failed to lower boxed receiver lastIndexOf(undefined)");
	GENERATED_TEST_ASSERT(generated_expect_search_index(index, 0, SUITE,
			CASE_NAME, "\"undefined\".lastIndexOf(undefined)")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for lastIndexOf(undefined)");
	return generated_test_pass(SUITE, CASE_NAME);
}
