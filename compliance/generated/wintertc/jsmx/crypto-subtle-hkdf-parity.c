#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "wintertc"
#define CASE_NAME "jsmx/crypto-subtle-hkdf-parity"

static int
make_string(jsval_region_t *region, const char *text, jsval_t *value_ptr)
{
	return jsval_string_new_utf8(region, (const uint8_t *)text, strlen(text),
			value_ptr);
}

static int
make_usage_array(jsval_region_t *region, int derive_bits, int derive_key,
		int sign, int verify, int encrypt, int decrypt,
		jsval_t *value_ptr)
{
	jsval_t array;
	jsval_t value;
	size_t cap = (derive_bits ? 1u : 0u) + (derive_key ? 1u : 0u)
			+ (sign ? 1u : 0u) + (verify ? 1u : 0u)
			+ (encrypt ? 1u : 0u) + (decrypt ? 1u : 0u);

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
	if (sign) {
		if (make_string(region, "sign", &value) < 0
				|| jsval_array_push(region, array, value) < 0) {
			return -1;
		}
	}
	if (verify) {
		if (make_string(region, "verify", &value) < 0
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
	if (decrypt) {
		if (make_string(region, "decrypt", &value) < 0
				|| jsval_array_push(region, array, value) < 0) {
			return -1;
		}
	}
	*value_ptr = array;
	return 0;
}

static int
make_hash_object(jsval_region_t *region, const char *hash_name,
		jsval_t *value_ptr)
{
	jsval_t object;
	jsval_t value;

	if (jsval_object_new(region, 1, &object) < 0
			|| make_string(region, hash_name, &value) < 0
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
make_hkdf_params(jsval_region_t *region, jsval_t salt_value,
		jsval_t info_value, jsval_t *value_ptr)
{
	jsval_t object;
	jsval_t value;
	jsval_t hash_object;

	if (jsval_object_new(region, 4, &object) < 0
			|| make_string(region, "HKDF", &value) < 0
			|| jsval_object_set_utf8(region, object, (const uint8_t *)"name", 4,
				value) < 0
			|| make_hash_object(region, "SHA-256", &hash_object) < 0
			|| jsval_object_set_utf8(region, object, (const uint8_t *)"hash", 4,
				hash_object) < 0
			|| jsval_object_set_utf8(region, object, (const uint8_t *)"salt", 4,
				salt_value) < 0
			|| jsval_object_set_utf8(region, object, (const uint8_t *)"info", 4,
				info_value) < 0) {
		return -1;
	}
	*value_ptr = object;
	return 0;
}

static int
make_hmac_algorithm(jsval_region_t *region, double length_value,
		jsval_t *value_ptr)
{
	jsval_t object;
	jsval_t hash_object;
	jsval_t value;

	if (jsval_object_new(region, 3, &object) < 0
			|| jsval_object_new(region, 1, &hash_object) < 0
			|| make_string(region, "HMAC", &value) < 0
			|| jsval_object_set_utf8(region, object, (const uint8_t *)"name", 4,
				value) < 0
			|| make_string(region, "SHA-256", &value) < 0
			|| jsval_object_set_utf8(region, hash_object,
				(const uint8_t *)"name", 4, value) < 0
			|| jsval_object_set_utf8(region, object, (const uint8_t *)"hash", 4,
				hash_object) < 0
			|| jsval_object_set_utf8(region, object, (const uint8_t *)"length", 6,
				jsval_number(length_value)) < 0) {
		return -1;
	}
	*value_ptr = object;
	return 0;
}

static int
make_aes_algorithm(jsval_region_t *region, double length_value,
		jsval_t *value_ptr)
{
	jsval_t object;
	jsval_t value;

	if (jsval_object_new(region, 2, &object) < 0
			|| make_string(region, "AES-GCM", &value) < 0
			|| jsval_object_set_utf8(region, object, (const uint8_t *)"name", 4,
				value) < 0
			|| jsval_object_set_utf8(region, object, (const uint8_t *)"length", 6,
				jsval_number(length_value)) < 0) {
		return -1;
	}
	*value_ptr = object;
	return 0;
}

static int
expect_string(jsval_region_t *region, jsval_t value, const char *expected,
		const char *label)
{
	size_t len = 0;
	size_t expected_len = strlen(expected);
	uint8_t buf[expected_len ? expected_len : 1];

	if (jsval_string_copy_utf8(region, value, NULL, 0, &len) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: failed to measure string", label);
	}
	if (len != expected_len) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: expected %zu bytes, got %zu", label, expected_len, len);
	}
	if (jsval_string_copy_utf8(region, value, buf, len, NULL) < 0
			|| memcmp(buf, expected, len) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: unexpected string bytes", label);
	}
	return GENERATED_TEST_PASS;
}

static int
expect_array_buffer_bytes(jsval_region_t *region, jsval_t value,
		const uint8_t *expected, size_t expected_len, const char *label)
{
	size_t len = 0;
	uint8_t buf[expected_len ? expected_len : 1];

	if (value.kind != JSVAL_KIND_ARRAY_BUFFER) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: expected ArrayBuffer", label);
	}
	if (jsval_array_buffer_copy_bytes(region, value, NULL, 0, &len) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: failed to measure ArrayBuffer", label);
	}
	if (len != expected_len) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: expected %zu bytes, got %zu", label, expected_len, len);
	}
	if (jsval_array_buffer_copy_bytes(region, value, buf, len, NULL) < 0
			|| memcmp(buf, expected, len) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: unexpected ArrayBuffer bytes", label);
	}
	return GENERATED_TEST_PASS;
}

int
main(void)
{
	static const uint8_t hkdf_zero_derivebits_32[] = {
		0x82, 0x31, 0x15, 0x09, 0x86, 0xb1, 0xed, 0x08,
		0x62, 0x8a, 0x63, 0xfa, 0x43, 0xd5, 0x42, 0xcb,
		0x2f, 0x2d, 0xb9, 0x1d, 0xa3, 0xfa, 0xc6, 0xd8,
		0x95, 0x0c, 0xab, 0x2f, 0x2b, 0x2e, 0xe4, 0x51
	};
	static const uint8_t hkdf_zero_derivekey_aes_16[] = {
		0x82, 0x31, 0x15, 0x09, 0x86, 0xb1, 0xed, 0x08,
		0x62, 0x8a, 0x63, 0xfa, 0x43, 0xd5, 0x42, 0xcb
	};
	static const uint8_t hkdf_zero_derivekey_hmac_64[] = {
		0x82, 0x31, 0x15, 0x09, 0x86, 0xb1, 0xed, 0x08,
		0x62, 0x8a, 0x63, 0xfa, 0x43, 0xd5, 0x42, 0xcb,
		0x2f, 0x2d, 0xb9, 0x1d, 0xa3, 0xfa, 0xc6, 0xd8,
		0x95, 0x0c, 0xab, 0x2f, 0x2b, 0x2e, 0xe4, 0x51,
		0x2e, 0xe4, 0xce, 0x8f, 0x92, 0x5c, 0xb7, 0x1e,
		0xa7, 0x3c, 0x6e, 0x7f, 0x0c, 0x29, 0x80, 0x1e,
		0x9f, 0xea, 0x6f, 0xfa, 0x11, 0xbe, 0x5e, 0x73,
		0x4c, 0xcd, 0x50, 0x39, 0x98, 0x3c, 0xd7, 0xae
	};
	uint8_t storage[262144];
	jsval_region_t region;
	jsval_t crypto_value;
	jsval_t subtle_value;
	jsval_t derive_usages;
	jsval_t sign_verify_usages;
	jsval_t encrypt_decrypt_usages;
	jsval_t raw_format;
	jsval_t key_input;
	jsval_t salt_input;
	jsval_t info_input;
	jsval_t hkdf_algorithm;
	jsval_t hkdf_params;
	jsval_t hmac_algorithm;
	jsval_t aes_algorithm;
	jsval_t base_key_promise;
	jsval_t base_key;
	jsval_t promise_value;
	jsval_t result;
	jsval_t prop;
	uint32_t usages = 0;
	uint32_t promise_state = 0;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_crypto_new(&region, &crypto_value) == 0,
			SUITE, CASE_NAME, "failed to allocate crypto");
	GENERATED_TEST_ASSERT(jsval_crypto_subtle(&region, crypto_value,
			&subtle_value) == 0, SUITE, CASE_NAME,
			"failed to read crypto.subtle");
	GENERATED_TEST_ASSERT(make_usage_array(&region, 1, 1, 0, 0, 0, 0,
			&derive_usages) == 0, SUITE, CASE_NAME,
			"failed to build derive usages");
	GENERATED_TEST_ASSERT(make_usage_array(&region, 0, 0, 1, 1, 0, 0,
			&sign_verify_usages) == 0, SUITE, CASE_NAME,
			"failed to build sign/verify usages");
	GENERATED_TEST_ASSERT(make_usage_array(&region, 0, 0, 0, 0, 1, 1,
			&encrypt_decrypt_usages) == 0, SUITE, CASE_NAME,
			"failed to build encrypt/decrypt usages");
	GENERATED_TEST_ASSERT(make_string(&region, "raw", &raw_format) == 0,
			SUITE, CASE_NAME, "failed to build raw format");
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
	GENERATED_TEST_ASSERT(make_hkdf_params(&region, salt_input, info_input,
			&hkdf_params) == 0, SUITE, CASE_NAME,
			"failed to build HKDF params");
	GENERATED_TEST_ASSERT(make_hmac_algorithm(&region, 512.0,
			&hmac_algorithm) == 0, SUITE, CASE_NAME,
			"failed to build HMAC algorithm");
	GENERATED_TEST_ASSERT(make_aes_algorithm(&region, 128.0, &aes_algorithm) == 0,
			SUITE, CASE_NAME, "failed to build AES algorithm");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_import_key(&region, subtle_value,
			raw_format, key_input, hkdf_algorithm, 1, derive_usages,
			&base_key_promise) == 0, SUITE, CASE_NAME,
			"failed to call HKDF importKey(raw)");
	GENERATED_TEST_ASSERT(jsval_promise_state(&region, base_key_promise,
			&promise_state) == 0 && promise_state == JSVAL_PROMISE_STATE_PENDING,
			SUITE, CASE_NAME,
			"expected HKDF importKey promise to stay pending");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0,
			SUITE, CASE_NAME, "failed to drain HKDF importKey");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, base_key_promise,
			&base_key) == 0 && base_key.kind == JSVAL_KIND_CRYPTO_KEY, SUITE,
			CASE_NAME, "expected HKDF importKey to resolve CryptoKey");
	GENERATED_TEST_ASSERT(jsval_crypto_key_algorithm(&region, base_key, &result)
			== 0, SUITE, CASE_NAME, "failed to read HKDF algorithm");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"name", 4, &prop) == 0, SUITE, CASE_NAME,
			"failed to read HKDF algorithm.name");
	GENERATED_TEST_ASSERT(expect_string(&region, prop, "HKDF",
			"hkdf algorithm") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected HKDF algorithm name");
	GENERATED_TEST_ASSERT(jsval_crypto_key_usages(&region, base_key, &usages)
			== 0, SUITE, CASE_NAME, "failed to read HKDF usages");
	GENERATED_TEST_ASSERT(usages == (JSVAL_CRYPTO_KEY_USAGE_DERIVE_BITS
			| JSVAL_CRYPTO_KEY_USAGE_DERIVE_KEY), SUITE, CASE_NAME,
			"unexpected HKDF usages");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_derive_bits(&region, subtle_value,
			hkdf_params, base_key, jsval_number(256.0), &promise_value) == 0,
			SUITE, CASE_NAME, "failed to call HKDF deriveBits");
	GENERATED_TEST_ASSERT(jsval_promise_state(&region, promise_value,
			&promise_state) == 0 && promise_state == JSVAL_PROMISE_STATE_PENDING,
			SUITE, CASE_NAME,
			"expected HKDF deriveBits promise to stay pending");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0,
			SUITE, CASE_NAME, "failed to drain HKDF deriveBits");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, promise_value, &result)
			== 0, SUITE, CASE_NAME, "failed to read HKDF deriveBits result");
	GENERATED_TEST_ASSERT(expect_array_buffer_bytes(&region, result,
			hkdf_zero_derivebits_32, sizeof(hkdf_zero_derivebits_32),
			"hkdf deriveBits") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected HKDF deriveBits bytes");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_derive_key(&region, subtle_value,
			hkdf_params, base_key, hmac_algorithm, 1, sign_verify_usages,
			&promise_value) == 0, SUITE, CASE_NAME,
			"failed to call HKDF deriveKey(HMAC)");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0,
			SUITE, CASE_NAME, "failed to drain HKDF deriveKey(HMAC)");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, promise_value, &result)
			== 0 && result.kind == JSVAL_KIND_CRYPTO_KEY, SUITE, CASE_NAME,
			"expected HKDF deriveKey(HMAC) CryptoKey");
	GENERATED_TEST_ASSERT(jsval_subtle_crypto_export_key(&region, subtle_value,
			raw_format, result, &promise_value) == 0, SUITE, CASE_NAME,
			"failed to export derived HMAC key");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0,
			SUITE, CASE_NAME, "failed to drain HKDF HMAC export");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, promise_value, &result)
			== 0, SUITE, CASE_NAME,
			"failed to read HKDF HMAC export result");
	GENERATED_TEST_ASSERT(expect_array_buffer_bytes(&region, result,
			hkdf_zero_derivekey_hmac_64, sizeof(hkdf_zero_derivekey_hmac_64),
			"hkdf deriveKey(hmac)") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected HKDF HMAC export bytes");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_derive_key(&region, subtle_value,
			hkdf_params, base_key, aes_algorithm, 1, encrypt_decrypt_usages,
			&promise_value) == 0, SUITE, CASE_NAME,
			"failed to call HKDF deriveKey(AES-GCM)");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0,
			SUITE, CASE_NAME, "failed to drain HKDF deriveKey(AES-GCM)");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, promise_value, &result)
			== 0 && result.kind == JSVAL_KIND_CRYPTO_KEY, SUITE, CASE_NAME,
			"expected HKDF deriveKey(AES-GCM) CryptoKey");
	GENERATED_TEST_ASSERT(jsval_subtle_crypto_export_key(&region, subtle_value,
			raw_format, result, &promise_value) == 0, SUITE, CASE_NAME,
			"failed to export derived AES key");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0,
			SUITE, CASE_NAME, "failed to drain HKDF AES export");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, promise_value, &result)
			== 0, SUITE, CASE_NAME,
			"failed to read HKDF AES export result");
	GENERATED_TEST_ASSERT(expect_array_buffer_bytes(&region, result,
			hkdf_zero_derivekey_aes_16, sizeof(hkdf_zero_derivekey_aes_16),
			"hkdf deriveKey(aes)") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected HKDF AES export bytes");

	return generated_test_pass(SUITE, CASE_NAME);
}
