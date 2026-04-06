#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "values"
#define CASE_NAME "jsmx/typeof-regexp-iterator-parity"

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

static int
expect_typeof(jsval_region_t *region, jsval_t value, const char *expected,
		const char *label)
{
	jsval_t result;

	if (jsval_typeof(region, value, &result) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: jsval_typeof failed", label);
	}
	return expect_string(region, result, (const uint8_t *)expected,
			strlen(expected), label);
}

int
main(void)
{
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t pattern;
	jsval_t global_flags;
	jsval_t regex;
	jsval_t subject;
	jsval_t iterator;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region, (const uint8_t *)"a", 1,
			&pattern) == 0, SUITE, CASE_NAME,
			"failed to construct regex pattern");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region, (const uint8_t *)"g", 1,
			&global_flags) == 0, SUITE, CASE_NAME,
			"failed to construct regex flags");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region, (const uint8_t *)"a", 1,
			&subject) == 0, SUITE, CASE_NAME,
			"failed to construct subject string");
	GENERATED_TEST_ASSERT(jsval_regexp_new(&region, pattern, 1, global_flags,
			&regex, &error) == 0, SUITE, CASE_NAME,
			"failed to construct regexp value");
	GENERATED_TEST_ASSERT(jsval_method_string_match_all(&region, subject, 1,
			regex, &iterator, &error) == 0, SUITE, CASE_NAME,
			"failed to construct matchAll iterator");

	GENERATED_TEST_ASSERT(expect_typeof(&region, regex, "object",
			"typeof regexp") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected typeof regexp result");
	GENERATED_TEST_ASSERT(expect_typeof(&region, iterator, "object",
			"typeof matchAll iterator") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected typeof matchAll iterator result");

	return generated_test_pass(SUITE, CASE_NAME);
}
