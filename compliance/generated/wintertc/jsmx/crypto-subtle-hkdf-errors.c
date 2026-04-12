#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "wintertc"
#define CASE_NAME "jsmx/crypto-subtle-hkdf-errors"

static int
make_string(jsval_region_t *region, const char *text, jsval_t *value_ptr)
{
	return jsval_string_new_utf8(region, (const uint8_t *)text, strlen(text),
			value_ptr);
}

static int
make_usage_array(jsval_region_t *region, int derive_bits, int derive_key,
		int encrypt, jsval_t *value_ptr)
{
	jsval_t array;
	jsval_t value;
	size_t cap = (derive_bits ? 1u : 0u) + (derive_key ? 1u : 0u)
			+ (encrypt ? 1u : 0u);

	if (jsval_array_new(region, cap, &array) < 0) {
		return -1;
	}
	if (derive_bits) {
		if (make_string(region, "deriveBits", &value) < 0
				|| jsval_array_push(region, array, value) < 0) {
			return -1;
		}
	}
	if (derive_key) {
		if (make_string(region, "deriveKey", &value) < 0
				|| jsval_array_push(region, array, value) < 0) {
			return -1;
		}
	}
	if (encrypt) {
		if (make_string(region, "encrypt", &value) < 0
				|| jsval_array_push(region, array, value) < 0) {
			return -1;
		}
	}
	*value_ptr = array;
	return 0;
}

static int
make_hash_object(jsval_region_t *region, jsval_t *value_ptr)
{
	jsval_t object;
	jsval_t value;

	if (jsval_object_new(region, 1, &object) < 0
			|| make_string(region, "SHA-256", &value) < 0
			|| jsval_object_set_utf8(region, object, (const uint8_t *)"name", 4,
				value) < 0) {
		return -1;
	}
	*value_ptr = object;
	return 0;
}

static int
make_hkdf_algorithm(jsval_region_t *region, jsval_t *value_ptr)
{
	jsval_t object;
	jsval_t value;

	if (jsval_object_new(region, 1, &object) < 0
			|| make_string(region, "HKDF", &value) < 0
			|| jsval_object_set_utf8(region, object, (const uint8_t *)"name", 4,
				value) < 0) {
		return -1;
	}
	*value_ptr = object;
	return 0;
}

static int
make_hkdf_params(jsval_region_t *region, int include_salt, jsval_t salt_value,
		int include_info, jsval_t info_value, jsval_t *value_ptr)
{
	jsval_t object;
	jsval_t value;
	jsval_t hash_object;
	size_t cap = 2u + (include_salt ? 1u : 0u) + (include_info ? 1u : 0u);

	if (jsval_object_new(region, cap, &object) < 0
			|| make_string(region, "HKDF", &value) < 0
			|| jsval_object_set_utf8(region, object, (const uint8_t *)"name", 4,
				value) < 0
			|| make_hash_object(region, &hash_object) < 0
			|| jsval_object_set_utf8(region, object, (const uint8_t *)"hash", 4,
				hash_object) < 0) {
		return -1;
	}
	if (include_salt
			&& jsval_object_set_utf8(region, object, (const uint8_t *)"salt", 4,
				salt_value) < 0) {
		return -1;
	}
	if (include_info
			&& jsval_object_set_utf8(region, object, (const uint8_t *)"info", 4,
				info_value) < 0) {
		return -1;
	}
	*value_ptr = object;
	return 0;
}

static int
make_unsupported_algorithm(jsval_region_t *region, jsval_t *value_ptr)
{
	jsval_t object;
	jsval_t value;

	if (jsval_object_new(region, 2, &object) < 0
			|| make_string(region, "AES-CBC", &value) < 0
			|| jsval_object_set_utf8(region, object, (const uint8_t *)"name", 4,
				value) < 0
			|| jsval_object_set_utf8(region, object, (const uint8_t *)"length", 6,
				jsval_number(128.0)) < 0) {
		return -1;
	}
	*value_ptr = object;
	return 0;
}

