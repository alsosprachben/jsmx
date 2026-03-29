#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "values"
#define CASE_NAME "test262/S11.6.1_A3.2_T2.1"

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
				"%s: string concatenation result did not match expected output",
				label);
	}
	return GENERATED_TEST_PASS;
}

int
main(void)
{
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t sum;
	jsval_t one_string;
	jsval_t x_string;

	/*
	 * Generated from test262:
	 * test/language/expressions/addition/S11.6.1_A3.2_T2.1.js
	 *
	 * This idiomatic flattened translation preserves the primitive string and
	 * number cases. The original wrapper-object checks are intentionally
	 * omitted because boxed strings and numbers belong above the current
	 * flattened `jsval` surface.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region, (const uint8_t *)"1", 1,
			&one_string) == 0, SUITE, CASE_NAME,
			"failed to construct primitive string operand");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region, (const uint8_t *)"x", 1,
			&x_string) == 0, SUITE, CASE_NAME,
			"failed to construct secondary string operand");

	GENERATED_TEST_ASSERT(jsval_add(&region, one_string, jsval_number(1.0), &sum) == 0,
			SUITE, CASE_NAME, "\"1\" + 1 should lower through jsval_add");
	GENERATED_TEST_ASSERT(expect_string(&region, sum, (const uint8_t *)"11", 2,
			"\"1\" + 1") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result for \"1\" + 1");

	GENERATED_TEST_ASSERT(jsval_add(&region, jsval_number(1.0), one_string, &sum) == 0,
			SUITE, CASE_NAME, "1 + \"1\" should lower through jsval_add");
	GENERATED_TEST_ASSERT(expect_string(&region, sum, (const uint8_t *)"11", 2,
			"1 + \"1\"") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result for 1 + \"1\"");

	GENERATED_TEST_ASSERT(jsval_add(&region, x_string, jsval_number(1.0), &sum) == 0,
			SUITE, CASE_NAME, "\"x\" + 1 should lower through jsval_add");
	GENERATED_TEST_ASSERT(expect_string(&region, sum, (const uint8_t *)"x1", 2,
			"\"x\" + 1") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result for \"x\" + 1");

	GENERATED_TEST_ASSERT(jsval_add(&region, jsval_number(1.0), x_string, &sum) == 0,
			SUITE, CASE_NAME, "1 + \"x\" should lower through jsval_add");
	GENERATED_TEST_ASSERT(expect_string(&region, sum, (const uint8_t *)"1x", 2,
			"1 + \"x\"") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result for 1 + \"x\"");

	return generated_test_pass(SUITE, CASE_NAME);
}
