#include <stdint.h>

#include "compliance/generated/string_search_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/endsWith/searchstring-found-without-position"

#define CHECK_TRUE(HAY, HAY_LEN, NEEDLE, NEEDLE_LEN, LABEL) do { \
	GENERATED_TEST_ASSERT(jsmethod_string_ends_with(&result, \
			jsmethod_value_string_utf8((const uint8_t *)(HAY), (HAY_LEN)), \
			jsmethod_value_string_utf8((const uint8_t *)(NEEDLE), (NEEDLE_LEN)), \
			0, jsmethod_value_undefined(), &error) == 0, SUITE, CASE_NAME, \
			"failed to lower %s", (LABEL)); \
	GENERATED_TEST_ASSERT(generated_expect_search_bool(result, 1, SUITE, \
			CASE_NAME, (LABEL)) == GENERATED_TEST_PASS, SUITE, CASE_NAME, \
			"unexpected result for %s", (LABEL)); \
} while (0)

int
main(void)
{
	jsmethod_error_t error;
	int result;
	static const uint8_t str[] = "The future is cool!";

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/endsWith/searchstring-found-without-position.js
	 */
	CHECK_TRUE(str, sizeof(str) - 1, "cool!", 5, "str.endsWith(\"cool!\")");
	CHECK_TRUE(str, sizeof(str) - 1, "!", 1, "str.endsWith(\"!\")");
	CHECK_TRUE(str, sizeof(str) - 1, str, sizeof(str) - 1,
			"str.endsWith(str)");
	return generated_test_pass(SUITE, CASE_NAME);
}

#undef CHECK_TRUE
