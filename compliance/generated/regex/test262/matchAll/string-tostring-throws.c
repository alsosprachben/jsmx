#include <stdint.h>

#include "compliance/generated/string_search_helpers.h"

#define SUITE "regex"
#define CASE_NAME "test262/matchAll/string-tostring-throws"

int
main(void)
{
	uint16_t string_storage[8];
	generated_string_error_ctx_t abrupt_ctx = {
		JSMETHOD_ERROR_ABRUPT, "Test262Error"
	};
	jsmethod_error_t error;
	jsstr16_t out;

	jsstr16_init_from_buf(&out, (char *)string_storage, sizeof(string_storage));

	/*
	 * Generated from test262:
	 * test/built-ins/RegExp/prototype/Symbol.matchAll/string-tostring-throws.js
	 *
	 * This idiomatic slow-path translation preserves abrupt ToString(string)
	 * before entering the semantic regex runtime.
	 */

	GENERATED_TEST_ASSERT(jsmethod_value_to_string(&out,
			jsmethod_value_coercible(&abrupt_ctx,
					generated_string_error_to_string),
			0, &error) == -1,
			SUITE, CASE_NAME, "expected abrupt string coercion failure");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_ABRUPT, SUITE,
			CASE_NAME, "expected ABRUPT string failure, got %d",
			(int)error.kind);
	return generated_test_pass(SUITE, CASE_NAME);
}
