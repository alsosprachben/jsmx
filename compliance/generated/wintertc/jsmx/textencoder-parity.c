#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "wintertc"
#define CASE_NAME "jsmx/textencoder-parity"

static int
expect_bytes(jsval_region_t *region, jsval_t typed_array,
		const uint8_t *expected, size_t expected_len, const char *label)
{
	jsval_t backing;
	uint8_t *bytes;
	size_t len = 0;

	if (typed_array.kind != JSVAL_KIND_TYPED_ARRAY) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: expected TypedArray", label);
	}
	if (jsval_typed_array_length(region, typed_array) != expected_len) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: length mismatch (want %zu)", label,
				expected_len);
	}
	if (jsval_typed_array_buffer(region, typed_array, &backing) != 0 ||
			jsval_array_buffer_bytes_mut(region, backing,
				&bytes, &len) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: failed to access backing bytes", label);
	}
	if (len < expected_len ||
			memcmp(bytes, expected, expected_len) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: byte contents mismatch", label);
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

	jsval_region_init(&region, storage, sizeof(storage));

	/* ASCII "hello" */
	{
		static const uint8_t expected[] = {0x68, 0x65, 0x6c, 0x6c, 0x6f};
		GENERATED_TEST_ASSERT(
				jsval_string_new_utf8(&region,
					(const uint8_t *)"hello", 5, &input) == 0,
				SUITE, CASE_NAME, "failed to build ASCII input");
		GENERATED_TEST_ASSERT(
				jsval_text_encode_utf8(&region, input, &encoded) == 0,
				SUITE, CASE_NAME, "encode failed for ASCII");
		GENERATED_TEST_ASSERT(
				expect_bytes(&region, encoded, expected,
					sizeof(expected), "ASCII")
					== GENERATED_TEST_PASS,
				SUITE, CASE_NAME, "ASCII bytes mismatch");
	}

	/* "café" — é is 0xC3 0xA9 */
	{
		static const uint8_t expected[] = {
			0x63, 0x61, 0x66, 0xc3, 0xa9
		};
		GENERATED_TEST_ASSERT(
				jsval_string_new_utf8(&region,
					(const uint8_t *)"caf\xc3\xa9", 5, &input) == 0,
				SUITE, CASE_NAME, "failed to build multibyte input");
		GENERATED_TEST_ASSERT(
				jsval_text_encode_utf8(&region, input, &encoded) == 0,
				SUITE, CASE_NAME, "encode failed for multibyte");
		GENERATED_TEST_ASSERT(
				expect_bytes(&region, encoded, expected,
					sizeof(expected), "multibyte")
					== GENERATED_TEST_PASS,
				SUITE, CASE_NAME, "multibyte bytes mismatch");
	}

	/* "🎉" (U+1F389) — F0 9F 8E 89 */
	{
		static const uint8_t expected[] = {0xf0, 0x9f, 0x8e, 0x89};
		GENERATED_TEST_ASSERT(
				jsval_string_new_utf8(&region,
					(const uint8_t *)"\xf0\x9f\x8e\x89", 4, &input) == 0,
				SUITE, CASE_NAME, "failed to build emoji input");
		GENERATED_TEST_ASSERT(
				jsval_text_encode_utf8(&region, input, &encoded) == 0,
				SUITE, CASE_NAME, "encode failed for emoji");
		GENERATED_TEST_ASSERT(
				expect_bytes(&region, encoded, expected,
					sizeof(expected), "emoji")
					== GENERATED_TEST_PASS,
				SUITE, CASE_NAME, "emoji bytes mismatch");
	}

	return generated_test_pass(SUITE, CASE_NAME);
}
