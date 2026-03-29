#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "values"
#define CASE_NAME "jsmx/json-backed-primitive-parity"

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
expect_number(jsval_t value, double expected, const char *label)
{
	if (value.kind != JSVAL_KIND_NUMBER || value.as.number != expected) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: expected numeric result %.17g", label, expected);
	}
	return GENERATED_TEST_PASS;
}

int
main(void)
{
	static const uint8_t input[] =
		"{\"num\":1,\"flag\":true,\"off\":false,\"text\":\"2\",\"nothing\":null}";
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t root;
	jsval_t num;
	jsval_t flag;
	jsval_t off;
	jsval_t text;
	jsval_t nothing;
	jsval_t native_text;
	jsval_t sum;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_json_parse(&region, input, sizeof(input) - 1, 24,
			&root) == 0, SUITE, CASE_NAME,
			"failed to parse input JSON into a jsval region");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"num", 3, &num) == 0,
			SUITE, CASE_NAME, "failed to read num");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"flag", 4, &flag) == 0,
			SUITE, CASE_NAME, "failed to read flag");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"off", 3, &off) == 0,
			SUITE, CASE_NAME, "failed to read off");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"text", 4, &text) == 0,
			SUITE, CASE_NAME, "failed to read text");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"nothing", 7, &nothing) == 0,
			SUITE, CASE_NAME, "failed to read nothing");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region, (const uint8_t *)"2", 1,
			&native_text) == 0, SUITE, CASE_NAME,
			"failed to construct native string literal");

	GENERATED_TEST_ASSERT(jsval_truthy(&region, num) == jsval_truthy(&region,
			jsval_number(1.0)), SUITE, CASE_NAME,
			"expected parsed number truthiness to match native number");
	GENERATED_TEST_ASSERT(jsval_truthy(&region, flag) == jsval_truthy(&region,
			jsval_bool(1)), SUITE, CASE_NAME,
			"expected parsed true truthiness to match native true");
	GENERATED_TEST_ASSERT(jsval_truthy(&region, off) == jsval_truthy(&region,
			jsval_bool(0)), SUITE, CASE_NAME,
			"expected parsed false truthiness to match native false");
	GENERATED_TEST_ASSERT(jsval_truthy(&region, text) == jsval_truthy(&region,
			native_text), SUITE, CASE_NAME,
			"expected parsed string truthiness to match native string");
	GENERATED_TEST_ASSERT(jsval_truthy(&region, nothing) == jsval_truthy(&region,
			jsval_null()), SUITE, CASE_NAME,
			"expected parsed null truthiness to match native null");

	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, num, jsval_number(1.0)) == 1,
			SUITE, CASE_NAME, "expected parsed number to strictly equal native 1");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, flag, jsval_bool(1)) == 1,
			SUITE, CASE_NAME, "expected parsed true to strictly equal native true");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, off, jsval_bool(0)) == 1,
			SUITE, CASE_NAME, "expected parsed false to strictly equal native false");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, text, native_text) == 1,
			SUITE, CASE_NAME, "expected parsed string to strictly equal native string");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, nothing, jsval_null()) == 1,
			SUITE, CASE_NAME, "expected parsed null to strictly equal native null");

	GENERATED_TEST_ASSERT(jsval_add(&region, text, jsval_number(1.0), &sum) == 0,
			SUITE, CASE_NAME, "expected parsed string + 1 to lower through jsval_add");
	GENERATED_TEST_ASSERT(expect_string(&region, sum, (const uint8_t *)"21", 2,
			"parsed string + 1") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected parsed string concatenation result");
	GENERATED_TEST_ASSERT(jsval_add(&region, flag, jsval_number(1.0), &sum) == 0,
			SUITE, CASE_NAME, "expected parsed true + 1 to lower through jsval_add");
	GENERATED_TEST_ASSERT(expect_number(sum, 2.0, "parsed true + 1")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected parsed true + 1 result");
	GENERATED_TEST_ASSERT(jsval_add(&region, nothing, jsval_number(1.0), &sum) == 0,
			SUITE, CASE_NAME, "expected parsed null + 1 to lower through jsval_add");
	GENERATED_TEST_ASSERT(expect_number(sum, 1.0, "parsed null + 1")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected parsed null + 1 result");

	return generated_test_pass(SUITE, CASE_NAME);
}
