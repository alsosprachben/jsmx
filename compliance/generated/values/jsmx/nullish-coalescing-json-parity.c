#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "values"
#define CASE_NAME "jsmx/nullish-coalescing-json-parity"

static int
expect_string(jsval_region_t *region, jsval_t value,
		const uint8_t *expected, size_t expected_len, const char *label)
{
	size_t actual_len = 0;
	uint8_t buf[expected_len ? expected_len : 1];

	if (jsval_string_copy_utf8(region, value, NULL, 0, &actual_len) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: failed to measure string result", label);
	}
	if (actual_len != expected_len) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: expected %zu bytes, got %zu",
				label, expected_len, actual_len);
	}
	if (jsval_string_copy_utf8(region, value, buf, actual_len, NULL) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: failed to copy string result", label);
	}
	if (memcmp(buf, expected, expected_len) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: string result did not match expected output", label);
	}
	return GENERATED_TEST_PASS;
}

int
main(void)
{
	static const uint8_t input[] =
		"{\"nothing\":null,\"flag\":false,\"zero\":0,\"empty\":\"\",\"text\":\"x\",\"obj\":{}}";
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t root;
	jsval_t nothing;
	jsval_t flag;
	jsval_t zero;
	jsval_t empty;
	jsval_t text;
	jsval_t obj;
	jsval_t fallback;
	jsval_t other_obj;
	jsval_t left;
	jsval_t result;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_json_parse(&region, input, sizeof(input) - 1, 24,
			&root) == 0, SUITE, CASE_NAME,
			"failed to parse input JSON into a jsval region");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"nothing", 7, &nothing) == 0,
			SUITE, CASE_NAME, "failed to read nothing");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"flag", 4, &flag) == 0,
			SUITE, CASE_NAME, "failed to read flag");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"zero", 4, &zero) == 0,
			SUITE, CASE_NAME, "failed to read zero");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"empty", 5, &empty) == 0,
			SUITE, CASE_NAME, "failed to read empty");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"text", 4, &text) == 0,
			SUITE, CASE_NAME, "failed to read text");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"obj", 3, &obj) == 0,
			SUITE, CASE_NAME, "failed to read obj");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"fallback", 8, &fallback) == 0, SUITE, CASE_NAME,
			"failed to construct fallback string");
	GENERATED_TEST_ASSERT(jsval_object_new(&region, 0, &other_obj) == 0, SUITE,
			CASE_NAME, "failed to construct fallback object");

	left = nothing;
	if (jsval_is_nullish(left)) {
		result = fallback;
	} else {
		result = left;
	}
	GENERATED_TEST_ASSERT(expect_string(&region, result,
			(const uint8_t *)"fallback", 8,
			"parsed null ?? fallback") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected parsed null ?? fallback result");

	left = flag;
	if (jsval_is_nullish(left)) {
		result = fallback;
	} else {
		result = left;
	}
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, result, jsval_bool(0)) == 1,
			SUITE, CASE_NAME, "expected parsed false to survive nullish coalescing");

	left = zero;
	if (jsval_is_nullish(left)) {
		result = fallback;
	} else {
		result = left;
	}
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, result, jsval_number(0.0)) == 1,
			SUITE, CASE_NAME, "expected parsed zero to survive nullish coalescing");

	left = empty;
	if (jsval_is_nullish(left)) {
		result = fallback;
	} else {
		result = left;
	}
	GENERATED_TEST_ASSERT(expect_string(&region, result, (const uint8_t *)"", 0,
			"parsed empty ?? fallback") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected parsed empty ?? fallback result");

	left = text;
	if (jsval_is_nullish(left)) {
		result = fallback;
	} else {
		result = left;
	}
	GENERATED_TEST_ASSERT(expect_string(&region, result, (const uint8_t *)"x", 1,
			"parsed text ?? fallback") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected parsed text ?? fallback result");

	left = obj;
	if (jsval_is_nullish(left)) {
		result = other_obj;
	} else {
		result = left;
	}
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, result, obj) == 1, SUITE,
			CASE_NAME,
			"expected parsed object identity to survive nullish coalescing");

	return generated_test_pass(SUITE, CASE_NAME);
}
