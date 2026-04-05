#include <stdio.h>
#include <string.h>

#include "compliance/generated/regex_core_helpers.h"
#include "compliance/generated/string_replace_callback_helpers.h"

#define SUITE "strings"
#define CASE_NAME "jsmx/named-groups-replace-callback-parity"

typedef struct generated_named_groups_callback_record_s {
	size_t call_count;
	int input_set;
	char input[32];
	size_t offsets[4];
	char digits[4][16];
	int tail_defined[4];
	char tails[4][16];
} generated_named_groups_callback_record_t;

static int
generated_record_named_groups_callback(jsval_region_t *region, void *opaque,
		const jsval_replace_call_t *call, jsval_t *result_ptr,
		jsmethod_error_t *error)
{
	generated_named_groups_callback_record_t *ctx =
			(generated_named_groups_callback_record_t *)opaque;
	jsval_t value;
	char tail_buf[16];
	char replacement[64];
	int len;
	size_t index;

	(void)error;
	if (region == NULL || ctx == NULL || call == NULL || result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	index = ctx->call_count++;
	if (index >= 4 || call->groups.kind != JSVAL_KIND_OBJECT) {
		errno = EINVAL;
		return -1;
	}
	if (!ctx->input_set) {
		if (generated_copy_jsval_utf8_value(region, call->input, ctx->input,
				sizeof(ctx->input)) < 0) {
			return -1;
		}
		ctx->input_set = 1;
	}
	ctx->offsets[index] = call->offset;
	if (jsval_object_get_utf8(region, call->groups,
			(const uint8_t *)"digits", 6, &value) < 0
			|| generated_copy_jsval_utf8_value(region, value,
				ctx->digits[index], sizeof(ctx->digits[index])) < 0) {
		return -1;
	}
	if (jsval_object_get_utf8(region, call->groups,
			(const uint8_t *)"tail", 4, &value) < 0) {
		return -1;
	}
	if (value.kind == JSVAL_KIND_UNDEFINED) {
		ctx->tail_defined[index] = 0;
		strcpy(ctx->tails[index], "");
		strcpy(tail_buf, "U");
	} else {
		ctx->tail_defined[index] = 1;
		if (generated_copy_jsval_utf8_value(region, value, ctx->tails[index],
				sizeof(ctx->tails[index])) < 0) {
			return -1;
		}
		strcpy(tail_buf, ctx->tails[index]);
	}
	len = snprintf(replacement, sizeof(replacement), "<%s|%s|%zu>",
			ctx->digits[index], tail_buf, call->offset);
	if (len < 0 || (size_t)len >= sizeof(replacement)) {
		errno = EOVERFLOW;
		return -1;
	}
	return jsval_string_new_utf8(region, (const uint8_t *)replacement,
			(size_t)len, result_ptr);
}

int
main(void)
{
	static const uint8_t json[] = "{\"text\":\"a1b2\"}";
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t root;
	jsval_t parsed_text;
	jsval_t native_text;
	jsval_t regex;
	jsval_t result;
	jsmethod_error_t error;
	generated_named_groups_callback_record_t parsed_replace = {0};
	generated_named_groups_callback_record_t native_replace = {0};
	generated_named_groups_callback_record_t parsed_replace_all = {0};
	generated_named_groups_callback_record_t native_replace_all = {0};

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_json_parse(&region, json, sizeof(json) - 1, 12,
			&root) == 0, SUITE, CASE_NAME, "json parse failed");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"text", 4, &parsed_text) == 0,
			SUITE, CASE_NAME, "parsed text lookup failed");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"a1b2", 4, &native_text) == 0,
			SUITE, CASE_NAME, "native text build failed");
	GENERATED_TEST_ASSERT(generated_regexp_new_utf8(&region,
			"(?<digits>[0-9])(?<tail>[a-z])?", "g", &regex, &error) == 0,
			SUITE, CASE_NAME, "regex build failed");

	GENERATED_TEST_ASSERT(jsval_method_string_replace_fn(&region, parsed_text,
			regex, generated_record_named_groups_callback, &parsed_replace,
			&result, &error) == 0,
			SUITE, CASE_NAME, "parsed replace callback failed");
	GENERATED_TEST_ASSERT(generated_expect_jsval_utf8_string(&region,
			result, "a<1|b|1><2|U|3>", SUITE, CASE_NAME,
			"parsed replace callback") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "parsed replace callback mismatch");
	GENERATED_TEST_ASSERT(jsval_method_string_replace_fn(&region, native_text,
			regex, generated_record_named_groups_callback, &native_replace,
			&result, &error) == 0,
			SUITE, CASE_NAME, "native replace callback failed");
	GENERATED_TEST_ASSERT(generated_expect_jsval_utf8_string(&region,
			result, "a<1|b|1><2|U|3>", SUITE, CASE_NAME,
			"native replace callback") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "native replace callback mismatch");

	GENERATED_TEST_ASSERT(jsval_method_string_replace_all_fn(&region,
			parsed_text, regex, generated_record_named_groups_callback,
			&parsed_replace_all, &result, &error) == 0,
			SUITE, CASE_NAME, "parsed replaceAll callback failed");
	GENERATED_TEST_ASSERT(generated_expect_jsval_utf8_string(&region,
			result, "a<1|b|1><2|U|3>", SUITE, CASE_NAME,
			"parsed replaceAll callback") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "parsed replaceAll callback mismatch");
	GENERATED_TEST_ASSERT(jsval_method_string_replace_all_fn(&region,
			native_text, regex, generated_record_named_groups_callback,
			&native_replace_all, &result, &error) == 0,
			SUITE, CASE_NAME, "native replaceAll callback failed");
	GENERATED_TEST_ASSERT(generated_expect_jsval_utf8_string(&region,
			result, "a<1|b|1><2|U|3>", SUITE, CASE_NAME,
			"native replaceAll callback") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "native replaceAll callback mismatch");

	GENERATED_TEST_ASSERT(parsed_replace.call_count == 2 &&
			native_replace.call_count == 2 &&
			parsed_replace_all.call_count == 2 &&
			native_replace_all.call_count == 2,
			SUITE, CASE_NAME, "expected two callback invocations per run");
	GENERATED_TEST_ASSERT(strcmp(parsed_replace.input, "a1b2") == 0 &&
			strcmp(native_replace.input, "a1b2") == 0 &&
			strcmp(parsed_replace_all.input, "a1b2") == 0 &&
			strcmp(native_replace_all.input, "a1b2") == 0,
			SUITE, CASE_NAME, "unexpected callback input");
	GENERATED_TEST_ASSERT(parsed_replace.offsets[0] == 1 &&
			parsed_replace.offsets[1] == 3 &&
			native_replace.offsets[0] == 1 &&
			native_replace.offsets[1] == 3 &&
			parsed_replace_all.offsets[0] == 1 &&
			parsed_replace_all.offsets[1] == 3 &&
			native_replace_all.offsets[0] == 1 &&
			native_replace_all.offsets[1] == 3,
			SUITE, CASE_NAME, "unexpected callback offsets");
	GENERATED_TEST_ASSERT(strcmp(parsed_replace.digits[0], "1") == 0 &&
			strcmp(parsed_replace.digits[1], "2") == 0 &&
			strcmp(native_replace.digits[0], "1") == 0 &&
			strcmp(native_replace.digits[1], "2") == 0,
			SUITE, CASE_NAME, "unexpected replace callback digits");
	GENERATED_TEST_ASSERT(parsed_replace.tail_defined[0] == 1 &&
			strcmp(parsed_replace.tails[0], "b") == 0 &&
			parsed_replace.tail_defined[1] == 0 &&
			native_replace.tail_defined[0] == 1 &&
			strcmp(native_replace.tails[0], "b") == 0 &&
			native_replace.tail_defined[1] == 0,
			SUITE, CASE_NAME, "unexpected replace callback tails");
	GENERATED_TEST_ASSERT(parsed_replace_all.tail_defined[0] == 1 &&
			strcmp(parsed_replace_all.tails[0], "b") == 0 &&
			parsed_replace_all.tail_defined[1] == 0 &&
			native_replace_all.tail_defined[0] == 1 &&
			strcmp(native_replace_all.tails[0], "b") == 0 &&
			native_replace_all.tail_defined[1] == 0,
			SUITE, CASE_NAME, "unexpected replaceAll callback tails");

	return generated_test_pass(SUITE, CASE_NAME);
}
