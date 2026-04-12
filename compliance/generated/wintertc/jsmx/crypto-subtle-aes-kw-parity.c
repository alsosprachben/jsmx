#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "wintertc"
#define CASE_NAME "jsmx/crypto-subtle-aes-kw-parity"

static int
make_string(jsval_region_t *region, const char *text, jsval_t *value_ptr)
{
	return jsval_string_new_utf8(region, (const uint8_t *)text, strlen(text),
			value_ptr);
}

static int
make_wrap_unwrap_usages(jsval_region_t *region, jsval_t *value_ptr)
{
	jsval_t array;
	jsval_t value;

	if (jsval_array_new(region, 2, &array) < 0
			|| make_string(region, "wrapKey", &value) < 0
			|| jsval_array_push(region, array, value) < 0
			|| make_string(region, "unwrapKey", &value) < 0
			|| jsval_array_push(region, array, value) < 0) {
		return -1;
	}
	*value_ptr = array;
	return 0;
}

static int
make_encrypt_decrypt_usages(jsval_region_t *region, jsval_t *value_ptr)
{
	jsval_t array;
	jsval_t value;

	if (jsval_array_new(region, 2, &array) < 0
			|| make_string(region, "encrypt", &value) < 0
			|| jsval_array_push(region, array, value) < 0
			|| make_string(region, "decrypt", &value) < 0
			|| jsval_array_push(region, array, value) < 0) {
		return -1;
	}
	*value_ptr = array;
	return 0;
}

