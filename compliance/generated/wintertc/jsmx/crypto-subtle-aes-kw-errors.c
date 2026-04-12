#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "wintertc"
#define CASE_NAME "jsmx/crypto-subtle-aes-kw-errors"

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
				"%s: expected DOMException name %s", label, expected);
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
	uint8_t storage[262144];
	jsval_region_t region;
	jsval_t crypto_value;
	jsval_t subtle_value;
	jsval_t wrap_usages;
	jsval_t enc_usages;
	jsval_t kw_algorithm;
	jsval_t kw_import_algorithm;
	jsval_t aes_gcm_algorithm;
	jsval_t raw_input;
	jsval_t raw_input_buffer;
	jsval_t short_wrapped;
	jsval_t short_wrapped_buffer;
	jsval_t valid_wrapped;
	jsval_t valid_wrapped_buffer;
	jsval_t raw_format;
	jsval_t jwk_format;
	jsval_t pkcs8_format;
	jsval_t non_extractable_promise;
	jsval_t non_extractable_key;
	jsval_t non_extractable_export_promise;
	jsval_t unsupported_format_promise;
	jsval_t malformed_jwk_promise;
	jsval_t generated_promise;
	jsval_t generated_key;
	jsval_t short_unwrap_promise;
	jsval_t bad_inner_alg;
	jsval_t bad_inner_unwrap_promise;
	jsval_t jwk_object;
	jsval_t result;
	jsmethod_error_t error;

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
	GENERATED_TEST_ASSERT(jsval_object_new(&region, 2, &aes_gcm_algorithm) == 0
			&& make_string(&region, "AES-GCM", &result) == 0
			&& jsval_object_set_utf8(&region, aes_gcm_algorithm,
				(const uint8_t *)"name", 4, result) == 0
			&& jsval_object_set_utf8(&region, aes_gcm_algorithm,
				(const uint8_t *)"length", 6, jsval_number(128.0)) == 0, SUITE,
			CASE_NAME, "failed to build AES-GCM inner algorithm");
	GENERATED_TEST_ASSERT(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8,
			16, &raw_input) == 0, SUITE, CASE_NAME,
			"failed to allocate AES-KW raw input");
	GENERATED_TEST_ASSERT(jsval_typed_array_buffer(&region, raw_input,
			&raw_input_buffer) == 0, SUITE, CASE_NAME,
			"failed to read AES-KW raw input buffer");
	GENERATED_TEST_ASSERT(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8,
			7, &short_wrapped) == 0, SUITE, CASE_NAME,
			"failed to allocate short wrapped buffer");
	GENERATED_TEST_ASSERT(jsval_typed_array_buffer(&region, short_wrapped,
			&short_wrapped_buffer) == 0, SUITE, CASE_NAME,
			"failed to read short wrapped buffer");
	GENERATED_TEST_ASSERT(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8,
			24, &valid_wrapped) == 0, SUITE, CASE_NAME,
			"failed to allocate valid wrapped buffer");
	GENERATED_TEST_ASSERT(jsval_typed_array_buffer(&region, valid_wrapped,
			&valid_wrapped_buffer) == 0, SUITE, CASE_NAME,
			"failed to read valid wrapped buffer");
	GENERATED_TEST_ASSERT(make_string(&region, "raw", &raw_format) == 0, SUITE,
			CASE_NAME, "failed to build raw format");
	GENERATED_TEST_ASSERT(make_string(&region, "jwk", &jwk_format) == 0, SUITE,
			CASE_NAME, "failed to build jwk format");
	GENERATED_TEST_ASSERT(make_string(&region, "pkcs8", &pkcs8_format) == 0,
			SUITE, CASE_NAME, "failed to build pkcs8 format");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_generate_key(&region, subtle_value,
			kw_algorithm, 0, wrap_usages, &non_extractable_promise) == 0, SUITE,
			CASE_NAME, "failed to call nonextractable AES-KW generateKey");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain nonextractable AES-KW generateKey");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, non_extractable_promise,
			&non_extractable_key) == 0, SUITE, CASE_NAME,
			"failed to read nonextractable AES-KW key");
	GENERATED_TEST_ASSERT(jsval_subtle_crypto_export_key(&region, subtle_value,
			raw_format, non_extractable_key, &non_extractable_export_promise)
			== 0, SUITE, CASE_NAME,
			"failed to call exportKey on nonextractable AES-KW key");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region,
			non_extractable_export_promise, &result) == 0, SUITE, CASE_NAME,
			"failed to read nonextractable AES-KW export rejection");
	GENERATED_TEST_ASSERT(expect_dom_exception_name(&region, result,
			"InvalidAccessError", "exportKey nonextractable")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected nonextractable AES-KW export rejection");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_import_key(&region, subtle_value,
			pkcs8_format, raw_input_buffer, kw_import_algorithm, 1, wrap_usages,
			&unsupported_format_promise) == 0, SUITE, CASE_NAME,
			"failed to call AES-KW importKey with unsupported format");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, unsupported_format_promise,
			&result) == 0, SUITE, CASE_NAME,
			"failed to read AES-KW unsupported-format rejection");
	GENERATED_TEST_ASSERT(expect_dom_exception_name(&region, result,
			"NotSupportedError", "importKey unsupported format")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected AES-KW unsupported-format rejection");

	GENERATED_TEST_ASSERT(jsval_object_new(&region, 5, &jwk_object) == 0, SUITE,
			CASE_NAME, "failed to allocate malformed AES-KW JWK");
	GENERATED_TEST_ASSERT(make_string(&region, "RSA", &result) == 0
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
			CASE_NAME, "failed to populate malformed AES-KW JWK");
	GENERATED_TEST_ASSERT(jsval_subtle_crypto_import_key(&region, subtle_value,
			jwk_format, jwk_object, kw_import_algorithm, 1, wrap_usages,
			&malformed_jwk_promise) == 0, SUITE, CASE_NAME,
			"failed to call AES-KW importKey with malformed JWK");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, malformed_jwk_promise,
			&result) == 0, SUITE, CASE_NAME,
			"failed to read malformed AES-KW JWK rejection");
	GENERATED_TEST_ASSERT(expect_dom_exception_name(&region, result, "DataError",
			"importKey malformed JWK") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected AES-KW malformed-JWK rejection");

	/* Generate a working wrapping key for the unwrap negative tests. */
	GENERATED_TEST_ASSERT(jsval_subtle_crypto_generate_key(&region, subtle_value,
			kw_algorithm, 1, wrap_usages, &generated_promise) == 0, SUITE,
			CASE_NAME, "failed to generate AES-KW wrapping key");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain wrapping-key generateKey");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, generated_promise,
			&generated_key) == 0
			&& generated_key.kind == JSVAL_KIND_CRYPTO_KEY, SUITE, CASE_NAME,
			"expected wrapping-key CryptoKey");

	/* Unwrap of too-short wrapped buffer -> OperationError. */
	GENERATED_TEST_ASSERT(jsval_subtle_crypto_unwrap_key(&region, subtle_value,
			raw_format, short_wrapped_buffer, generated_key, kw_algorithm,
			aes_gcm_algorithm, 1, enc_usages, &short_unwrap_promise) == 0,
			SUITE, CASE_NAME,
			"failed to call unwrapKey with too-short buffer");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, short_unwrap_promise,
			&result) == 0, SUITE, CASE_NAME,
			"failed to read short-unwrap rejection");
	GENERATED_TEST_ASSERT(expect_dom_exception_name(&region, result,
			"OperationError", "unwrapKey short buffer")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected short-unwrap rejection");

	/* Unwrap with an unsupported inner algorithm -> NotSupportedError. */
	GENERATED_TEST_ASSERT(jsval_object_new(&region, 1, &bad_inner_alg) == 0
			&& make_string(&region, "RSA-OAEP", &result) == 0
			&& jsval_object_set_utf8(&region, bad_inner_alg,
				(const uint8_t *)"name", 4, result) == 0, SUITE, CASE_NAME,
			"failed to build unsupported inner algorithm");
	GENERATED_TEST_ASSERT(jsval_subtle_crypto_unwrap_key(&region, subtle_value,
			raw_format, valid_wrapped_buffer, generated_key, kw_algorithm,
			bad_inner_alg, 1, enc_usages, &bad_inner_unwrap_promise) == 0,
			SUITE, CASE_NAME,
			"failed to call unwrapKey with unsupported inner algorithm");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, bad_inner_unwrap_promise,
			&result) == 0, SUITE, CASE_NAME,
			"failed to read unsupported-inner unwrap rejection");
	GENERATED_TEST_ASSERT(expect_dom_exception_name(&region, result,
			"NotSupportedError", "unwrapKey unsupported inner")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected unsupported-inner unwrap rejection");

	/*
	 * JWK unwrap with garbage ciphertext: supply an all-zero 24-byte
	 * buffer which the AES-KW unwrap will likely reject at the AIV
	 * check. Even if AIV passes, the decrypted bytes won't parse as
	 * JSON. Either way the result is a DOMException rejection.
	 */
	{
		jsval_t jwk_garbage_promise;

		GENERATED_TEST_ASSERT(jsval_subtle_crypto_unwrap_key(&region,
				subtle_value, jwk_format, valid_wrapped_buffer, generated_key,
				kw_algorithm, aes_gcm_algorithm, 1, enc_usages,
				&jwk_garbage_promise) == 0, SUITE, CASE_NAME,
				"failed to call unwrapKey(jwk) with garbage ciphertext");
		memset(&error, 0, sizeof(error));
		GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0,
				SUITE, CASE_NAME,
				"failed to drain unwrapKey(jwk) garbage");
		GENERATED_TEST_ASSERT(jsval_promise_result(&region, jwk_garbage_promise,
				&result) == 0 && result.kind == JSVAL_KIND_DOM_EXCEPTION,
				SUITE, CASE_NAME,
				"expected DOMException for garbage jwk unwrap");
	}
	return generated_test_pass(SUITE, CASE_NAME);
}
