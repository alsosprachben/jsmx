#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "wintertc"
#define CASE_NAME "jsmx/crypto-subtle-hmac-errors"

static int
make_string(jsval_region_t *region, const char *text, jsval_t *value_ptr)
{
	return jsval_string_new_utf8(region, (const uint8_t *)text, strlen(text),
			value_ptr);
}

static int
make_usage_array(jsval_region_t *region, int sign, jsval_t *value_ptr)
{
	jsval_t array;
	jsval_t value;

	if (jsval_array_new(region, sign ? 1u : 0u, &array) < 0) {
		return -1;
	}
	if (sign) {
		if (make_string(region, "sign", &value) < 0
				|| jsval_array_push(region, array, value) < 0) {
			return -1;
		}
	}
	*value_ptr = array;
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
			&& jsval_object_set_utf8(region, object, (const uint8_t *)"length",
				6, jsval_number(length_value)) < 0) {
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
	uint8_t storage[131072];
	jsval_region_t region;
	jsval_t crypto_value;
	jsval_t subtle_value;
	jsval_t sign_usages;
	jsval_t empty_usages;
	jsval_t hmac_algorithm;
	jsval_t hmac_algorithm_9;
	jsval_t raw_input;
	jsval_t pkcs8_format;
	jsval_t raw_format;
	jsval_t jwk_format;
	jsval_t empty_usages_promise;
	jsval_t unsupported_format_promise;
	jsval_t malformed_jwk_promise;
	jsval_t non_extractable_key_promise;
	jsval_t non_extractable_key;
	jsval_t non_extractable_export_promise;
	jsval_t jwk_object;
	jsval_t result;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_crypto_new(&region, &crypto_value) == 0,
			SUITE, CASE_NAME, "failed to allocate crypto object");
	GENERATED_TEST_ASSERT(jsval_crypto_subtle(&region, crypto_value,
			&subtle_value) == 0, SUITE, CASE_NAME,
			"failed to read crypto.subtle");
	GENERATED_TEST_ASSERT(make_usage_array(&region, 1, &sign_usages) == 0,
			SUITE, CASE_NAME, "failed to build sign usages");
	GENERATED_TEST_ASSERT(jsval_array_new(&region, 0, &empty_usages) == 0,
			SUITE, CASE_NAME, "failed to build empty usages");
	GENERATED_TEST_ASSERT(make_hmac_algorithm(&region, 0, 0.0,
			&hmac_algorithm) == 0, SUITE, CASE_NAME,
			"failed to build HMAC algorithm");
	GENERATED_TEST_ASSERT(make_hmac_algorithm(&region, 1, 9.0,
			&hmac_algorithm_9) == 0, SUITE, CASE_NAME,
			"failed to build HMAC length algorithm");
	GENERATED_TEST_ASSERT(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8,
			32, &raw_input) == 0, SUITE, CASE_NAME,
			"failed to allocate raw input");
	GENERATED_TEST_ASSERT(make_string(&region, "pkcs8", &pkcs8_format) == 0,
			SUITE, CASE_NAME, "failed to build pkcs8 format string");
	GENERATED_TEST_ASSERT(make_string(&region, "raw", &raw_format) == 0, SUITE,
			CASE_NAME, "failed to build raw format string");
	GENERATED_TEST_ASSERT(make_string(&region, "jwk", &jwk_format) == 0, SUITE,
			CASE_NAME, "failed to build jwk format string");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_generate_key(&region, subtle_value,
			hmac_algorithm, 1, empty_usages, &empty_usages_promise) == 0, SUITE,
			CASE_NAME, "failed to call generateKey with empty usages");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, empty_usages_promise,
			&result) == 0, SUITE, CASE_NAME,
			"failed to read empty-usages rejection");
	GENERATED_TEST_ASSERT(expect_dom_exception_name(&region, result,
			"SyntaxError", "generateKey empty usages") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected generateKey empty-usages rejection");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_import_key(&region, subtle_value,
			pkcs8_format, raw_input, hmac_algorithm, 1, sign_usages,
			&unsupported_format_promise) == 0, SUITE, CASE_NAME,
			"failed to call importKey with unsupported format");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, unsupported_format_promise,
			&result) == 0, SUITE, CASE_NAME,
			"failed to read unsupported-format rejection");
	GENERATED_TEST_ASSERT(expect_dom_exception_name(&region, result,
			"NotSupportedError", "importKey unsupported format")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected importKey unsupported-format rejection");

	GENERATED_TEST_ASSERT(jsval_object_new(&region, 5, &jwk_object) == 0,
			SUITE, CASE_NAME, "failed to allocate malformed JWK");
	GENERATED_TEST_ASSERT(make_string(&region, "RSA", &result) == 0
			&& jsval_object_set_utf8(&region, jwk_object,
				(const uint8_t *)"kty", 3, result) == 0
			&& make_string(&region, "__8", &result) == 0
			&& jsval_object_set_utf8(&region, jwk_object,
				(const uint8_t *)"k", 1, result) == 0
			&& make_string(&region, "HS256", &result) == 0
			&& jsval_object_set_utf8(&region, jwk_object,
				(const uint8_t *)"alg", 3, result) == 0
			&& jsval_object_set_utf8(&region, jwk_object,
				(const uint8_t *)"ext", 3, jsval_bool(1)) == 0
			&& jsval_object_set_utf8(&region, jwk_object,
				(const uint8_t *)"key_ops", 7, sign_usages) == 0, SUITE,
			CASE_NAME, "failed to populate malformed JWK");
	GENERATED_TEST_ASSERT(jsval_subtle_crypto_import_key(&region, subtle_value,
			jwk_format, jwk_object, hmac_algorithm_9, 1, sign_usages,
			&malformed_jwk_promise) == 0, SUITE, CASE_NAME,
			"failed to call importKey with malformed JWK");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, malformed_jwk_promise,
			&result) == 0, SUITE, CASE_NAME,
			"failed to read malformed-JWK rejection");
	GENERATED_TEST_ASSERT(expect_dom_exception_name(&region, result,
			"DataError", "importKey malformed JWK") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected malformed-JWK rejection");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_generate_key(&region, subtle_value,
			hmac_algorithm, 0, sign_usages, &non_extractable_key_promise) == 0,
			SUITE, CASE_NAME, "failed to call generateKey nonextractable");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain generateKey nonextractable");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, non_extractable_key_promise,
			&non_extractable_key) == 0, SUITE, CASE_NAME,
			"failed to read nonextractable key");
	GENERATED_TEST_ASSERT(jsval_subtle_crypto_export_key(&region, subtle_value,
			raw_format, non_extractable_key, &non_extractable_export_promise) == 0,
			SUITE, CASE_NAME, "failed to call exportKey on nonextractable key");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region,
			non_extractable_export_promise, &result) == 0, SUITE, CASE_NAME,
			"failed to read nonextractable export rejection");
	GENERATED_TEST_ASSERT(expect_dom_exception_name(&region, result,
			"InvalidAccessError", "exportKey nonextractable")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected nonextractable export rejection");
	return generated_test_pass(SUITE, CASE_NAME);
}
