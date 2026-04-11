#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "wintertc"
#define CASE_NAME "jsmx/crypto-subtle-hmac-parity"

static int
make_string(jsval_region_t *region, const char *text, jsval_t *value_ptr)
{
	return jsval_string_new_utf8(region, (const uint8_t *)text, strlen(text),
			value_ptr);
}

static int
make_usage_array(jsval_region_t *region, int sign, int verify,
		jsval_t *value_ptr)
{
	jsval_t array;
	jsval_t value;
	size_t cap = (sign ? 1u : 0u) + (verify ? 1u : 0u);

	if (jsval_array_new(region, cap, &array) < 0) {
		return -1;
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
	*value_ptr = array;
	return 0;
}

static int
make_hmac_algorithm(jsval_region_t *region, const char *hash_name,
		int have_length, double length_value, jsval_t *value_ptr)
{
	jsval_t object;
	jsval_t hash_object;
	jsval_t value;

	if (jsval_object_new(region, have_length ? 3 : 2, &object) < 0
			|| jsval_object_new(region, 1, &hash_object) < 0
			|| make_string(region, "HMAC", &value) < 0
			|| jsval_object_set_utf8(region, object, (const uint8_t *)"name", 4,
				value) < 0
			|| make_string(region, hash_name, &value) < 0
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
	static const uint8_t zero_key_32[32] = { 0 };
	static const uint8_t masked_key_9_bits[] = { 0xff, 0x80 };
	static const char *expected_sign_verify_ops[] = { "sign", "verify" };
	static const char *expected_sign_ops[] = { "sign" };
	uint8_t storage[262144];
	jsval_region_t region;
	jsval_t crypto_value;
	jsval_t subtle_value;
	jsval_t input;
	jsval_t input_buffer;
	jsval_t bad_input;
	jsval_t bad_input_buffer;
	jsval_t usages;
	jsval_t sign_usages;
	jsval_t hmac_algorithm;
	jsval_t hmac_algorithm_9;
	jsval_t hmac_name;
	jsval_t raw_format;
	jsval_t jwk_format;
	jsval_t generated_key_promise;
	jsval_t generated_key;
	jsval_t exported_raw_promise;
	jsval_t exported_jwk_promise;
	jsval_t sign_promise;
	jsval_t verify_promise;
	jsval_t verify_false_promise;
	jsval_t raw_import_key_promise;
	jsval_t raw_import_key;
	jsval_t raw_key_input;
	jsval_t jwk_object;
	jsval_t jwk_import_key_promise;
	jsval_t jwk_import_key;
	jsval_t jwk_export_raw_promise;
	jsval_t jwk_export_jwk_promise;
	jsval_t result;
	jsval_t prop;
	jsval_promise_state_t state;
	jsmethod_error_t error;
	uint32_t usages_mask = 0;
	size_t byte_length = 0;
	int extractable = 0;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_crypto_new(&region, &crypto_value) == 0,
			SUITE, CASE_NAME, "failed to allocate crypto object");
	GENERATED_TEST_ASSERT(jsval_crypto_subtle(&region, crypto_value,
			&subtle_value) == 0, SUITE, CASE_NAME,
			"failed to read crypto.subtle");
	GENERATED_TEST_ASSERT(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8,
			16, &input) == 0, SUITE, CASE_NAME,
			"failed to allocate sign input");
	GENERATED_TEST_ASSERT(jsval_typed_array_buffer(&region, input, &input_buffer)
			== 0, SUITE, CASE_NAME,
			"failed to read sign input buffer");
	GENERATED_TEST_ASSERT(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8,
			15, &bad_input) == 0, SUITE, CASE_NAME,
			"failed to allocate bad verify input");
	GENERATED_TEST_ASSERT(jsval_typed_array_buffer(&region, bad_input,
			&bad_input_buffer) == 0, SUITE, CASE_NAME,
			"failed to read bad verify input buffer");
	GENERATED_TEST_ASSERT(make_usage_array(&region, 1, 1, &usages) == 0,
			SUITE, CASE_NAME, "failed to build sign/verify usages");
	GENERATED_TEST_ASSERT(make_usage_array(&region, 1, 0, &sign_usages) == 0,
			SUITE, CASE_NAME, "failed to build sign-only usages");
	GENERATED_TEST_ASSERT(make_hmac_algorithm(&region, "SHA-256", 0, 0.0,
			&hmac_algorithm) == 0, SUITE, CASE_NAME,
			"failed to build HMAC algorithm");
	GENERATED_TEST_ASSERT(make_hmac_algorithm(&region, "SHA-256", 1, 9.0,
			&hmac_algorithm_9) == 0, SUITE, CASE_NAME,
			"failed to build HMAC length algorithm");
	GENERATED_TEST_ASSERT(make_string(&region, "HMAC", &hmac_name) == 0,
			SUITE, CASE_NAME, "failed to build HMAC name string");
	GENERATED_TEST_ASSERT(make_string(&region, "raw", &raw_format) == 0,
			SUITE, CASE_NAME, "failed to build raw format string");
	GENERATED_TEST_ASSERT(make_string(&region, "jwk", &jwk_format) == 0,
			SUITE, CASE_NAME, "failed to build jwk format string");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_generate_key(&region, subtle_value,
			hmac_algorithm, 1, usages, &generated_key_promise) == 0, SUITE,
			CASE_NAME, "failed to call subtle.generateKey");
	GENERATED_TEST_ASSERT(jsval_promise_state(&region, generated_key_promise,
			&state) == 0 && state == JSVAL_PROMISE_STATE_PENDING, SUITE,
			CASE_NAME, "expected generateKey promise to stay pending");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0,
			SUITE, CASE_NAME, "failed to drain generateKey microtasks");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, generated_key_promise,
			&generated_key) == 0 && generated_key.kind == JSVAL_KIND_CRYPTO_KEY,
			SUITE, CASE_NAME, "expected generateKey to resolve CryptoKey");
	GENERATED_TEST_ASSERT(jsval_crypto_key_algorithm(&region, generated_key,
			&result) == 0 && result.kind == JSVAL_KIND_OBJECT, SUITE, CASE_NAME,
			"expected generated key algorithm object");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"name", 4, &prop) == 0, SUITE, CASE_NAME,
			"failed to read generated key algorithm name");
	GENERATED_TEST_ASSERT(expect_string(&region, prop, "HMAC", "algorithm.name")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected generated key algorithm name");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"hash", 4, &prop) == 0, SUITE, CASE_NAME,
			"failed to read generated key hash");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, prop,
			(const uint8_t *)"name", 4, &result) == 0, SUITE, CASE_NAME,
			"failed to read generated key hash.name");
	GENERATED_TEST_ASSERT(expect_string(&region, result, "SHA-256",
			"algorithm.hash.name") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected generated key hash name");
	GENERATED_TEST_ASSERT(jsval_crypto_key_usages(&region, generated_key,
			&usages_mask) == 0, SUITE, CASE_NAME,
			"failed to read generated key usages");
	GENERATED_TEST_ASSERT(usages_mask == (JSVAL_CRYPTO_KEY_USAGE_SIGN
			| JSVAL_CRYPTO_KEY_USAGE_VERIFY), SUITE, CASE_NAME,
			"unexpected generated key usages");
	GENERATED_TEST_ASSERT(jsval_crypto_key_extractable(&region, generated_key,
			&extractable) == 0 && extractable == 1, SUITE, CASE_NAME,
			"expected generated key to be extractable");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_export_key(&region, subtle_value,
			raw_format, generated_key, &exported_raw_promise) == 0, SUITE,
			CASE_NAME, "failed to call exportKey(raw)");
	GENERATED_TEST_ASSERT(jsval_promise_state(&region, exported_raw_promise,
			&state) == 0 && state == JSVAL_PROMISE_STATE_PENDING, SUITE,
			CASE_NAME, "expected exportKey(raw) promise to stay pending");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain exportKey(raw)");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, exported_raw_promise,
			&result) == 0, SUITE, CASE_NAME,
			"failed to read exportKey(raw) result");
	GENERATED_TEST_ASSERT(result.kind == JSVAL_KIND_ARRAY_BUFFER, SUITE,
			CASE_NAME, "expected exportKey(raw) ArrayBuffer");
	GENERATED_TEST_ASSERT(jsval_array_buffer_byte_length(&region, result,
			&byte_length) == 0, SUITE, CASE_NAME,
			"failed to read exportKey(raw) byte length");
	GENERATED_TEST_ASSERT(byte_length == 64, SUITE, CASE_NAME,
			"expected 64-byte exportKey(raw) key");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_sign(&region, subtle_value,
			hmac_name, generated_key, input_buffer, &sign_promise) == 0, SUITE,
			CASE_NAME, "failed to call subtle.sign");
	GENERATED_TEST_ASSERT(jsval_promise_state(&region, sign_promise, &state) == 0
			&& state == JSVAL_PROMISE_STATE_PENDING, SUITE, CASE_NAME,
			"expected subtle.sign promise to stay pending");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain subtle.sign");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, sign_promise, &result)
			== 0, SUITE, CASE_NAME, "failed to read subtle.sign result");
	GENERATED_TEST_ASSERT(result.kind == JSVAL_KIND_ARRAY_BUFFER, SUITE,
			CASE_NAME, "expected subtle.sign ArrayBuffer");
	GENERATED_TEST_ASSERT(jsval_array_buffer_byte_length(&region, result,
			&byte_length) == 0, SUITE, CASE_NAME,
			"failed to read subtle.sign byte length");
	GENERATED_TEST_ASSERT(byte_length == 32, SUITE, CASE_NAME,
			"expected 32-byte HMAC signature");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_verify(&region, subtle_value,
			hmac_name, generated_key, result, input_buffer, &verify_promise) == 0,
			SUITE, CASE_NAME, "failed to call subtle.verify(true)");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain subtle.verify(true)");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, verify_promise, &prop)
			== 0 && prop.kind == JSVAL_KIND_BOOL && prop.as.boolean == 1, SUITE,
			CASE_NAME, "expected subtle.verify(true) to fulfill true");
	GENERATED_TEST_ASSERT(jsval_subtle_crypto_verify(&region, subtle_value,
			hmac_name, generated_key, result, bad_input_buffer,
			&verify_false_promise) == 0, SUITE, CASE_NAME,
			"failed to call subtle.verify(false)");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain subtle.verify(false)");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, verify_false_promise,
			&prop) == 0 && prop.kind == JSVAL_KIND_BOOL && prop.as.boolean == 0,
			SUITE, CASE_NAME, "expected subtle.verify(false) to fulfill false");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_export_key(&region, subtle_value,
			jwk_format, generated_key, &exported_jwk_promise) == 0, SUITE,
			CASE_NAME, "failed to call exportKey(jwk)");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain exportKey(jwk)");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, exported_jwk_promise,
			&result) == 0, SUITE, CASE_NAME,
			"failed to read exportKey(jwk) result");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"kty", 3, &prop) == 0, SUITE, CASE_NAME,
			"failed to read exportKey(jwk).kty");
	GENERATED_TEST_ASSERT(expect_string(&region, prop, "oct", "jwk.kty")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected exportKey(jwk).kty");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"alg", 3, &prop) == 0, SUITE, CASE_NAME,
			"failed to read exportKey(jwk).alg");
	GENERATED_TEST_ASSERT(expect_string(&region, prop, "HS256", "jwk.alg")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected exportKey(jwk).alg");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"key_ops", 7, &prop) == 0, SUITE, CASE_NAME,
			"failed to read exportKey(jwk).key_ops");
	GENERATED_TEST_ASSERT(expect_string_array(&region, prop,
			expected_sign_verify_ops, 2, "jwk.key_ops")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected exportKey(jwk).key_ops");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"ext", 3, &prop) == 0, SUITE, CASE_NAME,
			"failed to read exportKey(jwk).ext");
	GENERATED_TEST_ASSERT(prop.kind == JSVAL_KIND_BOOL && prop.as.boolean == 1,
			SUITE, CASE_NAME, "expected exportKey(jwk).ext to be true");

	GENERATED_TEST_ASSERT(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8,
			32, &raw_key_input) == 0, SUITE, CASE_NAME,
			"failed to allocate raw import key input");
	GENERATED_TEST_ASSERT(jsval_subtle_crypto_import_key(&region, subtle_value,
			raw_format, raw_key_input, hmac_algorithm, 1, sign_usages,
			&raw_import_key_promise) == 0, SUITE, CASE_NAME,
			"failed to call importKey(raw)");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain importKey(raw)");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, raw_import_key_promise,
			&raw_import_key) == 0, SUITE, CASE_NAME,
			"failed to read importKey(raw) result");
	GENERATED_TEST_ASSERT(jsval_subtle_crypto_export_key(&region, subtle_value,
			raw_format, raw_import_key, &exported_raw_promise) == 0, SUITE,
			CASE_NAME, "failed to export imported raw key");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain export imported raw key");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, exported_raw_promise,
			&result) == 0, SUITE, CASE_NAME,
			"failed to read imported raw export result");
	GENERATED_TEST_ASSERT(expect_array_buffer_bytes(&region, result, zero_key_32,
			sizeof(zero_key_32), "import/export raw") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected import/export raw bytes");

	GENERATED_TEST_ASSERT(jsval_object_new(&region, 5, &jwk_object) == 0,
			SUITE, CASE_NAME, "failed to allocate JWK object");
	GENERATED_TEST_ASSERT(make_string(&region, "oct", &prop) == 0
			&& jsval_object_set_utf8(&region, jwk_object,
				(const uint8_t *)"kty", 3, prop) == 0, SUITE, CASE_NAME,
			"failed to set JWK kty");
	GENERATED_TEST_ASSERT(make_string(&region, "__8", &prop) == 0
			&& jsval_object_set_utf8(&region, jwk_object,
				(const uint8_t *)"k", 1, prop) == 0, SUITE, CASE_NAME,
			"failed to set JWK k");
	GENERATED_TEST_ASSERT(make_string(&region, "HS256", &prop) == 0
			&& jsval_object_set_utf8(&region, jwk_object,
				(const uint8_t *)"alg", 3, prop) == 0, SUITE, CASE_NAME,
			"failed to set JWK alg");
	GENERATED_TEST_ASSERT(jsval_object_set_utf8(&region, jwk_object,
			(const uint8_t *)"ext", 3, jsval_bool(1)) == 0, SUITE, CASE_NAME,
			"failed to set JWK ext");
	GENERATED_TEST_ASSERT(jsval_object_set_utf8(&region, jwk_object,
			(const uint8_t *)"key_ops", 7, sign_usages) == 0, SUITE, CASE_NAME,
			"failed to set JWK key_ops");
	GENERATED_TEST_ASSERT(jsval_subtle_crypto_import_key(&region, subtle_value,
			jwk_format, jwk_object, hmac_algorithm_9, 1, sign_usages,
			&jwk_import_key_promise) == 0, SUITE, CASE_NAME,
			"failed to call importKey(jwk)");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain importKey(jwk)");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, jwk_import_key_promise,
			&jwk_import_key) == 0, SUITE, CASE_NAME,
			"failed to read importKey(jwk) result");
	GENERATED_TEST_ASSERT(jsval_subtle_crypto_export_key(&region, subtle_value,
			raw_format, jwk_import_key, &jwk_export_raw_promise) == 0, SUITE,
			CASE_NAME, "failed to export imported JWK as raw");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain export imported JWK as raw");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, jwk_export_raw_promise,
			&result) == 0, SUITE, CASE_NAME,
			"failed to read imported JWK raw export result");
	GENERATED_TEST_ASSERT(expect_array_buffer_bytes(&region, result,
			masked_key_9_bits, sizeof(masked_key_9_bits),
			"import/export jwk raw") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected imported JWK raw bytes");
	GENERATED_TEST_ASSERT(jsval_subtle_crypto_export_key(&region, subtle_value,
			jwk_format, jwk_import_key, &jwk_export_jwk_promise) == 0, SUITE,
			CASE_NAME, "failed to export imported JWK as jwk");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain export imported JWK as jwk");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, jwk_export_jwk_promise,
			&result) == 0, SUITE, CASE_NAME,
			"failed to read imported JWK jwk export result");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"k", 1, &prop) == 0, SUITE, CASE_NAME,
			"failed to read imported JWK export k");
	GENERATED_TEST_ASSERT(expect_string(&region, prop, "_4A", "jwk export k")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected imported JWK export k");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"key_ops", 7, &prop) == 0, SUITE, CASE_NAME,
			"failed to read imported JWK export key_ops");
	GENERATED_TEST_ASSERT(expect_string_array(&region, prop, expected_sign_ops,
			1, "jwk export key_ops") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected imported JWK export key_ops");
	return generated_test_pass(SUITE, CASE_NAME);
}