static int
expect_dom_exception_name(jsval_region_t *region, jsval_t value,
		const char *expected, const char *label)
{
	jsval_t name;
	size_t len = 0;
	size_t expected_len = strlen(expected);
	uint8_t buf[expected_len ? expected_len : 1];

	if (value.kind != JSVAL_KIND_DOM_EXCEPTION) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: expected DOMException", label);
	}
	if (jsval_dom_exception_name(region, value, &name) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: failed to read DOMException name", label);
	}
	if (jsval_string_copy_utf8(region, name, NULL, 0, &len) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: failed to measure DOMException name", label);
	}
	if (len != expected_len) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: expected DOMException %s", label, expected);
	}
	if (jsval_string_copy_utf8(region, name, buf, len, NULL) < 0
			|| memcmp(buf, expected, len) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: unexpected DOMException name", label);
	}
	return GENERATED_TEST_PASS;
}

int
main(void)
{
	uint8_t storage[196608];
	jsval_region_t region;
	jsval_t crypto_value;
	jsval_t subtle_value;
	jsval_t derive_usages;
	jsval_t derive_key_only_usages;
	jsval_t encrypt_usages;
	jsval_t raw_format;
	jsval_t jwk_format;
	jsval_t key_input;
	jsval_t salt_input;
	jsval_t info_input;
	jsval_t hkdf_algorithm;
	jsval_t hkdf_params;
	jsval_t hkdf_missing_salt;
	jsval_t hkdf_missing_info;
	jsval_t unsupported_algorithm;
	jsval_t import_promise;
	jsval_t derive_key_only_promise;
	jsval_t base_key;
	jsval_t derive_key_only_key;
	jsval_t promise_value;
	jsval_t result;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_crypto_new(&region, &crypto_value) == 0,
			SUITE, CASE_NAME, "failed to allocate crypto");
	GENERATED_TEST_ASSERT(jsval_crypto_subtle(&region, crypto_value,
			&subtle_value) == 0, SUITE, CASE_NAME,
			"failed to read crypto.subtle");
	GENERATED_TEST_ASSERT(make_usage_array(&region, 1, 1, 0,
			&derive_usages) == 0, SUITE, CASE_NAME,
			"failed to build derive usages");
	GENERATED_TEST_ASSERT(make_usage_array(&region, 0, 1, 0,
			&derive_key_only_usages) == 0, SUITE, CASE_NAME,
			"failed to build deriveKey-only usages");
	GENERATED_TEST_ASSERT(make_usage_array(&region, 0, 0, 1,
			&encrypt_usages) == 0, SUITE, CASE_NAME,
			"failed to build encrypt usages");
	GENERATED_TEST_ASSERT(make_string(&region, "raw", &raw_format) == 0, SUITE,
			CASE_NAME, "failed to build raw format");
	GENERATED_TEST_ASSERT(make_string(&region, "jwk", &jwk_format) == 0, SUITE,
			CASE_NAME, "failed to build jwk format");
	GENERATED_TEST_ASSERT(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8,
			8, &key_input) == 0, SUITE, CASE_NAME,
			"failed to allocate key input");
	GENERATED_TEST_ASSERT(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8,
			8, &salt_input) == 0, SUITE, CASE_NAME,
			"failed to allocate salt input");
	GENERATED_TEST_ASSERT(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8,
			8, &info_input) == 0, SUITE, CASE_NAME,
			"failed to allocate info input");
	GENERATED_TEST_ASSERT(make_hkdf_algorithm(&region, &hkdf_algorithm) == 0,
			SUITE, CASE_NAME, "failed to build HKDF algorithm");
	GENERATED_TEST_ASSERT(make_hkdf_params(&region, 1, salt_input, 1, info_input,
			&hkdf_params) == 0, SUITE, CASE_NAME,
			"failed to build HKDF params");
	GENERATED_TEST_ASSERT(make_hkdf_params(&region, 0, salt_input, 1, info_input,
			&hkdf_missing_salt) == 0, SUITE, CASE_NAME,
			"failed to build HKDF missing-salt params");
	GENERATED_TEST_ASSERT(make_hkdf_params(&region, 1, salt_input, 0, info_input,
			&hkdf_missing_info) == 0, SUITE, CASE_NAME,
			"failed to build HKDF missing-info params");
	GENERATED_TEST_ASSERT(make_unsupported_algorithm(&region,
			&unsupported_algorithm) == 0, SUITE, CASE_NAME,
			"failed to build unsupported algorithm");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_import_key(&region, subtle_value,
			raw_format, key_input, hkdf_algorithm, 1, derive_key_only_usages,
			&derive_key_only_promise) == 0, SUITE, CASE_NAME,
			"failed to import deriveKey-only HKDF key");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0,
			SUITE, CASE_NAME, "failed to drain deriveKey-only import");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, derive_key_only_promise,
			&derive_key_only_key) == 0, SUITE, CASE_NAME,
			"failed to read deriveKey-only HKDF key");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_import_key(&region, subtle_value,
			jwk_format, key_input, hkdf_algorithm, 1, derive_usages,
			&promise_value) == 0, SUITE, CASE_NAME,
			"failed to call HKDF importKey(jwk)");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, promise_value, &result)
			== 0, SUITE, CASE_NAME, "failed to read HKDF importKey(jwk) result");
	GENERATED_TEST_ASSERT(expect_dom_exception_name(&region, result,
			"NotSupportedError", "hkdf import format")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected HKDF import format error");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_derive_bits(&region, subtle_value,
			hkdf_params, derive_key_only_key, jsval_number(256.0),
			&promise_value) == 0, SUITE, CASE_NAME,
			"failed to call HKDF deriveBits with wrong usage");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, promise_value, &result)
			== 0, SUITE, CASE_NAME, "failed to read HKDF wrong-usage result");
	GENERATED_TEST_ASSERT(expect_dom_exception_name(&region, result,
			"InvalidAccessError", "hkdf wrong usage")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected HKDF wrong-usage error");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_import_key(&region, subtle_value,
			raw_format, key_input, hkdf_algorithm, 1, derive_usages,
			&import_promise) == 0, SUITE, CASE_NAME,
			"failed to import full-usage HKDF key");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain HKDF import");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, import_promise, &base_key)
			== 0, SUITE, CASE_NAME, "failed to read HKDF base key");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_derive_bits(&region, subtle_value,
			hkdf_params, base_key, jsval_number(255.0), &promise_value) == 0,
			SUITE, CASE_NAME, "failed to call HKDF deriveBits invalid length");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, promise_value, &result)
			== 0, SUITE, CASE_NAME, "failed to read HKDF invalid-length result");
	GENERATED_TEST_ASSERT(expect_dom_exception_name(&region, result,
			"OperationError", "hkdf invalid length") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected HKDF invalid-length error");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_derive_bits(&region, subtle_value,
			hkdf_missing_salt, base_key, jsval_number(256.0),
			&promise_value) == 0, SUITE, CASE_NAME,
			"failed to call HKDF deriveBits missing salt");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, promise_value, &result)
			== 0, SUITE, CASE_NAME, "failed to read HKDF missing-salt result");
	GENERATED_TEST_ASSERT(expect_dom_exception_name(&region, result,
			"TypeError", "hkdf missing salt") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected HKDF missing-salt error");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_derive_bits(&region, subtle_value,
			hkdf_missing_info, base_key, jsval_number(256.0),
			&promise_value) == 0, SUITE, CASE_NAME,
			"failed to call HKDF deriveBits missing info");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, promise_value, &result)
			== 0, SUITE, CASE_NAME, "failed to read HKDF missing-info result");
	GENERATED_TEST_ASSERT(expect_dom_exception_name(&region, result,
			"TypeError", "hkdf missing info") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected HKDF missing-info error");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_derive_key(&region, subtle_value,
			hkdf_params, base_key, unsupported_algorithm, 1, encrypt_usages,
			&promise_value) == 0, SUITE, CASE_NAME,
			"failed to call HKDF deriveKey unsupported target");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, promise_value, &result)
			== 0, SUITE, CASE_NAME,
			"failed to read HKDF unsupported-target result");
	GENERATED_TEST_ASSERT(expect_dom_exception_name(&region, result,
			"NotSupportedError", "hkdf unsupported target")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected HKDF unsupported-target error");

	return generated_test_pass(SUITE, CASE_NAME);
}
