#include "compliance/generated/test_contract.h"
#include <errno.h>
#include <string.h>
#include "jsval.h"

#define SUITE "strings"
#define CASE_NAME "jsmx/u-literal-negated-range-class-replace-rewrite"

typedef struct replace_ctx_s {
	int call_count;
	const uint16_t *expected_input;
	size_t expected_input_len;
	const uint16_t *expected_matches[2];
	size_t expected_match_lens[2];
	size_t expected_offsets[2];
	const char *replacement_values[2];
} replace_ctx_t;

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
negated_range_replace_callback(jsval_region_t *region, void *opaque,
		const jsval_replace_call_t *call, jsval_t *result_ptr,
		jsmethod_error_t *error)
{
	replace_ctx_t *ctx = (replace_ctx_t *)opaque;
	size_t index = (size_t)ctx->call_count++;

	(void)error;
	if (call == NULL || index >= 2 || call->capture_count != 0
			|| call->captures != NULL
			|| call->groups.kind != JSVAL_KIND_UNDEFINED
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
	static const uint16_t range_subject_units[] = {
		0xD834, 0xDF06, 'A', 'B', 'D', 0xDF06, 'Z'
	};
	static const uint16_t subst_subject_units[] = {'A', 'B', 'D', 'Z'};
	static const uint16_t b_to_d_and_low_ranges[] = {
		'B', 'D', 0xDF06, 0xDF06
	};
	static const uint16_t a_unit[] = {'A'};
	static const uint16_t z_unit[] = {'Z'};
	static const uint16_t replace_expected[] = {
		0xD834, 0xDF06, 'X', 'B', 'D', 0xDF06, 'Z'
	};
	static const uint16_t replace_all_expected[] = {
		0xD834, 0xDF06, 'X', 'B', 'D', 0xDF06, 'X'
	};
	static const uint16_t special_expected[] = {
		'[', '$', ']', '[', 'A', ']', '[', '$', '1', ']',
		'[', ']', '[', 'B', 'D', 'Z', ']', 'B', 'D', 'Z'
	};
	static const uint16_t callback_expected[] = {
		0xD834, 0xDF06, '<', 'A', '>', 'B', 'D', 0xDF06, '<', 'B', '>'
	};
	uint8_t storage[131072];
	jsval_region_t region;
	jsval_t range_subject;
	jsval_t subst_subject;
	jsval_t ascii_x;
	jsval_t special_replacement;
	jsval_t result;
	jsmethod_error_t error;
	replace_ctx_t callback_ctx = {
		0,
		range_subject_units,
		sizeof(range_subject_units) / sizeof(range_subject_units[0]),
		{a_unit, z_unit},
		{1, 1},
		{2, 6},
		{"<A>", "<B>"}
	};

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, range_subject_units,
			sizeof(range_subject_units) / sizeof(range_subject_units[0]),
			&range_subject) == 0,
			SUITE, CASE_NAME, "failed to build negated range subject");
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
			SUITE, CASE_NAME, "failed to build special replacement");

	GENERATED_TEST_ASSERT(jsval_method_string_replace_u_literal_negated_range_class(
			&region, range_subject, b_to_d_and_low_ranges,
			sizeof(b_to_d_and_low_ranges) /
			(2 * sizeof(b_to_d_and_low_ranges[0])), ascii_x,
			&result, &error) == 0,
			SUITE, CASE_NAME, "negated range replace failed");
	GENERATED_TEST_ASSERT(expect_utf16(&region, result, replace_expected,
			sizeof(replace_expected) / sizeof(replace_expected[0])) == 0,
			SUITE, CASE_NAME, "negated range replace result mismatch");

	GENERATED_TEST_ASSERT(jsval_method_string_replace_all_u_literal_negated_range_class(
			&region, range_subject, b_to_d_and_low_ranges,
			sizeof(b_to_d_and_low_ranges) /
			(2 * sizeof(b_to_d_and_low_ranges[0])), ascii_x,
			&result, &error) == 0,
			SUITE, CASE_NAME, "negated range replaceAll failed");
	GENERATED_TEST_ASSERT(expect_utf16(&region, result, replace_all_expected,
			sizeof(replace_all_expected) /
			sizeof(replace_all_expected[0])) == 0,
			SUITE, CASE_NAME, "negated range replaceAll result mismatch");

	GENERATED_TEST_ASSERT(jsval_method_string_replace_u_literal_negated_range_class(
			&region, subst_subject, b_to_d_and_low_ranges,
			sizeof(b_to_d_and_low_ranges) /
			(2 * sizeof(b_to_d_and_low_ranges[0])), special_replacement,
			&result, &error) == 0,
			SUITE, CASE_NAME, "negated range substitution failed");
	GENERATED_TEST_ASSERT(expect_utf16(&region, result, special_expected,
			sizeof(special_expected) / sizeof(special_expected[0])) == 0,
			SUITE, CASE_NAME, "negated range substitution result mismatch");

	GENERATED_TEST_ASSERT(jsval_method_string_replace_all_u_literal_negated_range_class_fn(
			&region, range_subject, b_to_d_and_low_ranges,
			sizeof(b_to_d_and_low_ranges) /
			(2 * sizeof(b_to_d_and_low_ranges[0])),
			negated_range_replace_callback, &callback_ctx, &result,
			&error) == 0,
			SUITE, CASE_NAME, "negated range callback replaceAll failed");
	GENERATED_TEST_ASSERT(callback_ctx.call_count == 2, SUITE, CASE_NAME,
			"expected two negated range callback replaceAll invocations");
	GENERATED_TEST_ASSERT(expect_utf16(&region, result, callback_expected,
			sizeof(callback_expected) /
			sizeof(callback_expected[0])) == 0,
			SUITE, CASE_NAME, "negated range callback replaceAll result mismatch");

	return generated_test_pass(SUITE, CASE_NAME);
}
