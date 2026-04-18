#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "wintertc"
#define CASE_NAME "jsmx/textdecoder-parity"

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
				"%s: length mismatch (want %zu)", label, expected_len);
	}
	if (jsval_string_copy_utf8(region, value, buf, len, NULL) < 0 ||
			memcmp(buf, expected, len) != 0) {
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
	jsval_t input;
	jsval_t encoded;
	jsval_t decoded;
	jsval_t ab;
	uint8_t *bytes;
	size_t len = 0;

	jsval_region_init(&region, storage, sizeof(storage));

	/* Round-trip "hello" through encode → decode. */
	GENERATED_TEST_ASSERT(
			jsval_string_new_utf8(&region, (const uint8_t *)"hello", 5,
				&input) == 0 &&
			jsval_text_encode_utf8(&region, input, &encoded) == 0 &&
			jsval_text_decode_utf8(&region, encoded, &decoded) == 0,
			SUITE, CASE_NAME, "ASCII round-trip failed");
	GENERATED_TEST_ASSERT(
			expect_string(&region, decoded, "hello", "ASCII")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "ASCII result mismatch");

	/* Round-trip "café" (multi-byte). */
	GENERATED_TEST_ASSERT(
			jsval_string_new_utf8(&region,
				(const uint8_t *)"caf\xc3\xa9", 5, &input) == 0 &&
			jsval_text_encode_utf8(&region, input, &encoded) == 0 &&
			jsval_text_decode_utf8(&region, encoded, &decoded) == 0,
			SUITE, CASE_NAME, "multibyte round-trip failed");
	GENERATED_TEST_ASSERT(
			expect_string(&region, decoded, "caf\xc3\xa9", "multibyte")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "multibyte result mismatch");

	/* Decode directly from an ArrayBuffer. */
	GENERATED_TEST_ASSERT(
			jsval_array_buffer_new(&region, 5, &ab) == 0 &&
			jsval_array_buffer_bytes_mut(&region, ab, &bytes, &len) == 0,
			SUITE, CASE_NAME, "failed to create source ArrayBuffer");
	memcpy(bytes, "world", 5);
	GENERATED_TEST_ASSERT(
			jsval_text_decode_utf8(&region, ab, &decoded) == 0,
			SUITE, CASE_NAME, "ArrayBuffer decode failed");
	GENERATED_TEST_ASSERT(
			expect_string(&region, decoded, "world", "ArrayBuffer")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "ArrayBuffer decode mismatch");

	return generated_test_pass(SUITE, CASE_NAME);
}
