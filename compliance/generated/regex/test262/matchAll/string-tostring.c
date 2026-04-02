#include <stdint.h>

#include "compliance/generated/regex_match_all_helpers.h"
#include "compliance/generated/string_search_helpers.h"

#define SUITE "regex"
#define CASE_NAME "test262/matchAll/string-tostring"

int
main(void)
{
	static const uint16_t input_text[] = {'a', '*', 'b'};
	uint8_t region_storage[16384];
	uint16_t string_storage[sizeof(input_text) / sizeof(input_text[0])];
	jsval_region_t region;
	jsval_t regexp;
	jsval_t input_value;
	jsval_t iterator;
	jsval_t result;
	generated_string_callback_ctx_t ctx = {
		0, input_text, sizeof(input_text) / sizeof(input_text[0]), NULL
	};
	jsmethod_error_t error;
	jsstr16_t out;

	jsval_region_init(&region, region_storage, sizeof(region_storage));
	jsstr16_init_from_buf(&out, (char *)string_storage, sizeof(string_storage));

	/*
	 * Generated from test262:
	 * test/built-ins/RegExp/prototype/Symbol.matchAll/string-tostring.js
	 *
	 * This idiomatic slow-path translation materializes ToString(string)
	 * explicitly before calling the semantic regex runtime entrypoint.
	 */

	GENERATED_TEST_ASSERT(jsmethod_value_to_string(&out,
			jsmethod_value_coercible(&ctx, generated_string_callback_to_string),
			0, &error) == 0,
			SUITE, CASE_NAME, "string coercion failed");
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, string_storage,
			out.len, &input_value) == 0,
			SUITE, CASE_NAME, "native string build failed");
	GENERATED_TEST_ASSERT(generated_regexp_new_utf8(&region, "\\w", "g",
			&regexp, &error) == 0,
			SUITE, CASE_NAME, "regex build failed");
	GENERATED_TEST_ASSERT(jsval_regexp_match_all(&region, regexp, input_value,
			&iterator, &error) == 0,
			SUITE, CASE_NAME, "matchAll iterator build failed");
	GENERATED_TEST_ASSERT(generated_expect_match_iterator_next(&region,
			iterator, &result, SUITE, CASE_NAME, "first match")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "first iterator step mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_string_prop(&region, result,
			"0", "a", SUITE, CASE_NAME, "first match[0]")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "first match[0] mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_number_prop(&region, result,
			"index", 0.0, SUITE, CASE_NAME, "first index")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "first index mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_iterator_next(&region,
			iterator, &result, SUITE, CASE_NAME, "second match")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "second iterator step mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_string_prop(&region, result,
			"0", "b", SUITE, CASE_NAME, "second match[0]")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "second match[0] mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_number_prop(&region, result,
			"index", 2.0, SUITE, CASE_NAME, "second index")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "second index mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_iterator_done(&region,
			iterator, SUITE, CASE_NAME, "done")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "expected iterator completion");
	return generated_test_pass(SUITE, CASE_NAME);
}
