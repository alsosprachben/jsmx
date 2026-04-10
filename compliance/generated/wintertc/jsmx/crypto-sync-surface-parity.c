#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "wintertc"
#define CASE_NAME "jsmx/crypto-sync-surface-parity"

static int
expect_string(jsval_region_t *region, jsval_t value, const uint8_t *expected,
		size_t expected_len, const char *label)
{
	size_t actual_len = 0;
	uint8_t buf[expected_len ? expected_len : 1];

	if (jsval_string_copy_utf8(region, value, NULL, 0, &actual_len) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: failed to measure string", label);
	}
	if (actual_len != expected_len) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: expected %zu bytes, got %zu",
				label, expected_len, actual_len);
	}
	if (jsval_string_copy_utf8(region, value, buf, actual_len, NULL) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: failed to copy string", label);
	}
	if (memcmp(buf, expected, expected_len) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: unexpected string result", label);
	}
	return GENERATED_TEST_PASS;
}

int
main(void)
{
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t crypto_value;
	jsval_t subtle_a;
	jsval_t subtle_b;
	jsval_t result;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_crypto_new(&region, &crypto_value) == 0,
			SUITE, CASE_NAME, "failed to allocate crypto object");
	GENERATED_TEST_ASSERT(jsval_typeof(&region, crypto_value, &result) == 0,
			SUITE, CASE_NAME, "failed to compute typeof(crypto)");
	GENERATED_TEST_ASSERT(expect_string(&region, result,
			(const uint8_t *)"object", 6, "typeof crypto")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected typeof(crypto)");
	GENERATED_TEST_ASSERT(jsval_truthy(&region, crypto_value) == 1,
			SUITE, CASE_NAME, "crypto should be truthy");
	GENERATED_TEST_ASSERT(jsval_crypto_subtle(&region, crypto_value, &subtle_a)
			== 0, SUITE, CASE_NAME, "failed to read crypto.subtle");
	GENERATED_TEST_ASSERT(jsval_crypto_subtle(&region, crypto_value, &subtle_b)
			== 0, SUITE, CASE_NAME,
			"failed to read crypto.subtle a second time");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, subtle_a, subtle_b) == 1,
			SUITE, CASE_NAME,
			"expected stable subtle object identity");
	GENERATED_TEST_ASSERT(jsval_typeof(&region, subtle_a, &result) == 0,
			SUITE, CASE_NAME, "failed to compute typeof(subtle)");
	GENERATED_TEST_ASSERT(expect_string(&region, result,
			(const uint8_t *)"object", 6, "typeof subtle")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected typeof(subtle)");
	return generated_test_pass(SUITE, CASE_NAME);
}
