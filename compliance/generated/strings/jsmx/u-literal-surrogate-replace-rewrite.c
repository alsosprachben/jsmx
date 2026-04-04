#include "compliance/generated/test_contract.h"
#include <errno.h>
#include <string.h>
#include "jsval.h"

#define SUITE "strings"
#define CASE_NAME "jsmx/u-literal-surrogate-replace-rewrite"

typedef struct generated_surrogate_replace_ctx_s {
	int call_count;
	const uint16_t *expected_input;
	size_t expected_input_len;
	const uint16_t *expected_matches[2];
	size_t expected_match_lens[2];
	size_t expected_offsets[2];
	const char *replacement_values[2];
} generated_surrogate_replace_ctx_t;

static int
expect_utf16(jsval_region_t *region, jsval_t value, const uint16_t *expected,
		size_t expected_len)
{
	static const uint16_t empty_unit = 0;
	jsval_t expected_value;

	if (jsval_string_new_utf16(region,
			expected_len > 0 ? expected : &empty_unit, expected_len,
			&expected_value) < 0) {
		return -1;
	}
	return jsval_strict_eq(region, value, expected_value) == 1 ? 0 : -1;
}

static int
generated_surrogate_replace_callback(jsval_region_t *region, void *opaque,
		const jsval_replace_call_t *call, jsval_t *result_ptr,
		jsmethod_error_t *error)
{
	generated_surrogate_replace_ctx_t *ctx =
			(generated_surrogate_replace_ctx_t *)opaque;
	size_t index = (size_t)ctx->call_count++;

	(void)error;
	if (call == NULL || index >= 2 || call->capture_count != 0
			|| call->captures != NULL
			|| call->offset != ctx->expected_offsets[index]
			|| expect_utf16(region, call->input, ctx->expected_input,
				ctx->expected_input_len) < 0
			|| expect_utf16(region, call->match,
				ctx->expected_matches[index],
				ctx->expected_match_lens[index]) < 0) {
		errno = EINVAL;
		return -1;
	}
	return jsval_string_new_utf8(region,
			(const uint8_t *)ctx->replacement_values[index],
			strlen(ctx->replacement_values[index]), result_ptr);
}

