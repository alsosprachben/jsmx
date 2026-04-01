#include <string.h>
#include <stdint.h>

#include "compliance/generated/string_replace_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/replace/S15.5.4.11_A1_T11"

int
main(void)
{
	uint16_t storage[1];
	int replacement_calls = 0;
	generated_string_error_ctx_t search_ctx = {
		JSMETHOD_ERROR_ABRUPT, "insearchValue"
	};
	jsmethod_error_t error;
	jsstr16_t probe;

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/replace/S15.5.4.11_A1_T11.js
	 *
	 * This is an idiomatic slow-path translation. The upstream throwing
	 * searchValue object is modeled entirely in the translator-emitted slow
	 * path before any flattened replace dispatch. The later replacement
	 * callback proves the slow path does not evaluate replaceValue once
	 * searchValue coercion fails.
	 */

	jsstr16_init_from_buf(&probe, (char *)storage, sizeof(storage));
	GENERATED_TEST_ASSERT(generated_string_error_to_string(&search_ctx, &probe,
			&error) == -1, SUITE, CASE_NAME,
			"expected searchValue coercion to fail");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_ABRUPT,
			SUITE, CASE_NAME, "expected ABRUPT searchValue coercion");
	GENERATED_TEST_ASSERT(error.message != NULL
			&& strcmp(error.message, "insearchValue") == 0, SUITE, CASE_NAME,
			"expected searchValue message insearchValue, got %s",
			error.message != NULL ? error.message : "(null)");
	GENERATED_TEST_ASSERT(replacement_calls == 0, SUITE, CASE_NAME,
			"expected replaceValue coercion not to run, got %d calls",
			replacement_calls);
	return generated_test_pass(SUITE, CASE_NAME);
}