static int
make_aes_kw_algorithm(jsval_region_t *region, int have_length,
		double length_value, jsval_t *value_ptr)
{
	jsval_t object;
	jsval_t value;

	if (jsval_object_new(region, have_length ? 2 : 1, &object) < 0
			|| make_string(region, "AES-KW", &value) < 0
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
make_aes_gcm_algorithm(jsval_region_t *region, double length_value,
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

int
main(void)
{
	uint8_t storage[262144];
	jsval_region_t region;
	jsval_t crypto_value;
	jsval_t subtle_value;
	jsval_t wrap_usages;
	jsval_t enc_usages;
	jsval_t kw_algorithm;
	jsval_t kw_import_algorithm;
	jsval_t aes_gcm_algorithm;
	jsval_t raw_format;
	jsval_t jwk_format;
	jsval_t input;
	jsval_t input_buffer;
	jsval_t generated_promise;
	jsval_t generated_key;
	jsval_t exported_raw_promise;
	jsval_t raw_imported_promise;
	jsval_t raw_imported_key;
	jsval_t jwk_object;
	jsval_t jwk_imported_promise;
	jsval_t jwk_imported_key;
	jsval_t inner_promise;
	jsval_t inner_key;
	jsval_t wrap_promise;
	jsval_t wrapped_buf;
	jsval_t unwrap_promise;
	jsval_t unwrapped_key;
	jsval_t result;
	jsval_t prop;
	jsmethod_error_t error;
	uint32_t usages_mask = 0;
	size_t byte_length = 0;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_crypto_new(&region, &crypto_value) == 0,
			SUITE, CASE_NAME, "failed to allocate crypto object");
	GENERATED_TEST_ASSERT(jsval_crypto_subtle(&region, crypto_value,
			&subtle_value) == 0, SUITE, CASE_NAME,
			"failed to read crypto.subtle");
	GENERATED_TEST_ASSERT(make_wrap_unwrap_usages(&region, &wrap_usages) == 0,
			SUITE, CASE_NAME, "failed to build wrap/unwrap usages");
	GENERATED_TEST_ASSERT(make_encrypt_decrypt_usages(&region, &enc_usages)
			== 0, SUITE, CASE_NAME, "failed to build encrypt/decrypt usages");
	GENERATED_TEST_ASSERT(make_aes_kw_algorithm(&region, 1, 128.0,
			&kw_algorithm) == 0, SUITE, CASE_NAME,
			"failed to build AES-KW algorithm");
	GENERATED_TEST_ASSERT(make_aes_kw_algorithm(&region, 0, 0.0,
			&kw_import_algorithm) == 0, SUITE, CASE_NAME,
			"failed to build AES-KW import algorithm");
	GENERATED_TEST_ASSERT(make_aes_gcm_algorithm(&region, 128.0,
			&aes_gcm_algorithm) == 0, SUITE, CASE_NAME,
			"failed to build AES-GCM inner algorithm");
	GENERATED_TEST_ASSERT(make_string(&region, "raw", &raw_format) == 0, SUITE,
			CASE_NAME, "failed to build raw format");
	GENERATED_TEST_ASSERT(make_string(&region, "jwk", &jwk_format) == 0, SUITE,
			CASE_NAME, "failed to build jwk format");
	GENERATED_TEST_ASSERT(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8,
			16, &input) == 0, SUITE, CASE_NAME, "failed to allocate input");
	GENERATED_TEST_ASSERT(jsval_typed_array_buffer(&region, input, &input_buffer)
			== 0, SUITE, CASE_NAME, "failed to read input buffer");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_generate_key(&region, subtle_value,
			kw_algorithm, 1, wrap_usages, &generated_promise) == 0, SUITE,
			CASE_NAME, "failed to call AES-KW generateKey");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain AES-KW generateKey");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, generated_promise,
			&generated_key) == 0 && generated_key.kind == JSVAL_KIND_CRYPTO_KEY,
			SUITE, CASE_NAME,
			"expected AES-KW generateKey to resolve CryptoKey");
	GENERATED_TEST_ASSERT(jsval_crypto_key_algorithm(&region, generated_key,
			&result) == 0 && result.kind == JSVAL_KIND_OBJECT, SUITE, CASE_NAME,
			"expected generated AES-KW key algorithm object");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"name", 4, &prop) == 0, SUITE, CASE_NAME,
			"failed to read AES-KW algorithm name");
	GENERATED_TEST_ASSERT(expect_string(&region, prop, "AES-KW",
			"generated algorithm.name") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected AES-KW algorithm name");
	GENERATED_TEST_ASSERT(jsval_crypto_key_usages(&region, generated_key,
			&usages_mask) == 0
			&& usages_mask == (JSVAL_CRYPTO_KEY_USAGE_WRAP_KEY
				| JSVAL_CRYPTO_KEY_USAGE_UNWRAP_KEY), SUITE, CASE_NAME,
			"expected generated AES-KW key usages wrapKey/unwrapKey");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_export_key(&region, subtle_value,
			raw_format, generated_key, &exported_raw_promise) == 0, SUITE,
			CASE_NAME, "failed to call AES-KW exportKey(raw)");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain AES-KW exportKey(raw)");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, exported_raw_promise,
			&result) == 0 && result.kind == JSVAL_KIND_ARRAY_BUFFER, SUITE,
			CASE_NAME, "expected AES-KW exportKey(raw) ArrayBuffer");
	GENERATED_TEST_ASSERT(jsval_array_buffer_byte_length(&region, result,
			&byte_length) == 0 && byte_length == 16, SUITE, CASE_NAME,
			"expected 16-byte exported AES-KW raw key");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_import_key(&region, subtle_value,
			raw_format, input_buffer, kw_import_algorithm, 1, wrap_usages,
			&raw_imported_promise) == 0, SUITE, CASE_NAME,
			"failed to call AES-KW importKey(raw)");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain AES-KW importKey(raw)");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, raw_imported_promise,
			&raw_imported_key) == 0
			&& raw_imported_key.kind == JSVAL_KIND_CRYPTO_KEY, SUITE, CASE_NAME,
			"expected AES-KW raw import CryptoKey");

	GENERATED_TEST_ASSERT(jsval_object_new(&region, 5, &jwk_object) == 0, SUITE,
			CASE_NAME, "failed to allocate AES-KW JWK");
	GENERATED_TEST_ASSERT(make_string(&region, "oct", &result) == 0
			&& jsval_object_set_utf8(&region, jwk_object,
				(const uint8_t *)"kty", 3, result) == 0
			&& make_string(&region, "AAAAAAAAAAAAAAAAAAAAAA", &result) == 0
			&& jsval_object_set_utf8(&region, jwk_object,
				(const uint8_t *)"k", 1, result) == 0
			&& make_string(&region, "A128KW", &result) == 0
			&& jsval_object_set_utf8(&region, jwk_object,
				(const uint8_t *)"alg", 3, result) == 0
			&& jsval_object_set_utf8(&region, jwk_object,
				(const uint8_t *)"ext", 3, jsval_bool(1)) == 0
			&& jsval_object_set_utf8(&region, jwk_object,
				(const uint8_t *)"key_ops", 7, wrap_usages) == 0, SUITE,
			CASE_NAME, "failed to populate AES-KW JWK");
	GENERATED_TEST_ASSERT(jsval_subtle_crypto_import_key(&region, subtle_value,
			jwk_format, jwk_object, kw_import_algorithm, 1, wrap_usages,
			&jwk_imported_promise) == 0, SUITE, CASE_NAME,
			"failed to call AES-KW importKey(jwk)");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain AES-KW importKey(jwk)");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, jwk_imported_promise,
			&jwk_imported_key) == 0
			&& jwk_imported_key.kind == JSVAL_KIND_CRYPTO_KEY, SUITE, CASE_NAME,
			"expected AES-KW JWK import CryptoKey");

	/* Generate an AES-GCM inner key, wrap it, then unwrap it back. */
	GENERATED_TEST_ASSERT(jsval_subtle_crypto_generate_key(&region, subtle_value,
			aes_gcm_algorithm, 1, enc_usages, &inner_promise) == 0, SUITE,
			CASE_NAME, "failed to generate AES-GCM inner key");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain AES-GCM inner generateKey");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, inner_promise,
			&inner_key) == 0 && inner_key.kind == JSVAL_KIND_CRYPTO_KEY,
			SUITE, CASE_NAME, "expected inner AES-GCM CryptoKey");
	GENERATED_TEST_ASSERT(jsval_subtle_crypto_wrap_key(&region, subtle_value,
			raw_format, inner_key, generated_key, kw_algorithm,
			&wrap_promise) == 0, SUITE, CASE_NAME,
			"failed to call wrapKey for AES-GCM inner key");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain wrapKey");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, wrap_promise,
			&wrapped_buf) == 0 && wrapped_buf.kind == JSVAL_KIND_ARRAY_BUFFER,
			SUITE, CASE_NAME, "expected wrapKey to resolve ArrayBuffer");
	GENERATED_TEST_ASSERT(jsval_array_buffer_byte_length(&region, wrapped_buf,
			&byte_length) == 0 && byte_length == 24, SUITE, CASE_NAME,
			"expected 24-byte wrapped AES-128 key");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_unwrap_key(&region, subtle_value,
			raw_format, wrapped_buf, generated_key, kw_algorithm,
			aes_gcm_algorithm, 1, enc_usages, &unwrap_promise) == 0, SUITE,
			CASE_NAME, "failed to call unwrapKey for AES-GCM inner key");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain unwrapKey");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, unwrap_promise,
			&unwrapped_key) == 0
			&& unwrapped_key.kind == JSVAL_KIND_CRYPTO_KEY, SUITE, CASE_NAME,
			"expected unwrapKey to resolve CryptoKey");
	GENERATED_TEST_ASSERT(jsval_crypto_key_algorithm(&region, unwrapped_key,
			&result) == 0 && result.kind == JSVAL_KIND_OBJECT, SUITE, CASE_NAME,
			"expected unwrapped AES-GCM algorithm object");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"name", 4, &prop) == 0, SUITE, CASE_NAME,
			"failed to read unwrapped algorithm name");
	GENERATED_TEST_ASSERT(expect_string(&region, prop, "AES-GCM",
			"unwrapped algorithm.name") == GENERATED_TEST_PASS, SUITE,
			CASE_NAME, "unexpected unwrapped algorithm name");
	return generated_test_pass(SUITE, CASE_NAME);
}