int
main(void)
{
	static const uint16_t pair_low_units[] = {
		'A', 0xD834, 0xDF06, 0xDF06, 'B', 0xDF06
	};
	static const uint16_t subst_subject_units[] = {
		'X', 0xDF06, 'Y', 0xDF06
	};
	static const uint16_t low_unit[] = {0xDF06};
	static const uint16_t replace_first_expected[] = {
		'A', 0xD834, 0xDF06, 'X', 'B', 0xDF06
	};
	static const uint16_t replace_all_expected[] = {
		'A', 0xD834, 0xDF06, 'X', 'B', 'X'
	};
	static const uint16_t special_expected[] = {
		'X', '[', '$', ']', '[', 0xDF06, ']', '[', '$', '1', ']',
		'[', 'X', ']', '[', 'Y', 0xDF06, ']', 'Y', 0xDF06
	};
	static const uint16_t callback_first_expected[] = {
		'A', 0xD834, 0xDF06, '<', 'L', '>', 'B', 0xDF06
	};
	static const uint16_t callback_all_expected[] = {
		'A', 0xD834, 0xDF06, '<', 'A', '>', 'B', '<', 'B', '>'
	};
	uint8_t storage[131072];
	jsval_region_t region;
	jsval_t pair_low;
	jsval_t subst_subject;
	jsval_t ascii_x;
	jsval_t special_replacement;
	jsval_t result;
	jsmethod_error_t error;
	generated_surrogate_replace_ctx_t replace_ctx = {
		0,
		pair_low_units,
		sizeof(pair_low_units) / sizeof(pair_low_units[0]),
		{low_unit},
		{1},
		{3},
		{"<L>"}
	};
	generated_surrogate_replace_ctx_t replace_all_ctx = {
		0,
		pair_low_units,
		sizeof(pair_low_units) / sizeof(pair_low_units[0]),
		{low_unit, low_unit},
		{1, 1},
		{3, 5},
		{"<A>", "<B>"}
	};

	jsval_region_init(&region, storage, sizeof(storage));

	/*
	 * Repo-authored rewrite-backed fixture:
	 * lone-surrogate `/u` replace helpers preserve the no-capture contract
	 * while skipping surrogate halves inside pairs for replace, replaceAll,
	 * and callback replacers.
	 */
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, pair_low_units,
			sizeof(pair_low_units) / sizeof(pair_low_units[0]),
			&pair_low) == 0,
			SUITE, CASE_NAME, "failed to build pair/low subject");
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, subst_subject_units,
			sizeof(subst_subject_units) / sizeof(subst_subject_units[0]),
			&subst_subject) == 0,
			SUITE, CASE_NAME, "failed to build substitution subject");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"X", 1, &ascii_x) == 0,
			SUITE, CASE_NAME, "failed to build ascii replacement");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"[$$][$&][$1][$`][$']", 20,
			&special_replacement) == 0,
			SUITE, CASE_NAME, "failed to build substitution replacement");

	GENERATED_TEST_ASSERT(jsval_method_string_replace_u_literal_surrogate(
			&region, pair_low, 0xDF06, ascii_x, &result, &error) == 0,
			SUITE, CASE_NAME, "surrogate replace failed");
	GENERATED_TEST_ASSERT(expect_utf16(&region, result, replace_first_expected,
			sizeof(replace_first_expected) /
			sizeof(replace_first_expected[0])) == 0,
			SUITE, CASE_NAME, "replace should skip the surrogate pair and only rewrite the first isolated low surrogate");

	GENERATED_TEST_ASSERT(jsval_method_string_replace_all_u_literal_surrogate(
			&region, pair_low, 0xDF06, ascii_x, &result, &error) == 0,
			SUITE, CASE_NAME, "surrogate replaceAll failed");
	GENERATED_TEST_ASSERT(expect_utf16(&region, result, replace_all_expected,
			sizeof(replace_all_expected) /
			sizeof(replace_all_expected[0])) == 0,
			SUITE, CASE_NAME, "replaceAll should rewrite both isolated low surrogates only");

	GENERATED_TEST_ASSERT(jsval_method_string_replace_u_literal_surrogate(
			&region, subst_subject, 0xDF06, special_replacement, &result,
			&error) == 0,
			SUITE, CASE_NAME, "surrogate substitution replace failed");
	GENERATED_TEST_ASSERT(expect_utf16(&region, result, special_expected,
			sizeof(special_expected) / sizeof(special_expected[0])) == 0,
			SUITE, CASE_NAME, "replacement substitutions should treat $1 as literal text in the no-capture rewrite");

	GENERATED_TEST_ASSERT(jsval_method_string_replace_u_literal_surrogate_fn(
			&region, pair_low, 0xDF06, generated_surrogate_replace_callback,
			&replace_ctx, &result, &error) == 0,
			SUITE, CASE_NAME, "callback surrogate replace failed");
	GENERATED_TEST_ASSERT(replace_ctx.call_count == 1, SUITE, CASE_NAME,
			"replace callback should run once");
	GENERATED_TEST_ASSERT(expect_utf16(&region, result,
			callback_first_expected,
			sizeof(callback_first_expected) /
			sizeof(callback_first_expected[0])) == 0,
			SUITE, CASE_NAME, "callback replace result mismatch");

	GENERATED_TEST_ASSERT(jsval_method_string_replace_all_u_literal_surrogate_fn(
			&region, pair_low, 0xDF06, generated_surrogate_replace_callback,
			&replace_all_ctx, &result, &error) == 0,
			SUITE, CASE_NAME, "callback surrogate replaceAll failed");
	GENERATED_TEST_ASSERT(replace_all_ctx.call_count == 2, SUITE, CASE_NAME,
			"replaceAll callback should run twice");
	GENERATED_TEST_ASSERT(expect_utf16(&region, result,
			callback_all_expected,
			sizeof(callback_all_expected) /
			sizeof(callback_all_expected[0])) == 0,
			SUITE, CASE_NAME, "callback replaceAll result mismatch");

	return generated_test_pass(SUITE, CASE_NAME);
}
