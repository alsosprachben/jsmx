#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "values"
#define CASE_NAME "jsmx/symbol-primitive-parity"

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
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t description;
	jsval_t symbol_a;
	jsval_t symbol_b;
	jsval_t symbol_none;
	jsval_t result;
	int equal_result = 0;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"token", 5, &description) == 0,
			SUITE, CASE_NAME, "failed to allocate symbol description");
	GENERATED_TEST_ASSERT(jsval_symbol_new(&region, 1, description,
			&symbol_a) == 0, SUITE, CASE_NAME,
			"failed to allocate first symbol");
	GENERATED_TEST_ASSERT(jsval_symbol_new(&region, 1, description,
			&symbol_b) == 0, SUITE, CASE_NAME,
			"failed to allocate second symbol");
	GENERATED_TEST_ASSERT(jsval_symbol_new(&region, 0, jsval_undefined(),
			&symbol_none) == 0, SUITE, CASE_NAME,
			"failed to allocate symbol without description");

	GENERATED_TEST_ASSERT(jsval_typeof(&region, symbol_a, &result) == 0,
			SUITE, CASE_NAME, "failed to typeof symbol");
	GENERATED_TEST_ASSERT(expect_string(&region, result,
			(const uint8_t *)"symbol", 6, "typeof symbol")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected typeof symbol result");
	GENERATED_TEST_ASSERT(jsval_truthy(&region, symbol_a) == 1,
			SUITE, CASE_NAME, "expected symbol to be truthy");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, symbol_a, symbol_b) == 0,
			SUITE, CASE_NAME,
			"expected same-description symbols to stay distinct");
	GENERATED_TEST_ASSERT(jsval_abstract_eq(&region, symbol_a, symbol_b,
			&equal_result) == 0 && equal_result == 0,
			SUITE, CASE_NAME,
			"expected abstract equality for distinct symbols to stay false");
	GENERATED_TEST_ASSERT(jsval_symbol_description(&region, symbol_a,
			&result) == 0, SUITE, CASE_NAME,
			"failed to read symbol description");
	GENERATED_TEST_ASSERT(expect_string(&region, result,
			(const uint8_t *)"token", 5, "symbol.description")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected symbol description");
	GENERATED_TEST_ASSERT(jsval_symbol_description(&region, symbol_none,
			&result) == 0 && result.kind == JSVAL_KIND_UNDEFINED,
			SUITE, CASE_NAME,
			"expected missing symbol description to read back as undefined");

	return generated_test_pass(SUITE, CASE_NAME);
}
