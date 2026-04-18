#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "iterators"
#define CASE_NAME "jsmx/string-iterator-parity"

static int
expect_bytes(jsval_region_t *region, jsval_t value, const uint8_t *expected,
		size_t expected_len, const char *label)
{
	size_t len = 0;
	uint8_t buf[16];

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
	jsval_t input;
	jsval_t iterator;
	jsval_t value;
	jsmethod_error_t error;
	int done = 0;

	jsval_region_init(&region, storage, sizeof(storage));

	/* ASCII "abc" yields three 1-byte code-point strings. */
	GENERATED_TEST_ASSERT(
			jsval_string_new_utf8(&region, (const uint8_t *)"abc", 3,
				&input) == 0,
			SUITE, CASE_NAME, "abc setup");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(
			jsval_get_iterator(&region, input,
				JSVAL_ITERATOR_SELECTOR_DEFAULT, &iterator,
				&error) == 0,
			SUITE, CASE_NAME, "abc iterator");
	GENERATED_TEST_ASSERT(
			jsval_iterator_next(&region, iterator, &done, &value,
				&error) == 0 && done == 0 &&
			expect_bytes(&region, value, (const uint8_t *)"a", 1,
				"abc[0]") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "abc step 0");
	GENERATED_TEST_ASSERT(
			jsval_iterator_next(&region, iterator, &done, &value,
				&error) == 0 && done == 0 &&
			expect_bytes(&region, value, (const uint8_t *)"b", 1,
				"abc[1]") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "abc step 1");
	GENERATED_TEST_ASSERT(
			jsval_iterator_next(&region, iterator, &done, &value,
				&error) == 0 && done == 0 &&
			expect_bytes(&region, value, (const uint8_t *)"c", 1,
				"abc[2]") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "abc step 2");
	GENERATED_TEST_ASSERT(
			jsval_iterator_next(&region, iterator, &done, &value,
				&error) == 0 && done == 1,
			SUITE, CASE_NAME, "abc not done after 3 steps");

	/* Multi-byte "café" (5 UTF-8 bytes = 63 61 66 C3 A9, 4 code
	 * points). "é" is 2 bytes but 1 code point. */
	{
		static const uint8_t cafe_e[] = {0xc3, 0xa9};
		GENERATED_TEST_ASSERT(
				jsval_string_new_utf8(&region,
					(const uint8_t *)"caf\xc3\xa9", 5, &input) == 0,
				SUITE, CASE_NAME, "café setup");
		GENERATED_TEST_ASSERT(
				jsval_get_iterator(&region, input,
					JSVAL_ITERATOR_SELECTOR_DEFAULT, &iterator,
					&error) == 0,
				SUITE, CASE_NAME, "café iterator");
		GENERATED_TEST_ASSERT(
				jsval_iterator_next(&region, iterator, &done, &value,
					&error) == 0 && done == 0 &&
				expect_bytes(&region, value,
					(const uint8_t *)"c", 1, "café[0]")
					== GENERATED_TEST_PASS,
				SUITE, CASE_NAME, "café step 0");
		GENERATED_TEST_ASSERT(
				jsval_iterator_next(&region, iterator, &done, &value,
					&error) == 0 && done == 0 &&
				expect_bytes(&region, value,
					(const uint8_t *)"a", 1, "café[1]")
					== GENERATED_TEST_PASS,
				SUITE, CASE_NAME, "café step 1");
		GENERATED_TEST_ASSERT(
				jsval_iterator_next(&region, iterator, &done, &value,
					&error) == 0 && done == 0 &&
				expect_bytes(&region, value,
					(const uint8_t *)"f", 1, "café[2]")
					== GENERATED_TEST_PASS,
				SUITE, CASE_NAME, "café step 2");
		GENERATED_TEST_ASSERT(
				jsval_iterator_next(&region, iterator, &done, &value,
					&error) == 0 && done == 0 &&
				expect_bytes(&region, value, cafe_e, sizeof(cafe_e),
					"café[3]") == GENERATED_TEST_PASS,
				SUITE, CASE_NAME, "café step 3 (é)");
		GENERATED_TEST_ASSERT(
				jsval_iterator_next(&region, iterator, &done, &value,
					&error) == 0 && done == 1,
				SUITE, CASE_NAME, "café not done after 4 steps");
	}

	/* Astral "🎉" (U+1F389 = 4-byte UTF-8). A correct code-point
	 * iterator yields exactly one step. */
	{
		static const uint8_t party[] = {0xf0, 0x9f, 0x8e, 0x89};
		GENERATED_TEST_ASSERT(
				jsval_string_new_utf8(&region, party, sizeof(party),
					&input) == 0,
				SUITE, CASE_NAME, "🎉 setup");
		GENERATED_TEST_ASSERT(
				jsval_get_iterator(&region, input,
					JSVAL_ITERATOR_SELECTOR_DEFAULT, &iterator,
					&error) == 0,
				SUITE, CASE_NAME, "🎉 iterator");
		GENERATED_TEST_ASSERT(
				jsval_iterator_next(&region, iterator, &done, &value,
					&error) == 0 && done == 0 &&
				expect_bytes(&region, value, party, sizeof(party),
					"🎉") == GENERATED_TEST_PASS,
				SUITE, CASE_NAME, "🎉 step 0 not emoji");
		GENERATED_TEST_ASSERT(
				jsval_iterator_next(&region, iterator, &done, &value,
					&error) == 0 && done == 1,
				SUITE, CASE_NAME,
				"🎉 not done after 1 step (would indicate "
				"UTF-16-unit iteration)");
	}

	return generated_test_pass(SUITE, CASE_NAME);
}
