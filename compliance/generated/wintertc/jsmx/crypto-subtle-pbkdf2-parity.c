#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "wintertc"
#define CASE_NAME "jsmx/crypto-subtle-pbkdf2-parity"

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
make_pbkdf2_algorithm(jsval_region_t *region, jsval_t *value_ptr)
{
	jsval_t object;
	jsval_t value;

	if (jsval_object_new(region, 1, &object) < 0
			|| make_string(region, "PBKDF2", &value) < 0
			|| jsval_object_set_utf8(region, object, (const uint8_t *)"name", 4,
				value) < 0) {
		return -1;
	}
	*value_ptr = object;
	return 0;
}

static int
make_pbkdf2_params(jsval_region_t *region, jsval_t salt_value,
		jsval_t *value_ptr)
{
	jsval_t object;
	jsval_t value;
	jsval_t hash_object;

	if (jsval_object_new(region, 4, &object) < 0
			|| make_string(region, "PBKDF2", &value) < 0
			|| jsval_object_set_utf8(region, object, (const uint8_t *)"name", 4,
				value) < 0
			|| make_hash_object(region, "SHA-256", &hash_object) < 0
			|| jsval_object_set_utf8(region, object, (const uint8_t *)"hash", 4,
				hash_object) < 0
			|| jsval_object_set_utf8(region, object, (const uint8_t *)"salt", 4,
				salt_value) < 0
			|| jsval_object_set_utf8(region, object,
				(const uint8_t *)"iterations", 10, jsval_number(2.0)) < 0) {
		return -1;
	}
	*value_ptr = object;
	return 0;
}

static int
make_hmac_algorithm(jsval_region_t *region, int have_length,
		double length_value, jsval_t *value_ptr)
{
	jsval_t object;
	jsval_t hash_object;
	jsval_t value;

	if (jsval_object_new(region, have_length ? 3 : 2, &object) < 0
			|| jsval_object_new(region, 1, &hash_object) < 0
			|| make_string(region, "HMAC", &value) < 0
			|| jsval_object_set_utf8(region, object, (const uint8_t *)"name", 4,
				value) < 0
			|| make_string(region, "SHA-256", &value) < 0
			|| jsval_object_set_utf8(region, hash_object,
				(const uint8_t *)"name", 4, value) < 0
			|| jsval_object_set_utf8(region, object, (const uint8_t *)"hash", 4,
				hash_object) < 0) {
		return -1;
	}
	if (have_length
			&& jsval_object_set_utf8(region, object, (const uint8_t *)"length", 6,
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
				"%s: expected %zu string bytes, got %zu", label,
				expected_len, len);
	}
	if (jsval_string_copy_utf8(region, value, buf, len, NULL) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: failed to copy string", label);
	}
	if (memcmp(buf, expected, len) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: unexpected string bytes", label);
	}
	return GENERATED_TEST_PASS;
}

static int
expect_array_buffer_bytes(jsval_region_t *region, jsval_t value,
		const uint8_t *expected, size_t expected_len, const char *label)
{
	size_t actual_len = 0;
	uint8_t actual_buf[expected_len ? expected_len : 1];

	if (value.kind != JSVAL_KIND_ARRAY_BUFFER) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: expected ArrayBuffer result", label);
	}
	if (jsval_array_buffer_copy_bytes(region, value, NULL, 0, &actual_len) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: failed to measure ArrayBuffer", label);
	}
	if (actual_len != expected_len) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: expected %zu bytes, got %zu", label, expected_len,
				actual_len);
	}
	if (jsval_array_buffer_copy_bytes(region, value, actual_buf, actual_len,
			NULL) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: failed to copy ArrayBuffer bytes", label);
	}
	if (memcmp(actual_buf, expected, expected_len) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: unexpected ArrayBuffer bytes", label);
	}
	return GENERATED_TEST_PASS;
}

