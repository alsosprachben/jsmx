#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "wintertc"
#define CASE_NAME "jsmx/crypto-subtle-aes-cbc-errors"

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
	jsval_t bad_iv;
	jsval_t bad_iv_buffer;
	jsval_t bad_iv_params;
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
	jsval_t bad_iv_encrypt_promise;
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
			CASE_NAME, "failed to build AES-CBC usages");
	GENERATED_TEST_ASSERT(make_aes_algorithm(&region, 1, 128.0, &aes_algorithm)
			== 0, SUITE, CASE_NAME, "failed to build AES-CBC generate algorithm");
	GENERATED_TEST_ASSERT(make_aes_algorithm(&region, 0, 0.0,
			&aes_import_algorithm) == 0, SUITE, CASE_NAME,
			"failed to build AES-CBC import algorithm");
	GENERATED_TEST_ASSERT(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8,
			16, &raw_input) == 0, SUITE, CASE_NAME,
			"failed to allocate AES-CBC raw input");
	GENERATED_TEST_ASSERT(jsval_typed_array_buffer(&region, raw_input,
			&raw_input_buffer) == 0, SUITE, CASE_NAME,
			"failed to read AES-CBC raw input buffer");
	GENERATED_TEST_ASSERT(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8,
			16, &iv) == 0, SUITE, CASE_NAME, "failed to allocate AES-CBC IV");
	GENERATED_TEST_ASSERT(jsval_typed_array_buffer(&region, iv, &iv_buffer) == 0,
			SUITE, CASE_NAME, "failed to read AES-CBC IV buffer");
	GENERATED_TEST_ASSERT(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8,
			12, &bad_iv) == 0, SUITE, CASE_NAME,
			"failed to allocate bad AES-CBC IV");
	GENERATED_TEST_ASSERT(jsval_typed_array_buffer(&region, bad_iv,
			&bad_iv_buffer) == 0, SUITE, CASE_NAME,
			"failed to read bad AES-CBC IV buffer");
	GENERATED_TEST_ASSERT(make_string(&region, "raw", &raw_format) == 0, SUITE,
			CASE_NAME, "failed to build raw format");
	GENERATED_TEST_ASSERT(make_string(&region, "jwk", &jwk_format) == 0, SUITE,
			CASE_NAME, "failed to build jwk format");
	GENERATED_TEST_ASSERT(make_string(&region, "pkcs8", &pkcs8_format) == 0,
			SUITE, CASE_NAME, "failed to build pkcs8 format");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_generate_key(&region, subtle_value,
			aes_algorithm, 0, usages, &non_extractable_promise) == 0, SUITE,
			CASE_NAME, "failed to call nonextractable AES-CBC generateKey");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain nonextractable AES-CBC generateKey");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, non_extractable_promise,
			&non_extractable_key) == 0, SUITE, CASE_NAME,
			"failed to read nonextractable AES-CBC key");
	GENERATED_TEST_ASSERT(jsval_subtle_crypto_export_key(&region, subtle_value,
			raw_format, non_extractable_key, &non_extractable_export_promise) == 0,
			SUITE, CASE_NAME,
			"failed to call exportKey on nonextractable AES-CBC key");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region,
			non_extractable_export_promise, &result) == 0, SUITE, CASE_NAME,
			"failed to read nonextractable AES-CBC export rejection");
	GENERATED_TEST_ASSERT(expect_dom_exception_name(&region, result,
			"InvalidAccessError", "exportKey nonextractable")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected nonextractable AES-CBC export rejection");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_import_key(&region, subtle_value,
			pkcs8_format, raw_input_buffer, aes_import_algorithm, 1, usages,
			&unsupported_format_promise) == 0, SUITE, CASE_NAME,
			"failed to call AES-CBC importKey with unsupported format");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, unsupported_format_promise,
			&result) == 0, SUITE, CASE_NAME,
			"failed to read AES-CBC unsupported-format rejection");
	GENERATED_TEST_ASSERT(expect_dom_exception_name(&region, result,
			"NotSupportedError", "importKey unsupported format")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected AES-CBC unsupported-format rejection");

	GENERATED_TEST_ASSERT(jsval_object_new(&region, 5, &jwk_object) == 0, SUITE,
			CASE_NAME, "failed to allocate malformed AES-CBC JWK");
	GENERATED_TEST_ASSERT(make_string(&region, "RSA", &result) == 0
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
			"failed to populate malformed AES-CBC JWK");
	GENERATED_TEST_ASSERT(jsval_subtle_crypto_import_key(&region, subtle_value,
			jwk_format, jwk_object, aes_import_algorithm, 1, usages,
			&malformed_jwk_promise) == 0, SUITE, CASE_NAME,
			"failed to call AES-CBC importKey with malformed JWK");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, malformed_jwk_promise,
			&result) == 0, SUITE, CASE_NAME,
			"failed to read malformed AES-CBC JWK rejection");
	GENERATED_TEST_ASSERT(expect_dom_exception_name(&region, result, "DataError",
			"importKey malformed JWK") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected AES-CBC malformed-JWK rejection");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_import_key(&region, subtle_value,
			raw_format, raw_input_buffer, aes_import_algorithm, 1, usages,
			&raw_import_key_promise) == 0, SUITE, CASE_NAME,
			"failed to call AES-CBC importKey(raw)");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain AES-CBC importKey(raw)");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, raw_import_key_promise,
			&raw_import_key) == 0 && raw_import_key.kind == JSVAL_KIND_CRYPTO_KEY,
			SUITE, CASE_NAME, "expected AES-CBC raw import CryptoKey");

	GENERATED_TEST_ASSERT(jsval_object_new(&region, 2, &bad_iv_params) == 0,
			SUITE, CASE_NAME, "failed to allocate bad-iv params");
	GENERATED_TEST_ASSERT(make_string(&region, "AES-CBC", &result) == 0
			&& jsval_object_set_utf8(&region, bad_iv_params,
				(const uint8_t *)"name", 4, result) == 0
			&& jsval_object_set_utf8(&region, bad_iv_params,
				(const uint8_t *)"iv", 2, bad_iv_buffer) == 0, SUITE, CASE_NAME,
			"failed to populate bad-iv params");
	GENERATED_TEST_ASSERT(jsval_subtle_crypto_encrypt(&region, subtle_value,
			bad_iv_params, raw_import_key, raw_input_buffer,
			&bad_iv_encrypt_promise) == 0, SUITE, CASE_NAME,
			"failed to call AES-CBC encrypt with short iv");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, bad_iv_encrypt_promise,
			&result) == 0, SUITE, CASE_NAME,
			"failed to read bad-iv rejection");
	GENERATED_TEST_ASSERT(expect_dom_exception_name(&region, result,
			"OperationError", "encrypt short iv") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected AES-CBC short-iv rejection");
	return generated_test_pass(SUITE, CASE_NAME);
}
