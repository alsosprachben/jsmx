#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "wintertc"
#define CASE_NAME "jsmx/crypto-subtle-rsassa-pkcs1-v1-5-parity"

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
make_rsa_key_algorithm(jsval_region_t *region, double modulus_length,
		const char *hash, jsval_t *value_ptr)
{
	jsval_t object;
	jsval_t hash_object;
	jsval_t value;

	if (jsval_object_new(region, 3, &object) < 0
			|| make_string(region, "RSASSA-PKCS1-v1_5", &value) < 0
			|| jsval_object_set_utf8(region, object, (const uint8_t *)"name", 4,
				value) < 0
			|| jsval_object_set_utf8(region, object,
				(const uint8_t *)"modulusLength", 13,
				jsval_number(modulus_length)) < 0
			|| jsval_object_new(region, 1, &hash_object) < 0
			|| make_string(region, hash, &value) < 0
			|| jsval_object_set_utf8(region, hash_object,
				(const uint8_t *)"name", 4, value) < 0
			|| jsval_object_set_utf8(region, object, (const uint8_t *)"hash",
				4, hash_object) < 0) {
		return -1;
	}
	*value_ptr = object;
	return 0;
}

static int
make_rsa_op_algorithm(jsval_region_t *region, jsval_t *value_ptr)
{
	jsval_t object;
	jsval_t value;

	if (jsval_object_new(region, 1, &object) < 0
			|| make_string(region, "RSASSA-PKCS1-v1_5", &value) < 0
			|| jsval_object_set_utf8(region, object, (const uint8_t *)"name", 4,
				value) < 0) {
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

	if (jsval_string_copy_utf8(region, value, NULL, 0, &len) < 0
			|| len != expected_len
			|| jsval_string_copy_utf8(region, value, buf, len, NULL) < 0
			|| memcmp(buf, expected, len) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: expected %s", label, expected);
	}
	return GENERATED_TEST_PASS;
}

int
main(void)
{
	uint8_t storage[524288];
	jsval_region_t region;
	jsval_t crypto_value;
	jsval_t subtle_value;
	jsval_t usages;
	jsval_t verify_only_usages;
	jsval_t key_algorithm;
	jsval_t op_algorithm;
	jsval_t jwk_format;
	jsval_t generate_promise;
	jsval_t pair;
	jsval_t public_key;
	jsval_t private_key;
	jsval_t data_typed;
	jsval_t data_buffer;
	jsval_t sign_promise;
	jsval_t signature;
	jsval_t verify_promise;
	jsval_t public_export_promise;
	jsval_t private_export_promise;
	jsval_t jwk_public;
	jsval_t jwk_private;
	jsval_t reimport_promise;
	jsval_t reimported_public;
	jsval_t reverify_promise;
	jsval_t result;
	jsval_t prop;
	jsmethod_error_t error;
	size_t byte_length = 0;

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
	GENERATED_TEST_ASSERT(make_rsa_key_algorithm(&region, 2048.0, "SHA-256",
			&key_algorithm) == 0, SUITE, CASE_NAME,
			"failed to build RSA key algorithm");
	GENERATED_TEST_ASSERT(make_rsa_op_algorithm(&region, &op_algorithm) == 0,
			SUITE, CASE_NAME, "failed to build RSA op algorithm");
	GENERATED_TEST_ASSERT(make_string(&region, "jwk", &jwk_format) == 0,
			SUITE, CASE_NAME, "failed to build jwk format");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_generate_key(&region,
			subtle_value, key_algorithm, 1, usages, &generate_promise) == 0,
			SUITE, CASE_NAME, "failed to call RSA generateKey");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain RSA generateKey");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, generate_promise,
			&pair) == 0 && pair.kind == JSVAL_KIND_OBJECT, SUITE, CASE_NAME,
			"expected RSA generateKey pair");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, pair,
			(const uint8_t *)"publicKey", 9, &public_key) == 0
			&& public_key.kind == JSVAL_KIND_CRYPTO_KEY, SUITE, CASE_NAME,
			"expected RSA public CryptoKey");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, pair,
			(const uint8_t *)"privateKey", 10, &private_key) == 0
			&& private_key.kind == JSVAL_KIND_CRYPTO_KEY, SUITE, CASE_NAME,
			"expected RSA private CryptoKey");
	GENERATED_TEST_ASSERT(jsval_crypto_key_algorithm(&region, public_key,
			&result) == 0 && result.kind == JSVAL_KIND_OBJECT, SUITE, CASE_NAME,
			"expected RSA public algorithm");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"name", 4, &prop) == 0
			&& expect_string(&region, prop, "RSASSA-PKCS1-v1_5",
				"public algorithm name") == GENERATED_TEST_PASS, SUITE,
			CASE_NAME, "unexpected RSA algorithm name");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"modulusLength", 13, &prop) == 0
			&& prop.kind == JSVAL_KIND_NUMBER && prop.as.number == 2048.0,
			SUITE, CASE_NAME, "unexpected RSA modulusLength");

	GENERATED_TEST_ASSERT(jsval_typed_array_new(&region,
			JSVAL_TYPED_ARRAY_UINT8, 16, &data_typed) == 0, SUITE, CASE_NAME,
			"failed to allocate RSA data");
	GENERATED_TEST_ASSERT(jsval_typed_array_buffer(&region, data_typed,
			&data_buffer) == 0, SUITE, CASE_NAME,
			"failed to read RSA data buffer");
	GENERATED_TEST_ASSERT(jsval_subtle_crypto_sign(&region, subtle_value,
			op_algorithm, private_key, data_buffer, &sign_promise) == 0, SUITE,
			CASE_NAME, "failed to call RSA sign");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain RSA sign");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, sign_promise,
			&signature) == 0
			&& signature.kind == JSVAL_KIND_ARRAY_BUFFER
			&& jsval_array_buffer_byte_length(&region, signature,
				&byte_length) == 0 && byte_length == 256, SUITE, CASE_NAME,
			"expected 256-byte RSA signature");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_verify(&region, subtle_value,
			op_algorithm, public_key, signature, data_buffer,
			&verify_promise) == 0, SUITE, CASE_NAME,
			"failed to call RSA verify");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain RSA verify");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, verify_promise,
			&result) == 0 && result.kind == JSVAL_KIND_BOOL
			&& result.as.boolean == 1, SUITE, CASE_NAME,
			"expected RSA verify to resolve true");

	/* JWK export public + private, then reimport public and reverify. */
	GENERATED_TEST_ASSERT(jsval_subtle_crypto_export_key(&region, subtle_value,
			jwk_format, public_key, &public_export_promise) == 0, SUITE,
			CASE_NAME, "failed to call RSA exportKey(public jwk)");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain RSA exportKey(public jwk)");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, public_export_promise,
			&jwk_public) == 0 && jwk_public.kind == JSVAL_KIND_OBJECT, SUITE,
			CASE_NAME, "expected RSA public JWK");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, jwk_public,
			(const uint8_t *)"kty", 3, &prop) == 0
			&& expect_string(&region, prop, "RSA", "public kty")
				== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected RSA public JWK kty");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, jwk_public,
			(const uint8_t *)"alg", 3, &prop) == 0
			&& expect_string(&region, prop, "RS256", "public alg")
				== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected RSA public JWK alg");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_export_key(&region, subtle_value,
			jwk_format, private_key, &private_export_promise) == 0, SUITE,
			CASE_NAME, "failed to call RSA exportKey(private jwk)");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain RSA exportKey(private jwk)");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, private_export_promise,
			&jwk_private) == 0 && jwk_private.kind == JSVAL_KIND_OBJECT, SUITE,
			CASE_NAME, "expected RSA private JWK");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, jwk_private,
			(const uint8_t *)"d", 1, &prop) == 0
			&& prop.kind == JSVAL_KIND_STRING, SUITE, CASE_NAME,
			"expected RSA private JWK d field");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_import_key(&region, subtle_value,
			jwk_format, jwk_public, key_algorithm, 1, verify_only_usages,
			&reimport_promise) == 0, SUITE, CASE_NAME,
			"failed to call RSA importKey(jwk)");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain RSA importKey(jwk)");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, reimport_promise,
			&reimported_public) == 0
			&& reimported_public.kind == JSVAL_KIND_CRYPTO_KEY, SUITE,
			CASE_NAME, "expected RSA reimported CryptoKey");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_verify(&region, subtle_value,
			op_algorithm, reimported_public, signature, data_buffer,
			&reverify_promise) == 0, SUITE, CASE_NAME,
			"failed to call RSA reverify");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0, SUITE,
			CASE_NAME, "failed to drain RSA reverify");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, reverify_promise,
			&result) == 0 && result.kind == JSVAL_KIND_BOOL
			&& result.as.boolean == 1, SUITE, CASE_NAME,
			"expected RSA reverify to resolve true");

	return generated_test_pass(SUITE, CASE_NAME);
}
