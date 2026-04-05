#include <errno.h>
#include <stdio.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "strings"
#define CASE_NAME "jsmx/u-predefined-whitespace-class-replace-rewrite"

typedef struct generated_predefined_offset_callback_ctx_s {
	size_t call_count;
} generated_predefined_offset_callback_ctx_t;

static int
generated_predefined_offset_callback(jsval_region_t *region, void *opaque,
		const jsval_replace_call_t *call, jsval_t *result_ptr,
		jsmethod_error_t *error)
{
	generated_predefined_offset_callback_ctx_t *ctx =
			(generated_predefined_offset_callback_ctx_t *)opaque;
	char buf[16];
	int len;

	(void)error;
	if (region == NULL || call == NULL || result_ptr == NULL || ctx == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (call->capture_count != 0 || call->captures != NULL
			|| call->groups.kind != JSVAL_KIND_UNDEFINED) {
		errno = EINVAL;
		return -1;
	}
	ctx->call_count++;
	len = snprintf(buf, sizeof(buf), "[%zu]", call->offset);
	if (len < 0 || (size_t)len >= sizeof(buf)) {
		errno = EOVERFLOW;
		return -1;
	}
	return jsval_string_new_utf8(region, (const uint8_t *)buf, (size_t)len,
			result_ptr);
}

int
main(void)
{
	static const uint16_t whitespace_subject_units[] = {
		'A', 0x00A0, 'B', 0x2028, 'C'
	};
	static const uint16_t replace_space_expected[] = {
		'A', 'X', 'B', 'X', 'C'
	};
	static const uint16_t negated_callback_expected[] = {
		'[', '0', ']', 0x00A0, '[', '2', ']', 0x2028, '[', '4', ']'
	};
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t whitespace_subject;
	jsval_t ascii_x;
	jsval_t expected_value;
	jsval_t result;
	jsmethod_error_t error;
	generated_predefined_offset_callback_ctx_t callback_ctx = {0};

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region,
			whitespace_subject_units,
			sizeof(whitespace_subject_units) /
			sizeof(whitespace_subject_units[0]), &whitespace_subject) == 0,
			SUITE, CASE_NAME, "failed to build whitespace subject");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"X", 1, &ascii_x) == 0,
			SUITE, CASE_NAME, "failed to build replacement string");

	GENERATED_TEST_ASSERT(jsval_method_string_replace_all_u_whitespace_class(
			&region, whitespace_subject, ascii_x, &result, &error) == 0,
			SUITE, CASE_NAME, "whitespace replaceAll failed");
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region,
			replace_space_expected,
			sizeof(replace_space_expected) /
			sizeof(replace_space_expected[0]), &expected_value) == 0
			&& jsval_strict_eq(&region, result, expected_value) == 1,
			SUITE, CASE_NAME, "unexpected \\s replaceAll output");

	GENERATED_TEST_ASSERT(
			jsval_method_string_replace_all_u_negated_whitespace_class_fn(
				&region, whitespace_subject,
				generated_predefined_offset_callback, &callback_ctx,
				&result, &error) == 0,
			SUITE, CASE_NAME, "negated whitespace callback replaceAll failed");
	GENERATED_TEST_ASSERT(callback_ctx.call_count == 3,
			SUITE, CASE_NAME, "expected three \\S callback invocations");
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region,
			negated_callback_expected,
			sizeof(negated_callback_expected) /
			sizeof(negated_callback_expected[0]), &expected_value) == 0
			&& jsval_strict_eq(&region, result, expected_value) == 1,
			SUITE, CASE_NAME, "unexpected \\S callback replaceAll output");

	return generated_test_pass(SUITE, CASE_NAME);
}