int
main(void)
{
	static const uint8_t pbkdf2_zero_derivebits_32[] = {
		0x40, 0x2a, 0xf3, 0x77, 0xcd, 0xb5, 0xeb, 0x7e,
		0x01, 0x7a, 0x58, 0x19, 0xf4, 0x70, 0x85, 0x86,
		0x00, 0xe2, 0xc9, 0xbb, 0x25, 0xb7, 0xae, 0x51,
		0xb0, 0x4b, 0x20, 0x95, 0x83, 0xba, 0x77, 0xcd
	};
	static const uint8_t pbkdf2_zero_derivekey_aes_16[] = {
		0x40, 0x2a, 0xf3, 0x77, 0xcd, 0xb5, 0xeb, 0x7e,
		0x01, 0x7a, 0x58, 0x19, 0xf4, 0x70, 0x85, 0x86
	};
	static const uint8_t pbkdf2_zero_derivekey_hmac_64[] = {
		0x40, 0x2a, 0xf3, 0x77, 0xcd, 0xb5, 0xeb, 0x7e,
		0x01, 0x7a, 0x58, 0x19, 0xf4, 0x70, 0x85, 0x86,
		0x00, 0xe2, 0xc9, 0xbb, 0x25, 0xb7, 0xae, 0x51,
		0xb0, 0x4b, 0x20, 0x95, 0x83, 0xba, 0x77, 0xcd,
		0xd4, 0xbd, 0x77, 0x2c, 0x6d, 0xd0, 0xde, 0x9e,
		0xe4, 0x1c, 0x40, 0xa5, 0x35, 0x0f, 0xdc, 0x86,
		0xb6, 0x12, 0xad, 0xbd, 0xb3, 0xba, 0x67, 0x02,
		0x01, 0xd2, 0x54, 0x7c, 0xc1, 0x9b, 0xda, 0x01
	};
	uint8_t storage[262144];
	jsval_region_t region;
	jsval_t crypto_value;
	jsval_t subtle_value;
	jsval_t derive_usages;
	jsval_t sign_verify_usages;
	jsval_t encrypt_decrypt_usages;
	jsval_t raw_format;
	jsval_t password_input;
	jsval_t password_buffer;
	jsval_t salt_input;
	jsval_t salt_buffer;
	jsval_t pbkdf2_algorithm;
	jsval_t pbkdf2_params;
	jsval_t hmac_algorithm;
	jsval_t aes_algorithm;
	jsval_t import_promise;
	jsval_t pbkdf2_key;
	jsval_t derive_bits_promise;
	jsval_t derive_hmac_promise;
	jsval_t derive_aes_promise;
	jsval_t export_hmac_promise;
	jsval_t export_aes_promise;
	jsval_t derived_hmac_key;
	jsval_t derived_aes_key;
	jsval_t result;
	jsval_t prop;
	jsval_t scratch;
	jsval_promise_state_t state;
	jsmethod_error_t error;
	uint32_t usages_mask = 0;
	int extractable = 0;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_crypto_new(&region, &crypto_value) == 0,
			SUITE, CASE_NAME, "failed to allocate crypto object");
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
	GENERATED_TEST_ASSERT(make_string(&region, "raw", &raw_format) == 0, SUITE,
			CASE_NAME, "failed to build raw format");
	GENERATED_TEST_ASSERT(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8,
			8, &password_input) == 0, SUITE, CASE_NAME,
			"failed to allocate password input");
	GENERATED_TEST_ASSERT(jsval_typed_array_buffer(&region, password_input,
			&password_buffer) == 0, SUITE, CASE_NAME,
			"failed to read password input buffer");
	GENERATED_TEST_ASSERT(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8,
			8, &salt_input) == 0, SUITE, CASE_NAME,
			"failed to allocate salt input");
	GENERATED_TEST_ASSERT(jsval_typed_array_buffer(&region, salt_input,
			&salt_buffer) == 0, SUITE, CASE_NAME,
			"failed to read salt input buffer");
	GENERATED_TEST_ASSERT(make_pbkdf2_algorithm(&region, &pbkdf2_algorithm) == 0,
			SUITE, CASE_NAME, "failed to build PBKDF2 algorithm");
	GENERATED_TEST_ASSERT(make_pbkdf2_params(&region, salt_buffer,
			&pbkdf2_params) == 0, SUITE, CASE_NAME,
			"failed to build PBKDF2 params");
	GENERATED_TEST_ASSERT(make_hmac_algorithm(&region, 1, 512.0,
			&hmac_algorithm) == 0, SUITE, CASE_NAME,
			"failed to build HMAC target algorithm");
	GENERATED_TEST_ASSERT(make_aes_algorithm(&region, 128.0, &aes_algorithm) == 0,
			SUITE, CASE_NAME, "failed to build AES target algorithm");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_import_key(&region, subtle_value,
			raw_format, password_buffer, pbkdf2_algorithm, 1, derive_usages,
			&import_promise) == 0, SUITE, CASE_NAME,
			"failed to call PBKDF2 importKey(raw)");
	GENERATED_TEST_ASSERT(jsval_promise_state(&region, import_promise, &state)
			== 0 && state == JSVAL_PROMISE_STATE_PENDING, SUITE, CASE_NAME,
			"expected PBKDF2 importKey promise to stay pending");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain PBKDF2 importKey");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, import_promise,
			&pbkdf2_key) == 0 && pbkdf2_key.kind == JSVAL_KIND_CRYPTO_KEY, SUITE,
			CASE_NAME, "expected PBKDF2 importKey to resolve CryptoKey");
	GENERATED_TEST_ASSERT(jsval_crypto_key_algorithm(&region, pbkdf2_key, &result)
			== 0 && result.kind == JSVAL_KIND_OBJECT, SUITE, CASE_NAME,
			"expected PBKDF2 base key algorithm object");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"name", 4, &prop) == 0, SUITE, CASE_NAME,
			"failed to read PBKDF2 algorithm name");
	GENERATED_TEST_ASSERT(expect_string(&region, prop, "PBKDF2",
			"pbkdf2.algorithm.name") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected PBKDF2 algorithm name");
	GENERATED_TEST_ASSERT(jsval_crypto_key_usages(&region, pbkdf2_key,
			&usages_mask) == 0, SUITE, CASE_NAME,
			"failed to read PBKDF2 base-key usages");
	GENERATED_TEST_ASSERT(usages_mask == (JSVAL_CRYPTO_KEY_USAGE_DERIVE_BITS
			| JSVAL_CRYPTO_KEY_USAGE_DERIVE_KEY), SUITE, CASE_NAME,
			"unexpected PBKDF2 base-key usages");
	GENERATED_TEST_ASSERT(jsval_crypto_key_extractable(&region, pbkdf2_key,
			&extractable) == 0 && extractable == 1, SUITE, CASE_NAME,
			"expected PBKDF2 base key to be extractable");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_derive_bits(&region, subtle_value,
			pbkdf2_params, pbkdf2_key, jsval_number(256.0),
			&derive_bits_promise) == 0, SUITE, CASE_NAME,
			"failed to call PBKDF2 deriveBits");
	GENERATED_TEST_ASSERT(jsval_promise_state(&region, derive_bits_promise,
			&state) == 0 && state == JSVAL_PROMISE_STATE_PENDING, SUITE,
			CASE_NAME, "expected PBKDF2 deriveBits promise to stay pending");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain PBKDF2 deriveBits");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, derive_bits_promise,
			&result) == 0, SUITE, CASE_NAME,
			"failed to read PBKDF2 deriveBits result");
	GENERATED_TEST_ASSERT(expect_array_buffer_bytes(&region, result,
			pbkdf2_zero_derivebits_32, sizeof(pbkdf2_zero_derivebits_32),
			"deriveBits") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected PBKDF2 deriveBits bytes");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_derive_key(&region, subtle_value,
			pbkdf2_params, pbkdf2_key, hmac_algorithm, 1, sign_verify_usages,
			&derive_hmac_promise) == 0, SUITE, CASE_NAME,
			"failed to call PBKDF2 deriveKey(HMAC)");
	GENERATED_TEST_ASSERT(jsval_promise_state(&region, derive_hmac_promise,
			&state) == 0 && state == JSVAL_PROMISE_STATE_PENDING, SUITE,
			CASE_NAME, "expected PBKDF2 deriveKey(HMAC) promise to stay pending");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain PBKDF2 deriveKey(HMAC)");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, derive_hmac_promise,
			&derived_hmac_key) == 0
			&& derived_hmac_key.kind == JSVAL_KIND_CRYPTO_KEY, SUITE,
			CASE_NAME, "expected PBKDF2 deriveKey(HMAC) CryptoKey");
	GENERATED_TEST_ASSERT(jsval_crypto_key_algorithm(&region, derived_hmac_key,
			&prop) == 0 && prop.kind == JSVAL_KIND_OBJECT, SUITE, CASE_NAME,
			"expected derived HMAC algorithm object");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, prop,
			(const uint8_t *)"name", 4, &scratch) == 0, SUITE, CASE_NAME,
			"failed to read derived HMAC algorithm name");
	GENERATED_TEST_ASSERT(expect_string(&region, scratch, "HMAC",
			"derived-hmac.algorithm.name") == GENERATED_TEST_PASS, SUITE,
			CASE_NAME, "unexpected derived HMAC algorithm name");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, prop,
			(const uint8_t *)"hash", 4, &scratch) == 0, SUITE, CASE_NAME,
			"failed to read derived HMAC hash");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, scratch,
			(const uint8_t *)"name", 4, &scratch) == 0, SUITE, CASE_NAME,
			"failed to read derived HMAC hash.name");
	GENERATED_TEST_ASSERT(expect_string(&region, scratch, "SHA-256",
			"derived-hmac.algorithm.hash.name") == GENERATED_TEST_PASS, SUITE,
			CASE_NAME, "unexpected derived HMAC hash name");
	GENERATED_TEST_ASSERT(jsval_crypto_key_usages(&region, derived_hmac_key,
			&usages_mask) == 0, SUITE, CASE_NAME,
			"failed to read derived HMAC usages");
	GENERATED_TEST_ASSERT(usages_mask == (JSVAL_CRYPTO_KEY_USAGE_SIGN
			| JSVAL_CRYPTO_KEY_USAGE_VERIFY), SUITE, CASE_NAME,
			"unexpected derived HMAC usages");
	GENERATED_TEST_ASSERT(jsval_subtle_crypto_export_key(&region, subtle_value,
			raw_format, derived_hmac_key, &export_hmac_promise) == 0, SUITE,
			CASE_NAME,
			"failed to call exportKey(raw) on derived HMAC key");
	GENERATED_TEST_ASSERT(jsval_promise_state(&region, export_hmac_promise,
			&state) == 0 && state == JSVAL_PROMISE_STATE_PENDING, SUITE,
			CASE_NAME, "expected HMAC exportKey promise to stay pending");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain derived HMAC exportKey(raw)");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, export_hmac_promise,
			&scratch) == 0, SUITE, CASE_NAME,
			"failed to read derived HMAC export result");
	GENERATED_TEST_ASSERT(expect_array_buffer_bytes(&region, scratch,
			pbkdf2_zero_derivekey_hmac_64, sizeof(pbkdf2_zero_derivekey_hmac_64),
			"derived-hmac.exportKey(raw)") == GENERATED_TEST_PASS, SUITE,
			CASE_NAME, "unexpected derived HMAC export bytes");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_derive_key(&region, subtle_value,
			pbkdf2_params, pbkdf2_key, aes_algorithm, 1, encrypt_decrypt_usages,
			&derive_aes_promise) == 0, SUITE, CASE_NAME,
			"failed to call PBKDF2 deriveKey(AES-GCM)");
	GENERATED_TEST_ASSERT(jsval_promise_state(&region, derive_aes_promise,
			&state) == 0 && state == JSVAL_PROMISE_STATE_PENDING, SUITE,
			CASE_NAME, "expected PBKDF2 deriveKey(AES-GCM) promise to stay pending");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain PBKDF2 deriveKey(AES-GCM)");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, derive_aes_promise,
			&derived_aes_key) == 0
			&& derived_aes_key.kind == JSVAL_KIND_CRYPTO_KEY, SUITE,
			CASE_NAME, "expected PBKDF2 deriveKey(AES-GCM) CryptoKey");
	GENERATED_TEST_ASSERT(jsval_crypto_key_algorithm(&region, derived_aes_key,
			&prop) == 0 && prop.kind == JSVAL_KIND_OBJECT, SUITE, CASE_NAME,
			"expected derived AES algorithm object");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, prop,
			(const uint8_t *)"name", 4, &scratch) == 0, SUITE, CASE_NAME,
			"failed to read derived AES algorithm name");
	GENERATED_TEST_ASSERT(expect_string(&region, scratch, "AES-GCM",
			"derived-aes.algorithm.name") == GENERATED_TEST_PASS, SUITE,
			CASE_NAME, "unexpected derived AES algorithm name");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, prop,
			(const uint8_t *)"length", 6, &scratch) == 0
			&& scratch.kind == JSVAL_KIND_NUMBER
			&& scratch.as.number == 128.0, SUITE, CASE_NAME,
			"unexpected derived AES length");
	GENERATED_TEST_ASSERT(jsval_crypto_key_usages(&region, derived_aes_key,
			&usages_mask) == 0, SUITE, CASE_NAME,
			"failed to read derived AES usages");
	GENERATED_TEST_ASSERT(usages_mask == (JSVAL_CRYPTO_KEY_USAGE_ENCRYPT
			| JSVAL_CRYPTO_KEY_USAGE_DECRYPT), SUITE, CASE_NAME,
			"unexpected derived AES usages");
	GENERATED_TEST_ASSERT(jsval_subtle_crypto_export_key(&region, subtle_value,
			raw_format, derived_aes_key, &export_aes_promise) == 0, SUITE,
			CASE_NAME,
			"failed to call exportKey(raw) on derived AES key");
	GENERATED_TEST_ASSERT(jsval_promise_state(&region, export_aes_promise,
			&state) == 0 && state == JSVAL_PROMISE_STATE_PENDING, SUITE,
			CASE_NAME, "expected AES exportKey promise to stay pending");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain derived AES exportKey(raw)");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, export_aes_promise,
			&scratch) == 0, SUITE, CASE_NAME,
			"failed to read derived AES export result");
	GENERATED_TEST_ASSERT(expect_array_buffer_bytes(&region, scratch,
			pbkdf2_zero_derivekey_aes_16, sizeof(pbkdf2_zero_derivekey_aes_16),
			"derived-aes.exportKey(raw)") == GENERATED_TEST_PASS, SUITE,
			CASE_NAME, "unexpected derived AES export bytes");
	return generated_test_pass(SUITE, CASE_NAME);
}
