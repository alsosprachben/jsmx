#include "compliance/generated/regex_exec_match_helpers.h"
#include "compliance/generated/string_search_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/exec/S15.10.6.2_A1_T15"

int
main(void)
{
	static const uint16_t slow_text[] = {'f','a','l','s','e'};
	uint8_t storage[32768];
	uint16_t materialized_buf[8];
	jsval_region_t region;
	jsmethod_value_t slow_value;
	jsstr16_t slow_str;
	generated_string_callback_ctx_t slow_ctx = {0, slow_text, 5, NULL};
	jsval_t input;
	jsval_t pattern;
	jsval_t flags;
	jsval_t regex;
	jsval_t result;
	jsmethod_error_t error;

	/* Idiomatic slow-path translation of the input object's toString hook. */
	jsval_region_init(&region, storage, sizeof(storage));
	slow_value = jsmethod_value_coercible(&slow_ctx,
			generated_string_callback_to_string);
	jsstr16_init_from_buf(&slow_str, (char *)materialized_buf,
			sizeof(materialized_buf));
	GENERATED_TEST_ASSERT(jsmethod_value_to_string(&slow_str, slow_value, 0,
			&error) == 0, SUITE, CASE_NAME,
			"slow-path stringification failed");
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, slow_str.codeunits,
			slow_str.len, &input) == 0, SUITE, CASE_NAME,
			"input materialization failed");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"LS", 2, &pattern) == 0,
			SUITE, CASE_NAME, "pattern build failed");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"i", 1, &flags) == 0,
			SUITE, CASE_NAME, "flags build failed");
	GENERATED_TEST_ASSERT(jsval_regexp_new(&region, pattern, 1, flags, &regex,
			&error) == 0, SUITE, CASE_NAME, "regex build failed");
	GENERATED_TEST_ASSERT(jsval_regexp_exec(&region, regex, input, &result,
			&error) == 0, SUITE, CASE_NAME, "exec failed");
	GENERATED_TEST_ASSERT(generated_expect_match_string_prop(&region, result, "0",
			"ls", SUITE, CASE_NAME, "match[0]") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "match[0] mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_number_prop(&region, result,
			"index", 2.0, SUITE, CASE_NAME, "index") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "index mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_string_prop(&region, result,
			"input", "false", SUITE, CASE_NAME, "input") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "input mismatch");
	return generated_test_pass(SUITE, CASE_NAME);
}
