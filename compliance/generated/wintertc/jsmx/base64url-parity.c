#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "wintertc"
#define CASE_NAME "jsmx/base64url-parity"

static int
expect_string(jsval_region_t *region, jsval_t value, const char *expected,
		const char *label)
{
	size_t len = 0;
	size_t expected_len = strlen(expected);
	uint8_t buf[expected_len ? expected_len : 1];

	if (value.kind != JSVAL_KIND_STRING) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: expected string", label);
	}
	if (jsval_string_copy_utf8(region, value, NULL, 0, &len) < 0 ||
			len != expected_len) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: len %zu != %zu", label, len, expected_len);
	}
	if (jsval_string_copy_utf8(region, value, buf, len, NULL) < 0 ||
			memcmp(buf, expected, len) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: bytes mismatch", label);
	}
	return GENERATED_TEST_PASS;
}

static int
expect_array_bytes(jsval_region_t *region, jsval_t typed_array,
		const uint8_t *expected, size_t expected_len, const char *label)
{
	jsval_t backing;
	uint8_t *bytes;
	size_t len = 0;

	if (typed_array.kind != JSVAL_KIND_TYPED_ARRAY ||
			jsval_typed_array_length(region, typed_array)
				!= expected_len) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: length mismatch", label);
	}
	if (jsval_typed_array_buffer(region, typed_array, &backing) != 0 ||
			jsval_array_buffer_bytes_mut(region, backing,
				&bytes, &len) != 0 ||
			memcmp(bytes, expected, expected_len) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: bytes mismatch", label);
	}
	return GENERATED_TEST_PASS;
}

int
main(void)
{
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t ab;
	jsval_t encoded;
	jsval_t b64_str;
	jsval_t decoded;
	uint8_t *bytes;
	size_t len = 0;

	jsval_region_init(&region, storage, sizeof(storage));

	/* Alphabet: [0xfb, 0xef, 0xff] → "--__" (both base64url-only
	 * characters appear). Standard base64 of the same input would
	 * be "++//". */
	{
		static const uint8_t input[] = {0xfb, 0xef, 0xff};
		GENERATED_TEST_ASSERT(
				jsval_array_buffer_new(&region, sizeof(input), &ab) == 0 &&
				jsval_array_buffer_bytes_mut(&region, ab, &bytes,
					&len) == 0,
				SUITE, CASE_NAME, "alphabet input setup");
		memcpy(bytes, input, sizeof(input));
		GENERATED_TEST_ASSERT(
				jsval_base64url_encode(&region, ab, &encoded) == 0,
				SUITE, CASE_NAME, "encode alphabet bytes");
		GENERATED_TEST_ASSERT(
				expect_string(&region, encoded, "--__", "alphabet")
					== GENERATED_TEST_PASS,
				SUITE, CASE_NAME, "alphabet encode result");
	}

	/* No padding: 1-byte input → 2-char output (no "=="). */
	{
		static const uint8_t input[] = {0x66};
		GENERATED_TEST_ASSERT(
				jsval_array_buffer_new(&region, sizeof(input), &ab) == 0 &&
				jsval_array_buffer_bytes_mut(&region, ab, &bytes,
					&len) == 0,
				SUITE, CASE_NAME, "1-byte input setup");
		memcpy(bytes, input, sizeof(input));
		GENERATED_TEST_ASSERT(
				jsval_base64url_encode(&region, ab, &encoded) == 0,
				SUITE, CASE_NAME, "encode 1-byte");
		GENERATED_TEST_ASSERT(
				expect_string(&region, encoded, "Zg", "1-byte no pad")
					== GENERATED_TEST_PASS,
				SUITE, CASE_NAME, "1-byte no-pad result");
	}

	/* Round-trip: decode "--__" → [0xfb, 0xef, 0xff]. */
	{
		static const uint8_t expected[] = {0xfb, 0xef, 0xff};
		GENERATED_TEST_ASSERT(
				jsval_string_new_utf8(&region,
					(const uint8_t *)"--__", 4, &b64_str) == 0,
				SUITE, CASE_NAME, "b64url string setup");
		GENERATED_TEST_ASSERT(
				jsval_base64url_decode(&region, b64_str, &decoded) == 0,
				SUITE, CASE_NAME, "decode --__");
		GENERATED_TEST_ASSERT(
				expect_array_bytes(&region, decoded, expected,
					sizeof(expected), "alphabet")
					== GENERATED_TEST_PASS,
				SUITE, CASE_NAME, "alphabet decode bytes");
	}

	/* Round-trip: decode "Zg" (no padding) → [0x66]. */
	{
		static const uint8_t expected[] = {0x66};
		GENERATED_TEST_ASSERT(
				jsval_string_new_utf8(&region,
					(const uint8_t *)"Zg", 2, &b64_str) == 0,
				SUITE, CASE_NAME, "Zg string setup");
		GENERATED_TEST_ASSERT(
				jsval_base64url_decode(&region, b64_str, &decoded) == 0,
				SUITE, CASE_NAME, "decode Zg");
		GENERATED_TEST_ASSERT(
				expect_array_bytes(&region, decoded, expected, 1,
					"1-byte") == GENERATED_TEST_PASS,
				SUITE, CASE_NAME, "1-byte decode bytes");
	}

	return generated_test_pass(SUITE, CASE_NAME);
}
