#include "compliance/generated/string_search_helpers.h"

#define SUITE_NAME "strings"
#define CASE_NAME "test262/search/this-value-not-obj-coercible"

int
main(void)
{
	jsmethod_error_t error;
	ssize_t index;

	if (jsmethod_string_search_regex(&index, jsmethod_value_undefined(),
			jsmethod_value_string_utf8((const uint8_t *)".", 1),
			0, jsmethod_value_undefined(), &error) == 0) {
		return generated_test_fail(SUITE_NAME, CASE_NAME,
				"undefined receiver should fail");
	}
	if (error.kind != JSMETHOD_ERROR_TYPE) {
		return generated_test_fail(SUITE_NAME, CASE_NAME,
				"expected TYPE for undefined receiver, got %d",
				(int)error.kind);
	}
	if (jsmethod_string_search_regex(&index, jsmethod_value_null(),
			jsmethod_value_string_utf8((const uint8_t *)".", 1),
			0, jsmethod_value_undefined(), &error) == 0) {
		return generated_test_fail(SUITE_NAME, CASE_NAME,
				"null receiver should fail");
	}
	if (error.kind != JSMETHOD_ERROR_TYPE) {
		return generated_test_fail(SUITE_NAME, CASE_NAME,
				"expected TYPE for null receiver, got %d",
				(int)error.kind);
	}
	return generated_test_pass(SUITE_NAME, CASE_NAME);
}
