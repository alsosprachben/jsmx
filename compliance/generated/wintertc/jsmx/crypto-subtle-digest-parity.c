#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "wintertc"
#define CASE_NAME "jsmx/crypto-subtle-digest-parity"

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
				"%s: expected %zu digest bytes, got %zu", label,
				expected_len, actual_len);
	}
	if (jsval_array_buffer_copy_bytes(region, value, actual_buf, actual_len,
			NULL) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: failed to copy ArrayBuffer bytes", label);
	}
	if (memcmp(actual_buf, expected, expected_len) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: unexpected digest bytes", label);
	}
	return GENERATED_TEST_PASS;
}

int
main(void)
{
	static const uint8_t sha1_zeros_16[] = {
		0xe1, 0x29, 0xf2, 0x7c, 0x51, 0x03, 0xbc, 0x5c,
		0xc4, 0x4b, 0xcd, 0xf0, 0xa1, 0x5e, 0x16, 0x0d,
		0x44, 0x50, 0x66, 0xff
	};
	static const uint8_t sha256_zeros_16[] = {
		0x37, 0x47, 0x08, 0xff, 0xf7, 0x71, 0x9d, 0xd5,
		0x97, 0x9e, 0xc8, 0x75, 0xd5, 0x6c, 0xd2, 0x28,
		0x6f, 0x6d, 0x3c, 0xf7, 0xec, 0x31, 0x7a, 0x3b,
		0x25, 0x63, 0x2a, 0xab, 0x28, 0xec, 0x37, 0xbb
	};
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t crypto_value;
	jsval_t subtle_value;
	jsval_t input;
	jsval_t input_buffer;
	jsval_t algorithm_name;
	jsval_t algorithm_object;
	jsval_t digest_a;
	jsval_t digest_b;
	jsval_t result;
	jsval_promise_state_t state;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_crypto_new(&region, &crypto_value) == 0,
			SUITE, CASE_NAME, "failed to allocate crypto object");
	GENERATED_TEST_ASSERT(jsval_crypto_subtle(&region, crypto_value,
			&subtle_value) == 0, SUITE, CASE_NAME,
			"failed to read crypto.subtle");
	GENERATED_TEST_ASSERT(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8,
			16, &input) == 0, SUITE, CASE_NAME,
			"failed to allocate digest input");
	GENERATED_TEST_ASSERT(jsval_typed_array_buffer(&region, input, &input_buffer)
			== 0, SUITE, CASE_NAME,
			"failed to read digest input buffer");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"SHA-256", 7, &algorithm_name) == 0,
			SUITE, CASE_NAME, "failed to allocate SHA-256 string");

	GENERATED_TEST_ASSERT(jsval_subtle_crypto_digest(&region, subtle_value,
			algorithm_name, input, &digest_a) == 0, SUITE, CASE_NAME,
			"failed to call subtle.digest(string)");
	GENERATED_TEST_ASSERT(digest_a.kind == JSVAL_KIND_PROMISE, SUITE, CASE_NAME,
			"expected digest(string) to return Promise");
	GENERATED_TEST_ASSERT(jsval_promise_state(&region, digest_a, &state) == 0
			&& state == JSVAL_PROMISE_STATE_PENDING,
			SUITE, CASE_NAME,
			"expected digest(string) promise to stay pending before drain");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0,
			SUITE, CASE_NAME, "failed to drain digest(string) microtasks");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_NONE,
			SUITE, CASE_NAME,
			"unexpected error while draining digest(string)");
	GENERATED_TEST_ASSERT(jsval_promise_state(&region, digest_a, &state) == 0
			&& state == JSVAL_PROMISE_STATE_FULFILLED,
			SUITE, CASE_NAME,
			"expected digest(string) promise to fulfill");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, digest_a, &result) == 0,
			SUITE, CASE_NAME,
			"failed to read digest(string) result");
	GENERATED_TEST_ASSERT(expect_array_buffer_bytes(&region, result,
			sha256_zeros_16, sizeof(sha256_zeros_16), "digest(string)")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected digest(string) bytes");

	GENERATED_TEST_ASSERT(jsval_object_new(&region, 1, &algorithm_object) == 0,
			SUITE, CASE_NAME,
			"failed to allocate digest algorithm object");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"SHA-1", 5, &result) == 0,
			SUITE, CASE_NAME, "failed to allocate SHA-1 string");
	GENERATED_TEST_ASSERT(jsval_object_set_utf8(&region, algorithm_object,
			(const uint8_t *)"name", 4, result) == 0,
			SUITE, CASE_NAME,
			"failed to set digest algorithm object name");
	GENERATED_TEST_ASSERT(jsval_subtle_crypto_digest(&region, subtle_value,
			algorithm_object, input_buffer, &digest_b) == 0,
			SUITE, CASE_NAME, "failed to call subtle.digest({name})");
	GENERATED_TEST_ASSERT(jsval_promise_state(&region, digest_b, &state) == 0
			&& state == JSVAL_PROMISE_STATE_PENDING,
			SUITE, CASE_NAME,
			"expected digest({name}) promise to stay pending before drain");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0,
			SUITE, CASE_NAME, "failed to drain digest({name}) microtasks");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_NONE,
			SUITE, CASE_NAME,
			"unexpected error while draining digest({name})");
	GENERATED_TEST_ASSERT(jsval_promise_state(&region, digest_b, &state) == 0
			&& state == JSVAL_PROMISE_STATE_FULFILLED,
			SUITE, CASE_NAME,
			"expected digest({name}) promise to fulfill");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, digest_b, &result) == 0,
			SUITE, CASE_NAME,
			"failed to read digest({name}) result");
	GENERATED_TEST_ASSERT(expect_array_buffer_bytes(&region, result,
			sha1_zeros_16, sizeof(sha1_zeros_16), "digest({name})")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected digest({name}) bytes");
	return generated_test_pass(SUITE, CASE_NAME);
}
