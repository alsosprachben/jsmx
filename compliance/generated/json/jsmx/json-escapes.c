#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "json"
#define CASE_NAME "jsmx/json-escapes"

static int
expect_string_bytes(jsval_region_t *region, jsval_t value,
		const uint8_t *expected, size_t expected_len,
		const char *label)
{
	size_t len = 0;
	uint8_t buf[128];

	if (value.kind != JSVAL_KIND_STRING) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: not a string", label);
	}
	if (jsval_string_copy_utf8(region, value, NULL, 0, &len) != 0 ||
			len != expected_len || len >= sizeof(buf)) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: len %zu != %zu", label, len, expected_len);
	}
	if (jsval_string_copy_utf8(region, value, buf, len, NULL) != 0 ||
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
	jsval_t parsed;
	size_t json_len = 0;
	uint8_t json_buf[128];

	jsval_region_init(&region, storage, sizeof(storage));

	/* Simple escapes: \", \\, \n, \t round-trip. */
	{
		const char *source =
				"\"line\\nfeed\\\"quote\\\\back\\ttab\"";
		static const uint8_t expected_chars[] =
				"line\nfeed\"quote\\back\ttab";
		GENERATED_TEST_ASSERT(
				jsval_json_parse(&region, (const uint8_t *)source,
					strlen(source), 4, &parsed) == 0,
				SUITE, CASE_NAME, "parse simple-escape");
		GENERATED_TEST_ASSERT(
				expect_string_bytes(&region, parsed, expected_chars,
					sizeof(expected_chars) - 1, "simple decoded")
					== GENERATED_TEST_PASS,
				SUITE, CASE_NAME, "simple decoded bytes");
		/* Re-serialize; escape codec should emit the same
		 * canonical form (subject to slash/forward-slash rules,
		 * which jsmx chooses to escape or not consistently). */
		GENERATED_TEST_ASSERT(
				jsval_copy_json(&region, parsed, NULL, 0,
					&json_len) == 0 && json_len < sizeof(json_buf),
				SUITE, CASE_NAME, "serialize simple measure");
		GENERATED_TEST_ASSERT(
				jsval_copy_json(&region, parsed, json_buf, json_len,
					NULL) == 0,
				SUITE, CASE_NAME, "serialize simple fill");
		GENERATED_TEST_ASSERT(
				json_len == strlen(source) &&
				memcmp(json_buf, source, json_len) == 0,
				SUITE, CASE_NAME, "simple round-trip bytes");
	}

	/* \u0041 → "A". Codec may output either "\u0041" or "A"; we
	 * assert the decoded string is the single char "A". */
	{
		const char *source = "\"\\u0041\"";
		GENERATED_TEST_ASSERT(
				jsval_json_parse(&region, (const uint8_t *)source,
					strlen(source), 4, &parsed) == 0,
				SUITE, CASE_NAME, "parse \\u0041");
		GENERATED_TEST_ASSERT(
				expect_string_bytes(&region, parsed,
					(const uint8_t *)"A", 1, "A")
					== GENERATED_TEST_PASS,
				SUITE, CASE_NAME, "\\u0041 decoded");
	}

	/* Surrogate pair \\ud83d\\ude00 → U+1F600 (4-byte UTF-8
	 * F0 9F 98 80). */
	{
		const char *source = "\"\\ud83d\\ude00\"";
		static const uint8_t utf8_emoji[] = {0xf0, 0x9f, 0x98, 0x80};
		GENERATED_TEST_ASSERT(
				jsval_json_parse(&region, (const uint8_t *)source,
					strlen(source), 4, &parsed) == 0,
				SUITE, CASE_NAME, "parse surrogate pair");
		GENERATED_TEST_ASSERT(
				expect_string_bytes(&region, parsed, utf8_emoji,
					sizeof(utf8_emoji), "emoji")
					== GENERATED_TEST_PASS,
				SUITE, CASE_NAME, "surrogate decoded bytes");
	}

	/* Plain string with no escapes. */
	{
		const char *source = "\"plain\"";
		GENERATED_TEST_ASSERT(
				jsval_json_parse(&region, (const uint8_t *)source,
					strlen(source), 4, &parsed) == 0,
				SUITE, CASE_NAME, "parse plain");
		GENERATED_TEST_ASSERT(
				expect_string_bytes(&region, parsed,
					(const uint8_t *)"plain", 5, "plain")
					== GENERATED_TEST_PASS,
				SUITE, CASE_NAME, "plain decoded");
		GENERATED_TEST_ASSERT(
				jsval_copy_json(&region, parsed, NULL, 0,
					&json_len) == 0 && json_len < sizeof(json_buf),
				SUITE, CASE_NAME, "plain serialize measure");
		GENERATED_TEST_ASSERT(
				jsval_copy_json(&region, parsed, json_buf, json_len,
					NULL) == 0 &&
				json_len == strlen(source) &&
				memcmp(json_buf, source, json_len) == 0,
				SUITE, CASE_NAME, "plain round-trip bytes");
	}

	return generated_test_pass(SUITE, CASE_NAME);
}
