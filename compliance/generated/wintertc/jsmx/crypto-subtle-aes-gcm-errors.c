#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "wintertc"
#define CASE_NAME "jsmx/crypto-subtle-aes-gcm-errors"

static int
make_string(jsval_region_t *region, const char *text, jsval_t *value_ptr)
{
	return jsval_string_new_utf8(region, (const uint8_t *)text, strlen(text),
			value_ptr);
}

static int
make_usage_array(jsval_region_t *region, jsval_t *value_ptr)
{
	jsval_t array;
	jsval_t value;

	if (jsval_array_new(region, 2, &array) < 0) {
		return -1;
	}
	if (make_string(region, "encrypt", &value) < 0
			|| jsval_array_push(region, array, value) < 0
			|| make_string(region, "decrypt", &value) < 0
			|| jsval_array_push(region, array, value) < 0) {
		return -1;
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
			|| make_string(region, "AES-GCM", &value) < 0
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
	jsval_t usages;
	jsval_t aes_algorithm;
	jsval_t aes_import_algorithm;
	jsval_t raw_input;
	jsval_t raw_input_buffer;
	jsval_t iv;
	jsval_t iv_buffer;
	jsval_t params;
	jsval_t invalid_iv_params;
	jsval_t invalid_tag_params;
	jsval_t raw_format;
	jsval_t jwk_format;
	jsval_t pkcs8_format;
	jsval_t non_extractable_promise;
	jsval_t non_extractable_key;
	jsval_t non_extractable_export_promise;
	jsval_t unsupported_format_promise;
	jsval_t malformed_jwk_promise;
	jsval_t raw_import_key_promise;
	jsval_t raw_import_key;
	jsval_t invalid_iv_promise;
	jsval_t invalid_tag_promise;
	jsval_t bad_decrypt_key_promise;
	jsval_t bad_decrypt_key;
	jsval_t encrypt_promise;
	jsval_t bad_decrypt_promise;
	jsval_t wrap_no_usage_promise;
	jsval_t jwk_object;
	jsval_t result;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_crypto_new(&region, &crypto_value) == 0,
			SUITE, CASE_NAME, "failed to allocate crypto object");
	GENERATED_TEST_ASSERT(jsval_crypto_subtle(&region, crypto_value,
			&subtle_value) == 0, SUITE, CASE_NAME,
			"failed to read crypto.subtle");
	GENERATED_TEST_ASSERT(make_usage_array(&region, &usages) == 0, SUITE,
			CASE_NAME, "failed to build AES usages");
	GENERATED_TEST_ASSERT(make_aes_algorithm(&region, 1, 128.0, &aes_algorithm)
			== 0, SUITE, CASE_NAME, "failed to build AES generate algorithm");
	GENERATED_TEST_ASSERT(make_aes_algorithm(&region, 0, 0.0,
			&aes_import_algorithm) == 0, SUITE, CASE_NAME,
			"failed to build AES import algorithm");
	GENERATED_TEST_ASSERT(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8,
			16, &raw_input) == 0, SUITE, CASE_NAME,
			"failed to allocate AES raw input");
	GENERATED_TEST_ASSERT(jsval_typed_array_buffer(&region, raw_input,
			&raw_input_buffer) == 0, SUITE, CASE_NAME,
			"failed to read AES raw input buffer");
	GENERATED_TEST_ASSERT(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8,
			12, &iv) == 0, SUITE, CASE_NAME, "failed to allocate AES IV");
	GENERATED_TEST_ASSERT(jsval_typed_array_buffer(&region, iv, &iv_buffer) == 0,
			SUITE, CASE_NAME, "failed to read AES IV buffer");
	GENERATED_TEST_ASSERT(jsval_object_new(&region, 2, &params) == 0, SUITE,
			CASE_NAME, "failed to allocate AES params");
	GENERATED_TEST_ASSERT(make_string(&region, "AES-GCM", &result) == 0
			&& jsval_object_set_utf8(&region, params,
				(const uint8_t *)"name", 4, result) == 0
			&& jsval_object_set_utf8(&region, params,
				(const uint8_t *)"iv", 2, iv_buffer) == 0, SUITE, CASE_NAME,
			"failed to populate AES params");
	GENERATED_TEST_ASSERT(make_string(&region, "raw", &raw_format) == 0, SUITE,
			CASE_NAME, "failed to build raw format");
	GENERATED_TEST_ASSERT(make_string(&region, "jwk", &jwk_format) == 0, SUITE,
			CASE_NAME, "failed to build jwk format");
	GENERATED_TEST_ASSERT(make_string(&region, "pkcs8", &pkcs8_format) == 0,
			SUITE, CASE_NAME, "failed to build pkcs8 format");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_generate_key(&region, subtle_value,
			aes_algorithm, 0, usages, &non_extractable_promise) == 0, SUITE,
			CASE_NAME, "failed to call nonextractable AES generateKey");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain nonextractable AES generateKey");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, non_extractable_promise,
			&non_extractable_key) == 0, SUITE, CASE_NAME,
			"failed to read nonextractable AES key");
	GENERATED_TEST_ASSERT(jsval_subtle_crypto_export_key(&region, subtle_value,
			raw_format, non_extractable_key, &non_extractable_export_promise) == 0,
			SUITE, CASE_NAME, "failed to call exportKey on nonextractable AES key");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region,
			non_extractable_export_promise, &result) == 0, SUITE, CASE_NAME,
			"failed to read nonextractable AES export rejection");
	GENERATED_TEST_ASSERT(expect_dom_exception_name(&region, result,
			"InvalidAccessError", "exportKey nonextractable")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected nonextractable AES export rejection");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_import_key(&region, subtle_value,
			pkcs8_format, raw_input_buffer, aes_import_algorithm, 1, usages,
			&unsupported_format_promise) == 0, SUITE, CASE_NAME,
			"failed to call AES importKey with unsupported format");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, unsupported_format_promise,
			&result) == 0, SUITE, CASE_NAME,
			"failed to read AES unsupported-format rejection");
	GENERATED_TEST_ASSERT(expect_dom_exception_name(&region, result,
			"NotSupportedError", "importKey unsupported format")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected AES unsupported-format rejection");

	GENERATED_TEST_ASSERT(jsval_object_new(&region, 5, &jwk_object) == 0, SUITE,
			CASE_NAME, "failed to allocate malformed AES JWK");
	GENERATED_TEST_ASSERT(make_string(&region, "RSA", &result) == 0
			&& jsval_object_set_utf8(&region, jwk_object,
				(const uint8_t *)"kty", 3, result) == 0
			&& make_string(&region, "AAAAAAAAAAAAAAAAAAAAAA", &result) == 0
			&& jsval_object_set_utf8(&region, jwk_object,
				(const uint8_t *)"k", 1, result) == 0
			&& make_string(&region, "A128GCM", &result) == 0
			&& jsval_object_set_utf8(&region, jwk_object,
				(const uint8_t *)"alg", 3, result) == 0
			&& jsval_object_set_utf8(&region, jwk_object,
				(const uint8_t *)"ext", 3, jsval_bool(1)) == 0
			&& jsval_object_set_utf8(&region, jwk_object,
				(const uint8_t *)"key_ops", 7, usages) == 0, SUITE, CASE_NAME,
			"failed to populate malformed AES JWK");
	GENERATED_TEST_ASSERT(jsval_subtle_crypto_import_key(&region, subtle_value,
			jwk_format, jwk_object, aes_import_algorithm, 1, usages,
			&malformed_jwk_promise) == 0, SUITE, CASE_NAME,
			"failed to call AES importKey with malformed JWK");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, malformed_jwk_promise,
			&result) == 0, SUITE, CASE_NAME,
			"failed to read malformed AES JWK rejection");
	GENERATED_TEST_ASSERT(expect_dom_exception_name(&region, result, "DataError",
			"importKey malformed JWK") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected AES malformed-JWK rejection");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_import_key(&region, subtle_value,
			raw_format, raw_input_buffer, aes_import_algorithm, 1, usages,
			&raw_import_key_promise) == 0, SUITE, CASE_NAME,
			"failed to call AES importKey(raw)");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain AES importKey(raw)");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, raw_import_key_promise,
			&raw_import_key) == 0 && raw_import_key.kind == JSVAL_KIND_CRYPTO_KEY,
			SUITE, CASE_NAME, "expected AES raw import CryptoKey");

	GENERATED_TEST_ASSERT(jsval_object_new(&region, 1, &invalid_iv_params) == 0,
			SUITE, CASE_NAME, "failed to allocate invalid-iv params");
	GENERATED_TEST_ASSERT(make_string(&region, "AES-GCM", &result) == 0
			&& jsval_object_set_utf8(&region, invalid_iv_params,
				(const uint8_t *)"name", 4, result) == 0, SUITE, CASE_NAME,
			"failed to populate invalid-iv params");
	GENERATED_TEST_ASSERT(jsval_subtle_crypto_encrypt(&region, subtle_value,
			invalid_iv_params, raw_import_key, raw_input_buffer,
			&invalid_iv_promise) == 0, SUITE, CASE_NAME,
			"failed to call AES encrypt with missing iv");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, invalid_iv_promise,
			&result) == 0, SUITE, CASE_NAME,
			"failed to read invalid-iv rejection");
	GENERATED_TEST_ASSERT(expect_dom_exception_name(&region, result, "TypeError",
			"encrypt missing iv") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected AES missing-iv rejection");

	GENERATED_TEST_ASSERT(jsval_object_new(&region, 3, &invalid_tag_params) == 0,
			SUITE, CASE_NAME, "failed to allocate invalid-tag params");
	GENERATED_TEST_ASSERT(make_string(&region, "AES-GCM", &result) == 0
			&& jsval_object_set_utf8(&region, invalid_tag_params,
				(const uint8_t *)"name", 4, result) == 0
			&& jsval_object_set_utf8(&region, invalid_tag_params,
				(const uint8_t *)"iv", 2, iv_buffer) == 0
			&& jsval_object_set_utf8(&region, invalid_tag_params,
				(const uint8_t *)"tagLength", 9, jsval_number(40.0)) == 0, SUITE,
			CASE_NAME, "failed to populate invalid-tag params");
	GENERATED_TEST_ASSERT(jsval_subtle_crypto_encrypt(&region, subtle_value,
			invalid_tag_params, raw_import_key, raw_input_buffer,
			&invalid_tag_promise) == 0, SUITE, CASE_NAME,
			"failed to call AES encrypt with invalid tagLength");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, invalid_tag_promise,
			&result) == 0, SUITE, CASE_NAME,
			"failed to read invalid-tag rejection");
	GENERATED_TEST_ASSERT(expect_dom_exception_name(&region, result,
			"OperationError", "encrypt invalid tagLength")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected AES invalid-tag rejection");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_generate_key(&region, subtle_value,
			aes_algorithm, 1, usages, &bad_decrypt_key_promise) == 0, SUITE,
			CASE_NAME, "failed to call bad-decrypt AES generateKey");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain bad-decrypt AES generateKey");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, bad_decrypt_key_promise,
			&bad_decrypt_key) == 0 && bad_decrypt_key.kind == JSVAL_KIND_CRYPTO_KEY,
			SUITE, CASE_NAME, "expected bad-decrypt AES key");
	GENERATED_TEST_ASSERT(jsval_subtle_crypto_encrypt(&region, subtle_value,
			params, raw_import_key, raw_input_buffer, &encrypt_promise) == 0, SUITE,
			CASE_NAME, "failed to call AES encrypt before bad decrypt");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain AES encrypt before bad decrypt");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, encrypt_promise, &result)
			== 0 && result.kind == JSVAL_KIND_ARRAY_BUFFER, SUITE, CASE_NAME,
			"expected ciphertext before bad decrypt");
	GENERATED_TEST_ASSERT(jsval_subtle_crypto_decrypt(&region, subtle_value,
			params, bad_decrypt_key, result, &bad_decrypt_promise) == 0, SUITE,
			CASE_NAME, "failed to call AES decrypt with wrong key");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain AES wrong-key decrypt");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, bad_decrypt_promise,
			&result) == 0, SUITE, CASE_NAME,
			"failed to read AES wrong-key decrypt rejection");
	GENERATED_TEST_ASSERT(expect_dom_exception_name(&region, result,
			"OperationError", "decrypt wrong key") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected AES wrong-key rejection");

	/*
	 * AES-GCM as a wrapping cipher: a key with only encrypt/decrypt
	 * usages must reject wrapKey with InvalidAccessError before any
	 * cipher work runs.
	 */
	GENERATED_TEST_ASSERT(jsval_subtle_crypto_wrap_key(&region, subtle_value,
			raw_format, raw_import_key, raw_import_key, params,
			&wrap_no_usage_promise) == 0, SUITE, CASE_NAME,
			"failed to call AES-GCM wrapKey on non-wrap key");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, wrap_no_usage_promise,
			&result) == 0, SUITE, CASE_NAME,
			"failed to read AES-GCM wrapKey rejection");
	GENERATED_TEST_ASSERT(expect_dom_exception_name(&region, result,
			"InvalidAccessError", "wrapKey missing wrapKey usage")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected AES-GCM wrapKey rejection");
	return generated_test_pass(SUITE, CASE_NAME);
}
