#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "wintertc"
#define CASE_NAME "jsmx/text-emoji-roundtrip"

int
main(void)
{
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t input;
	jsval_t encoded;
	jsval_t decoded;
	jsval_t backing;
	uint8_t *bytes;
	size_t len = 0;
	uint8_t buf[8];
	static const uint8_t emoji_bytes[] = {0xf0, 0x9f, 0x8e, 0x89};

	jsval_region_init(&region, storage, sizeof(storage));

	/* Build the source string from its UTF-8 encoding. */
	GENERATED_TEST_ASSERT(
			jsval_string_new_utf8(&region, emoji_bytes,
				sizeof(emoji_bytes), &input) == 0,
			SUITE, CASE_NAME, "failed to build emoji source");

	/* Encode and verify the byte sequence. */
	GENERATED_TEST_ASSERT(
			jsval_text_encode_utf8(&region, input, &encoded) == 0,
			SUITE, CASE_NAME, "encode failed");
	GENERATED_TEST_ASSERT(
			jsval_typed_array_length(&region, encoded)
				== sizeof(emoji_bytes),
			SUITE, CASE_NAME, "encoded length mismatch");
	GENERATED_TEST_ASSERT(
			jsval_typed_array_buffer(&region, encoded, &backing) == 0 &&
			jsval_array_buffer_bytes_mut(&region, backing,
				&bytes, &len) == 0,
			SUITE, CASE_NAME, "failed to access encoded bytes");
	GENERATED_TEST_ASSERT(
			memcmp(bytes, emoji_bytes, sizeof(emoji_bytes)) == 0,
			SUITE, CASE_NAME, "encoded bytes mismatch");

	/* Decode back and verify string-equal to source. */
	GENERATED_TEST_ASSERT(
			jsval_text_decode_utf8(&region, encoded, &decoded) == 0,
			SUITE, CASE_NAME, "decode failed");
	GENERATED_TEST_ASSERT(
			jsval_string_copy_utf8(&region, decoded, NULL, 0, &len) == 0 &&
			len == sizeof(emoji_bytes),
			SUITE, CASE_NAME, "decoded length mismatch");
	GENERATED_TEST_ASSERT(
			jsval_string_copy_utf8(&region, decoded, buf, len, NULL) == 0
			&& memcmp(buf, emoji_bytes, sizeof(emoji_bytes)) == 0,
			SUITE, CASE_NAME, "round-trip string mismatch");

	return generated_test_pass(SUITE, CASE_NAME);
}
