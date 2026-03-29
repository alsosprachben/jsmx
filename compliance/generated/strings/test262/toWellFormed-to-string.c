#include "compliance/generated/test_contract.h"

#define SUITE "strings"
#define CASE_NAME "test262/toWellFormed-to-string"

int
main(void)
{
	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/toWellFormed/to-string.js
	 *
	 * This case requires a dynamic receiver object with a user-defined
	 * `toString` hook. That behavior belongs to the translator-emitted slow
	 * path outside the current flattened `jsmx` / `jsval` model, so the
	 * committed fixture records the boundary explicitly instead of faking it.
	 */
	return generated_test_known_unsupported(SUITE, CASE_NAME,
			"dynamic object receiver coercion requires a translator slow path");
}
