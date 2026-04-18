#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "iterators"
#define CASE_NAME "jsmx/collection-iterator-parity"

static int
expect_string(jsval_region_t *region, jsval_t value, const char *expected,
		const char *label)
{
	size_t len = 0;
	size_t expected_len = strlen(expected);
	uint8_t buf[32];

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
	jsval_t set;
	jsval_t map;
	jsval_t iterator;
	jsval_t value;
	jsval_t key;
	jsval_t str_a, str_b, str_c, str_x, str_y;
	jsmethod_error_t error;
	int done = 0;

	jsval_region_init(&region, storage, sizeof(storage));

	/* Set iteration: values in insertion order. */
	GENERATED_TEST_ASSERT(
			jsval_set_new(&region, 4, &set) == 0 &&
			jsval_string_new_utf8(&region, (const uint8_t *)"a", 1,
				&str_a) == 0 &&
			jsval_string_new_utf8(&region, (const uint8_t *)"b", 1,
				&str_b) == 0 &&
			jsval_string_new_utf8(&region, (const uint8_t *)"c", 1,
				&str_c) == 0 &&
			jsval_set_add(&region, set, str_a) == 0 &&
			jsval_set_add(&region, set, str_b) == 0 &&
			jsval_set_add(&region, set, str_c) == 0,
			SUITE, CASE_NAME, "set setup");

	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(
			jsval_get_iterator(&region, set,
				JSVAL_ITERATOR_SELECTOR_DEFAULT, &iterator,
				&error) == 0,
			SUITE, CASE_NAME, "set DEFAULT iterator");
	GENERATED_TEST_ASSERT(
			jsval_iterator_next(&region, iterator, &done, &value,
				&error) == 0 && done == 0 &&
			expect_string(&region, value, "a", "set[0]")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "set step 0 != a");
	GENERATED_TEST_ASSERT(
			jsval_iterator_next(&region, iterator, &done, &value,
				&error) == 0 && done == 0 &&
			expect_string(&region, value, "b", "set[1]")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "set step 1 != b");
	GENERATED_TEST_ASSERT(
			jsval_iterator_next(&region, iterator, &done, &value,
				&error) == 0 && done == 0 &&
			expect_string(&region, value, "c", "set[2]")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "set step 2 != c");
	GENERATED_TEST_ASSERT(
			jsval_iterator_next(&region, iterator, &done, &value,
				&error) == 0 && done == 1,
			SUITE, CASE_NAME, "set not done after 3 steps");

	/* Map: KEYS → "x", "y"; VALUES → 1, 2; ENTRIES → pairs. */
	GENERATED_TEST_ASSERT(
			jsval_map_new(&region, 4, &map) == 0 &&
			jsval_string_new_utf8(&region, (const uint8_t *)"x", 1,
				&str_x) == 0 &&
			jsval_string_new_utf8(&region, (const uint8_t *)"y", 1,
				&str_y) == 0 &&
			jsval_map_set(&region, map, str_x,
				jsval_number(1.0)) == 0 &&
			jsval_map_set(&region, map, str_y,
				jsval_number(2.0)) == 0,
			SUITE, CASE_NAME, "map setup");

	/* KEYS */
	GENERATED_TEST_ASSERT(
			jsval_get_iterator(&region, map,
				JSVAL_ITERATOR_SELECTOR_KEYS, &iterator,
				&error) == 0,
			SUITE, CASE_NAME, "map KEYS iterator");
	GENERATED_TEST_ASSERT(
			jsval_iterator_next(&region, iterator, &done, &value,
				&error) == 0 && done == 0 &&
			expect_string(&region, value, "x", "keys[0]")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "map key 0 != x");
	GENERATED_TEST_ASSERT(
			jsval_iterator_next(&region, iterator, &done, &value,
				&error) == 0 && done == 0 &&
			expect_string(&region, value, "y", "keys[1]")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "map key 1 != y");
	GENERATED_TEST_ASSERT(
			jsval_iterator_next(&region, iterator, &done, &value,
				&error) == 0 && done == 1,
			SUITE, CASE_NAME, "map KEYS not done");

	/* VALUES */
	GENERATED_TEST_ASSERT(
			jsval_get_iterator(&region, map,
				JSVAL_ITERATOR_SELECTOR_VALUES, &iterator,
				&error) == 0,
			SUITE, CASE_NAME, "map VALUES iterator");
	GENERATED_TEST_ASSERT(
			jsval_iterator_next(&region, iterator, &done, &value,
				&error) == 0 && done == 0 && value.as.number == 1.0,
			SUITE, CASE_NAME, "map value 0 != 1");
	GENERATED_TEST_ASSERT(
			jsval_iterator_next(&region, iterator, &done, &value,
				&error) == 0 && done == 0 && value.as.number == 2.0,
			SUITE, CASE_NAME, "map value 1 != 2");

	/* ENTRIES (requires _next_entry) */
	GENERATED_TEST_ASSERT(
			jsval_get_iterator(&region, map,
				JSVAL_ITERATOR_SELECTOR_ENTRIES, &iterator,
				&error) == 0,
			SUITE, CASE_NAME, "map ENTRIES iterator");
	GENERATED_TEST_ASSERT(
			jsval_iterator_next_entry(&region, iterator, &done, &key,
				&value, &error) == 0 && done == 0 &&
			expect_string(&region, key, "x", "entries[0].key")
				== GENERATED_TEST_PASS &&
			value.as.number == 1.0,
			SUITE, CASE_NAME, "map entry 0 != (x, 1)");
	GENERATED_TEST_ASSERT(
			jsval_iterator_next_entry(&region, iterator, &done, &key,
				&value, &error) == 0 && done == 0 &&
			expect_string(&region, key, "y", "entries[1].key")
				== GENERATED_TEST_PASS &&
			value.as.number == 2.0,
			SUITE, CASE_NAME, "map entry 1 != (y, 2)");
	GENERATED_TEST_ASSERT(
			jsval_iterator_next_entry(&region, iterator, &done, &key,
				&value, &error) == 0 && done == 1,
			SUITE, CASE_NAME, "map ENTRIES not done");

	return generated_test_pass(SUITE, CASE_NAME);
}
