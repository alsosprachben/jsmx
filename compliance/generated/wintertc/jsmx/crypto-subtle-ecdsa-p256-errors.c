#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "wintertc"
#define CASE_NAME "jsmx/crypto-subtle-ecdsa-p256-errors"

static int
make_string(jsval_region_t *region, const char *text, jsval_t *value_ptr)
{
	return jsval_string_new_utf8(region, (const uint8_t *)text, strlen(text),
			value_ptr);
}

static int
make_sign_verify_usages(jsval_region_t *region, int sign, int verify,
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
make_ecdsa_key_algorithm(jsval_region_t *region, const char *curve,
		jsval_t *value_ptr)
{
	jsval_t object;
	jsval_t value;

	if (jsval_object_new(region, 2, &object) < 0
			|| make_string(region, "ECDSA", &value) < 0
			|| jsval_object_set_utf8(region, object, (const uint8_t *)"name", 4,
				value) < 0
			|| make_string(region, curve, &value) < 0
			|| jsval_object_set_utf8(region, object,
				(const uint8_t *)"namedCurve", 10, value) < 0) {
		return -1;
	}
	*value_ptr = object;
	return 0;
}

static int
make_ecdsa_op_algorithm(jsval_region_t *region, const char *hash,
		jsval_t *value_ptr)
{
	jsval_t object;
	jsval_t value;

	if (jsval_object_new(region, 2, &object) < 0
			|| make_string(region, "ECDSA", &value) < 0
			|| jsval_object_set_utf8(region, object, (const uint8_t *)"name", 4,
				value) < 0
			|| make_string(region, hash, &value) < 0
			|| jsval_object_set_utf8(region, object, (const uint8_t *)"hash", 4,
				value) < 0) {
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
	if (jsval_dom_exception_name(region, value, &name) < 0
			|| jsval_string_copy_utf8(region, name, NULL, 0, &len) < 0
			|| len != expected_len
			|| jsval_string_copy_utf8(region, name, buf, len, NULL) < 0
			|| memcmp(buf, expected, len) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: expected DOMException name %s", label, expected);
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
	jsval_t verify_only_usages;
	jsval_t key_algorithm;
	jsval_t op_algorithm;
	jsval_t bad_curve_algorithm;
	jsval_t jwk_format;
	jsval_t generate_promise;
	jsval_t pair;
	jsval_t public_key;
	jsval_t private_key;
	jsval_t data_typed;
	jsval_t data_buffer;
	jsval_t sign_promise;
	jsval_t signature;
	jsval_t bad_sign_promise;
	jsval_t bad_verify_promise;
	jsval_t bad_curve_promise;
	jsval_t bad_import_promise;
	jsval_t bad_jwk;
	jsval_t value;
	jsval_t result;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_crypto_new(&region, &crypto_value) == 0,
			SUITE, CASE_NAME, "failed to allocate crypto object");
	GENERATED_TEST_ASSERT(jsval_crypto_subtle(&region, crypto_value,
			&subtle_value) == 0, SUITE, CASE_NAME,
			"failed to read crypto.subtle");
	GENERATED_TEST_ASSERT(make_sign_verify_usages(&region, 1, 1, &usages) == 0,
			SUITE, CASE_NAME, "failed to build sign/verify usages");
	GENERATED_TEST_ASSERT(make_sign_verify_usages(&region, 0, 1,
			&verify_only_usages) == 0, SUITE, CASE_NAME,
			"failed to build verify-only usages");
	GENERATED_TEST_ASSERT(make_ecdsa_key_algorithm(&region, "P-256",
			&key_algorithm) == 0, SUITE, CASE_NAME,
			"failed to build ECDSA key algorithm");
	GENERATED_TEST_ASSERT(make_ecdsa_op_algorithm(&region, "SHA-256",
			&op_algorithm) == 0, SUITE, CASE_NAME,
			"failed to build ECDSA op algorithm");
	GENERATED_TEST_ASSERT(make_string(&region, "jwk", &jwk_format) == 0,
			SUITE, CASE_NAME, "failed to build jwk format");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_generate_key(&region,
			subtle_value, key_algorithm, 1, usages, &generate_promise) == 0,
			SUITE, CASE_NAME, "failed to call ECDSA generateKey");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain ECDSA generateKey");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, generate_promise,
			&pair) == 0 && pair.kind == JSVAL_KIND_OBJECT, SUITE, CASE_NAME,
			"expected ECDSA pair object");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, pair,
			(const uint8_t *)"publicKey", 9, &public_key) == 0
			&& jsval_object_get_utf8(&region, pair,
				(const uint8_t *)"privateKey", 10, &private_key) == 0, SUITE,
			CASE_NAME, "expected ECDSA pair accessors");

	GENERATED_TEST_ASSERT(jsval_typed_array_new(&region,
			JSVAL_TYPED_ARRAY_UINT8, 16, &data_typed) == 0, SUITE, CASE_NAME,
			"failed to allocate ECDSA data");
	GENERATED_TEST_ASSERT(jsval_typed_array_buffer(&region, data_typed,
			&data_buffer) == 0, SUITE, CASE_NAME,
			"failed to read ECDSA data buffer");
	GENERATED_TEST_ASSERT(jsval_subtle_crypto_sign(&region, subtle_value,
			op_algorithm, private_key, data_buffer, &sign_promise) == 0, SUITE,
			CASE_NAME, "failed to call ECDSA sign");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain ECDSA sign");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, sign_promise,
			&signature) == 0
			&& signature.kind == JSVAL_KIND_ARRAY_BUFFER, SUITE, CASE_NAME,
			"expected ECDSA sign result");

	/* Unsupported curve -> NotSupportedError. */
	GENERATED_TEST_ASSERT(make_ecdsa_key_algorithm(&region, "P-521",
			&bad_curve_algorithm) == 0, SUITE, CASE_NAME,
			"failed to build bad curve algorithm");
	GENERATED_TEST_ASSERT(jsval_subtle_crypto_generate_key(&region,
			subtle_value, bad_curve_algorithm, 1, usages,
			&bad_curve_promise) == 0, SUITE, CASE_NAME,
			"failed to call ECDSA generateKey with bad curve");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, bad_curve_promise,
			&result) == 0, SUITE, CASE_NAME,
			"failed to read bad curve result");
	GENERATED_TEST_ASSERT(expect_dom_exception_name(&region, result,
			"NotSupportedError", "generateKey P-521")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected ECDSA bad-curve rejection");

	/* Sign with a public key -> InvalidAccessError. */
	GENERATED_TEST_ASSERT(jsval_subtle_crypto_sign(&region, subtle_value,
			op_algorithm, public_key, data_buffer, &bad_sign_promise) == 0,
			SUITE, CASE_NAME, "failed to call ECDSA sign with public key");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, bad_sign_promise,
			&result) == 0, SUITE, CASE_NAME,
			"failed to read ECDSA bad-sign result");
	GENERATED_TEST_ASSERT(expect_dom_exception_name(&region, result,
			"InvalidAccessError", "sign with public key")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected ECDSA bad-sign rejection");

	/* Verify with a private key -> InvalidAccessError. */
	GENERATED_TEST_ASSERT(jsval_subtle_crypto_verify(&region, subtle_value,
			op_algorithm, private_key, signature, data_buffer,
			&bad_verify_promise) == 0, SUITE, CASE_NAME,
			"failed to call ECDSA verify with private key");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, bad_verify_promise,
			&result) == 0, SUITE, CASE_NAME,
			"failed to read ECDSA bad-verify result");
	GENERATED_TEST_ASSERT(expect_dom_exception_name(&region, result,
			"InvalidAccessError", "verify with private key")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected ECDSA bad-verify rejection");

	/* Malformed JWK (wrong crv) -> DataError. */
	GENERATED_TEST_ASSERT(jsval_object_new(&region, 4, &bad_jwk) == 0, SUITE,
			CASE_NAME, "failed to build malformed JWK");
	GENERATED_TEST_ASSERT(make_string(&region, "EC", &value) == 0
			&& jsval_object_set_utf8(&region, bad_jwk, (const uint8_t *)"kty",
				3, value) == 0
			&& make_string(&region, "P-384", &value) == 0
			&& jsval_object_set_utf8(&region, bad_jwk, (const uint8_t *)"crv",
				3, value) == 0
			&& make_string(&region, "AAAA", &value) == 0
			&& jsval_object_set_utf8(&region, bad_jwk, (const uint8_t *)"x",
				1, value) == 0
			&& jsval_object_set_utf8(&region, bad_jwk, (const uint8_t *)"y",
				1, value) == 0, SUITE, CASE_NAME,
			"failed to populate malformed JWK");
	GENERATED_TEST_ASSERT(jsval_subtle_crypto_import_key(&region, subtle_value,
			jwk_format, bad_jwk, key_algorithm, 1, verify_only_usages,
			&bad_import_promise) == 0, SUITE, CASE_NAME,
			"failed to call ECDSA importKey(bad JWK)");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, bad_import_promise,
			&result) == 0, SUITE, CASE_NAME,
			"failed to read ECDSA bad JWK result");
	GENERATED_TEST_ASSERT(expect_dom_exception_name(&region, result,
			"DataError", "importKey bad JWK crv")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected ECDSA bad-JWK rejection");

	return generated_test_pass(SUITE, CASE_NAME);
}
