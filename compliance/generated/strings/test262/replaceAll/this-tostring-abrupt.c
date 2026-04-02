#include <stdint.h>

#include "compliance/generated/string_replace_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/replaceAll/this-tostring-abrupt"

int
main(void)
{
	static const uint16_t x_text[] = {'X'};
	uint16_t storage[8];
	int search_calls = 0;
	int replacement_calls = 0;
	generated_string_error_ctx_t abrupt_ctx = {
		JSMETHOD_ERROR_ABRUPT, "Test262Error"
	};
	generated_string_callback_ctx_t later_search_ctx = {
		0, x_text, GENERATED_LEN(x_text), &search_calls
	};
	generated_string_callback_ctx_t later_replacement_ctx = {
		0, x_text, GENERATED_LEN(x_text), &replacement_calls
	};
	jsmethod_error_t error;
	jsstr16_t out;

	jsstr16_init_from_buf(&out, (char *)storage, sizeof(storage));

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/replaceAll/this-tostring-abrupt.js
	 *
	 * This idiomatic flattened translation preserves abrupt receiver
	 * coercion. The second upstream sample, where receiver coercion
	 * produces a Symbol and ToString throws a TypeError, is modeled by
	 * passing a Symbol receiver directly into the flattened entrypoint.
	 */

	GENERATED_TEST_ASSERT(jsmethod_string_replace_all(&out,
			jsmethod_value_coercible(&abrupt_ctx,
					generated_string_error_to_string),
			jsmethod_value_coercible(&later_search_ctx,
					generated_string_callback_to_string),
			jsmethod_value_coercible(&later_replacement_ctx,
					generated_string_callback_to_string),
			&error) == -1, SUITE, CASE_NAME,
			"expected abrupt receiver coercion to fail");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_ABRUPT, SUITE,
			CASE_NAME, "expected ABRUPT receiver failure, got %d",
			(int)error.kind);
	GENERATED_TEST_ASSERT(search_calls == 0, SUITE, CASE_NAME,
			"expected searchValue coercion not to run, got %d calls",
			search_calls);
	GENERATED_TEST_ASSERT(replacement_calls == 0, SUITE, CASE_NAME,
			"expected replaceValue coercion not to run, got %d calls",
			replacement_calls);

	GENERATED_TEST_ASSERT(jsmethod_string_replace_all(&out,
			jsmethod_value_symbol(),
			jsmethod_value_coercible(&later_search_ctx,
					generated_string_callback_to_string),
			jsmethod_value_coercible(&later_replacement_ctx,
					generated_string_callback_to_string),
			&error) == -1, SUITE, CASE_NAME,
			"expected Symbol receiver ToString to fail");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_TYPE, SUITE,
			CASE_NAME, "expected TYPE for Symbol receiver, got %d",
			(int)error.kind);
	GENERATED_TEST_ASSERT(search_calls == 0, SUITE, CASE_NAME,
			"expected searchValue coercion to remain unevaluated, got %d calls",
			search_calls);
	GENERATED_TEST_ASSERT(replacement_calls == 0, SUITE, CASE_NAME,
			"expected replaceValue coercion to remain unevaluated, got %d calls",
			replacement_calls);
	return generated_test_pass(SUITE, CASE_NAME);
}
