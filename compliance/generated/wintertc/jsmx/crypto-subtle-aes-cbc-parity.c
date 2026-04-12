#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "wintertc"
#define CASE_NAME "jsmx/crypto-subtle-aes-cbc-parity"

static int
make_string(jsval_region_t *region, const char *text, jsval_t *value_ptr)
{
	return jsval_string_new_utf8(region, (const uint8_t *)text, strlen(text),
			value_ptr);
}

static int
make_usage_array(jsval_region_t *region, int encrypt, int decrypt,
		jsval_t *value_ptr)
{
	jsval_t array;
	jsval_t value;
	size_t cap = (encrypt ? 1u : 0u) + (decrypt ? 1u : 0u);

	if (jsval_array_new(region, cap, &array) < 0) {
		return -1;
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
make_aes_algorithm(jsval_region_t *region, int have_length, double length_value,
		jsval_t *value_ptr)
{
	jsval_t object;
	jsval_t value;

	if (jsval_object_new(region, have_length ? 2 : 1, &object) < 0
			|| make_string(region, "AES-CBC", &value) < 0
			|| jsval_object_set_utf8(region, object, (const uint8_t *)"name", 4,
				value) < 0) {
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
make_aes_cbc_params(jsval_region_t *region, jsval_t iv_buffer,
		jsval_t *value_ptr)
{
	jsval_t object;
	jsval_t value;

	if (jsval_object_new(region, 2, &object) < 0
			|| make_string(region, "AES-CBC", &value) < 0
			|| jsval_object_set_utf8(region, object, (const uint8_t *)"name", 4,
				value) < 0
			|| jsval_object_set_utf8(region, object, (const uint8_t *)"iv", 2,
				iv_buffer) < 0) {
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

static int
expect_string_array(jsval_region_t *region, jsval_t value,
		const char *const *expected, size_t expected_len, const char *label)
{
	size_t i;

	if (value.kind != JSVAL_KIND_ARRAY) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: expected array result", label);
	}
	if (jsval_array_length(region, value) != expected_len) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: expected array length %zu, got %zu", label,
				expected_len, jsval_array_length(region, value));
	}
	for (i = 0; i < expected_len; i++) {
		jsval_t entry;

		if (jsval_array_get(region, value, i, &entry) < 0) {
			return generated_test_fail(SUITE, CASE_NAME,
					"%s: failed to read array entry %zu", label, i);
		}
		if (expect_string(region, entry, expected[i], label)
				!= GENERATED_TEST_PASS) {
			return GENERATED_TEST_WRONG_RESULT;
		}
	}
	return GENERATED_TEST_PASS;
}

int
main(void)
{
	static const uint8_t zero_key_16[16] = { 0 };
	static const char *expected_encrypt_decrypt_ops[] = {
		"encrypt", "decrypt"
	};
	uint8_t storage[262144];
	jsval_region_t region;
	jsval_t crypto_value;
	jsval_t subtle_value;
	jsval_t usages;
	jsval_t aes_algorithm;
	jsval_t aes_import_algorithm;
	jsval_t aes_params;
	jsval_t raw_format;
	jsval_t jwk_format;
	jsval_t input;
	jsval_t input_buffer;
	jsval_t iv;
	jsval_t iv_buffer;
	jsval_t generated_key_promise;
	jsval_t generated_key;
	jsval_t exported_raw_promise;
	jsval_t raw_import_key_promise;
	jsval_t raw_import_key;
	jsval_t jwk_object;
	jsval_t jwk_import_key_promise;
	jsval_t jwk_import_key;
	jsval_t exported_jwk_promise;
	jsval_t encrypt_promise;
	jsval_t decrypt_promise;
	jsval_t result;
	jsval_t prop;
	jsval_promise_state_t state;
	jsmethod_error_t error;
	uint32_t usages_mask = 0;
	size_t byte_length = 0;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_crypto_new(&region, &crypto_value) == 0,
			SUITE, CASE_NAME, "failed to allocate crypto object");
	GENERATED_TEST_ASSERT(jsval_crypto_subtle(&region, crypto_value,
			&subtle_value) == 0, SUITE, CASE_NAME,
			"failed to read crypto.subtle");
	GENERATED_TEST_ASSERT(make_usage_array(&region, 1, 1, &usages) == 0, SUITE,
			CASE_NAME, "failed to build encrypt/decrypt usages");
	GENERATED_TEST_ASSERT(make_aes_algorithm(&region, 1, 256.0, &aes_algorithm)
			== 0, SUITE, CASE_NAME, "failed to build AES-CBC algorithm");
	GENERATED_TEST_ASSERT(make_aes_algorithm(&region, 0, 0.0,
			&aes_import_algorithm) == 0, SUITE, CASE_NAME,
			"failed to build AES-CBC import algorithm");
	GENERATED_TEST_ASSERT(make_string(&region, "raw", &raw_format) == 0, SUITE,
			CASE_NAME, "failed to build raw format");
	GENERATED_TEST_ASSERT(make_string(&region, "jwk", &jwk_format) == 0, SUITE,
			CASE_NAME, "failed to build jwk format");
	GENERATED_TEST_ASSERT(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8,
			16, &input) == 0, SUITE, CASE_NAME,
			"failed to allocate AES plaintext");
	GENERATED_TEST_ASSERT(jsval_typed_array_buffer(&region, input, &input_buffer)
			== 0, SUITE, CASE_NAME, "failed to read AES plaintext buffer");
	GENERATED_TEST_ASSERT(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8,
			16, &iv) == 0, SUITE, CASE_NAME, "failed to allocate AES IV");
	GENERATED_TEST_ASSERT(jsval_typed_array_buffer(&region, iv, &iv_buffer) == 0,
			SUITE, CASE_NAME, "failed to read AES IV buffer");
	GENERATED_TEST_ASSERT(make_aes_cbc_params(&region, iv_buffer, &aes_params)
			== 0, SUITE, CASE_NAME, "failed to build AES-CBC params");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_generate_key(&region, subtle_value,
			aes_algorithm, 1, usages, &generated_key_promise) == 0, SUITE,
			CASE_NAME, "failed to call AES-CBC generateKey");
	GENERATED_TEST_ASSERT(jsval_promise_state(&region, generated_key_promise,
			&state) == 0 && state == JSVAL_PROMISE_STATE_PENDING, SUITE,
			CASE_NAME, "expected AES-CBC generateKey promise to stay pending");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain AES-CBC generateKey microtasks");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, generated_key_promise,
			&generated_key) == 0 && generated_key.kind == JSVAL_KIND_CRYPTO_KEY,
			SUITE, CASE_NAME,
			"expected AES-CBC generateKey to resolve CryptoKey");
	GENERATED_TEST_ASSERT(jsval_crypto_key_algorithm(&region, generated_key,
			&result) == 0 && result.kind == JSVAL_KIND_OBJECT, SUITE, CASE_NAME,
			"expected generated AES-CBC key algorithm object");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"name", 4, &prop) == 0, SUITE, CASE_NAME,
			"failed to read generated AES-CBC algorithm name");
	GENERATED_TEST_ASSERT(expect_string(&region, prop, "AES-CBC",
			"generated algorithm.name") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected generated AES-CBC algorithm name");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"length", 6, &prop) == 0
			&& prop.kind == JSVAL_KIND_NUMBER && prop.as.number == 256.0, SUITE,
			CASE_NAME, "expected generated AES-CBC key length 256");
	GENERATED_TEST_ASSERT(jsval_crypto_key_usages(&region, generated_key,
			&usages_mask) == 0
			&& usages_mask == (JSVAL_CRYPTO_KEY_USAGE_ENCRYPT
				| JSVAL_CRYPTO_KEY_USAGE_DECRYPT), SUITE, CASE_NAME,
			"expected generated AES-CBC key usages encrypt/decrypt");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_export_key(&region, subtle_value,
			raw_format, generated_key, &exported_raw_promise) == 0, SUITE,
			CASE_NAME, "failed to call AES-CBC exportKey(raw)");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain AES-CBC exportKey(raw)");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, exported_raw_promise,
			&result) == 0 && result.kind == JSVAL_KIND_ARRAY_BUFFER, SUITE,
			CASE_NAME, "expected AES-CBC exportKey(raw) ArrayBuffer");
	GENERATED_TEST_ASSERT(jsval_array_buffer_byte_length(&region, result,
			&byte_length) == 0 && byte_length == 32, SUITE, CASE_NAME,
			"expected 32-byte generated AES-CBC raw key");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_import_key(&region, subtle_value,
			raw_format, input_buffer, aes_import_algorithm, 1, usages,
			&raw_import_key_promise) == 0, SUITE, CASE_NAME,
			"failed to call AES-CBC importKey(raw)");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain AES-CBC importKey(raw)");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, raw_import_key_promise,
			&raw_import_key) == 0 && raw_import_key.kind == JSVAL_KIND_CRYPTO_KEY,
			SUITE, CASE_NAME, "expected AES-CBC raw import CryptoKey");
	GENERATED_TEST_ASSERT(jsval_subtle_crypto_export_key(&region, subtle_value,
			raw_format, raw_import_key, &exported_raw_promise) == 0, SUITE,
			CASE_NAME, "failed to re-export imported AES-CBC raw key");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain imported AES-CBC re-export");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, exported_raw_promise,
			&result) == 0, SUITE, CASE_NAME,
			"failed to read imported AES-CBC raw export");
	GENERATED_TEST_ASSERT(expect_array_buffer_bytes(&region, result, zero_key_16,
			sizeof(zero_key_16), "raw import/export") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected imported AES-CBC raw key bytes");

	GENERATED_TEST_ASSERT(jsval_object_new(&region, 5, &jwk_object) == 0, SUITE,
			CASE_NAME, "failed to allocate AES-CBC JWK");
	GENERATED_TEST_ASSERT(make_string(&region, "oct", &result) == 0
			&& jsval_object_set_utf8(&region, jwk_object,
				(const uint8_t *)"kty", 3, result) == 0
			&& make_string(&region, "AAAAAAAAAAAAAAAAAAAAAA", &result) == 0
			&& jsval_object_set_utf8(&region, jwk_object,
				(const uint8_t *)"k", 1, result) == 0
			&& make_string(&region, "A128CBC", &result) == 0
			&& jsval_object_set_utf8(&region, jwk_object,
				(const uint8_t *)"alg", 3, result) == 0
			&& jsval_object_set_utf8(&region, jwk_object,
				(const uint8_t *)"ext", 3, jsval_bool(1)) == 0
			&& jsval_object_set_utf8(&region, jwk_object,
				(const uint8_t *)"key_ops", 7, usages) == 0, SUITE, CASE_NAME,
			"failed to populate AES-CBC JWK");
	GENERATED_TEST_ASSERT(jsval_subtle_crypto_import_key(&region, subtle_value,
			jwk_format, jwk_object, aes_import_algorithm, 1, usages,
			&jwk_import_key_promise) == 0, SUITE, CASE_NAME,
			"failed to call AES-CBC importKey(jwk)");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain AES-CBC importKey(jwk)");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, jwk_import_key_promise,
			&jwk_import_key) == 0 && jwk_import_key.kind == JSVAL_KIND_CRYPTO_KEY,
			SUITE, CASE_NAME, "expected AES-CBC JWK import CryptoKey");
	GENERATED_TEST_ASSERT(jsval_subtle_crypto_export_key(&region, subtle_value,
			jwk_format, jwk_import_key, &exported_jwk_promise) == 0, SUITE,
			CASE_NAME, "failed to call AES-CBC exportKey(jwk)");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain AES-CBC exportKey(jwk)");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, exported_jwk_promise,
			&result) == 0 && result.kind == JSVAL_KIND_OBJECT, SUITE, CASE_NAME,
			"expected AES-CBC JWK export object");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"alg", 3, &prop) == 0, SUITE, CASE_NAME,
			"failed to read AES-CBC exported JWK alg");
	GENERATED_TEST_ASSERT(expect_string(&region, prop, "A128CBC",
			"exported JWK alg") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected AES-CBC exported JWK alg");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"key_ops", 7, &prop) == 0, SUITE, CASE_NAME,
			"failed to read AES-CBC exported JWK key_ops");
	GENERATED_TEST_ASSERT(expect_string_array(&region, prop,
			expected_encrypt_decrypt_ops, 2, "exported JWK key_ops")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected AES-CBC exported JWK key_ops");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_encrypt(&region, subtle_value,
			aes_params, raw_import_key, input_buffer, &encrypt_promise) == 0,
			SUITE, CASE_NAME, "failed to call AES-CBC encrypt");
	GENERATED_TEST_ASSERT(jsval_promise_state(&region, encrypt_promise, &state)
			== 0 && state == JSVAL_PROMISE_STATE_PENDING, SUITE, CASE_NAME,
			"expected AES-CBC encrypt promise to stay pending");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain AES-CBC encrypt");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, encrypt_promise, &result)
			== 0, SUITE, CASE_NAME, "failed to read AES-CBC encrypt result");
	GENERATED_TEST_ASSERT(result.kind == JSVAL_KIND_ARRAY_BUFFER
			&& jsval_array_buffer_byte_length(&region, result, &byte_length) == 0
			&& byte_length == 32, SUITE, CASE_NAME,
			"expected 32-byte AES-CBC ciphertext (16-byte plaintext + PKCS#7 padding block)");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_decrypt(&region, subtle_value,
			aes_params, raw_import_key, result, &decrypt_promise) == 0, SUITE,
			CASE_NAME, "failed to call AES-CBC decrypt");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain AES-CBC decrypt");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, decrypt_promise, &result)
			== 0, SUITE, CASE_NAME, "failed to read AES-CBC decrypt result");
	GENERATED_TEST_ASSERT(expect_array_buffer_bytes(&region, result, zero_key_16,
			sizeof(zero_key_16), "decrypt") == GENERATED_TEST_PASS, SUITE,
			CASE_NAME, "unexpected AES-CBC decrypt bytes");
	return generated_test_pass(SUITE, CASE_NAME);
}
