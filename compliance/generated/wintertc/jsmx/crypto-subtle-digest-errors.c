#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "wintertc"
#define CASE_NAME "jsmx/crypto-subtle-digest-errors"

static int
expect_dom_exception_name(jsval_region_t *region, jsval_t value,
		const uint8_t *expected, size_t expected_len, const char *label)
{
	jsval_t name_value;
	size_t actual_len = 0;
	uint8_t buf[expected_len ? expected_len : 1];

	if (value.kind != JSVAL_KIND_DOM_EXCEPTION) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: expected DOMException result", label);
	}
	if (jsval_dom_exception_name(region, value, &name_value) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: failed to read exception name", label);
	}
	if (jsval_string_copy_utf8(region, name_value, NULL, 0, &actual_len) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: failed to measure exception name", label);
	}
	if (actual_len != expected_len) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: expected %zu exception-name bytes, got %zu", label,
				expected_len, actual_len);
	}
	if (jsval_string_copy_utf8(region, name_value, buf, actual_len, NULL) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: failed to copy exception name", label);
	}
	if (memcmp(buf, expected, expected_len) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: unexpected exception name", label);
	}
	return GENERATED_TEST_PASS;
}

int
main(void)
{
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t crypto_value;
	jsval_t subtle_value;
	jsval_t input;
	jsval_t promise_value;
	jsval_t result;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_crypto_new(&region, &crypto_value) == 0,
			SUITE, CASE_NAME, "failed to allocate crypto object");
	GENERATED_TEST_ASSERT(jsval_crypto_subtle(&region, crypto_value,
			&subtle_value) == 0, SUITE, CASE_NAME,
			"failed to read crypto.subtle");
	GENERATED_TEST_ASSERT(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8,
			16, &input) == 0, SUITE, CASE_NAME,
			"failed to allocate digest input");

	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"SHA-999", 7, &result) == 0,
			SUITE, CASE_NAME, "failed to allocate unsupported algorithm");
	GENERATED_TEST_ASSERT(jsval_subtle_crypto_digest(&region, subtle_value,
			result, input, &promise_value) == 0, SUITE, CASE_NAME,
			"failed to call subtle.digest with unsupported algorithm");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, promise_value, &result)
			== 0 && result.kind == JSVAL_KIND_DOM_EXCEPTION,
			SUITE, CASE_NAME,
			"expected unsupported algorithm to reject with DOMException");
	GENERATED_TEST_ASSERT(expect_dom_exception_name(&region, result,
			(const uint8_t *)"NotSupportedError", 17,
			"unsupported algorithm") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected unsupported-algorithm rejection");

	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"SHA-256", 7, &result) == 0,
			SUITE, CASE_NAME, "failed to allocate SHA-256 string");
	GENERATED_TEST_ASSERT(jsval_subtle_crypto_digest(&region, subtle_value,
			result, jsval_number(5.0), &promise_value) == 0,
			SUITE, CASE_NAME,
			"failed to call subtle.digest with invalid data");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, promise_value, &result)
			== 0 && result.kind == JSVAL_KIND_DOM_EXCEPTION,
			SUITE, CASE_NAME,
			"expected invalid data to reject with DOMException");
	GENERATED_TEST_ASSERT(expect_dom_exception_name(&region, result,
			(const uint8_t *)"TypeError", 9, "invalid data")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected invalid-data rejection");
	return generated_test_pass(SUITE, CASE_NAME);
}
