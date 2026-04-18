#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "collections"
#define CASE_NAME "jsmx/map-parity"

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
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t map;
	jsval_t key_a, key_b, key_c, key_missing;
	jsval_t out;
	size_t size = 0;
	int has = 0;
	int removed = 0;

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(
			jsval_map_new(&region, 4, &map) == 0 &&
			map.kind == JSVAL_KIND_MAP,
			SUITE, CASE_NAME, "map_new failed");
	GENERATED_TEST_ASSERT(
			jsval_map_size(&region, map, &size) == 0 && size == 0,
			SUITE, CASE_NAME, "initial size != 0");

	GENERATED_TEST_ASSERT(
			jsval_string_new_utf8(&region, (const uint8_t *)"a", 1,
				&key_a) == 0 &&
			jsval_string_new_utf8(&region, (const uint8_t *)"b", 1,
				&key_b) == 0 &&
			jsval_string_new_utf8(&region, (const uint8_t *)"c", 1,
				&key_c) == 0 &&
			jsval_string_new_utf8(&region, (const uint8_t *)"missing",
				7, &key_missing) == 0,
			SUITE, CASE_NAME, "key setup");

	/* set + get + has */
	GENERATED_TEST_ASSERT(
			jsval_map_set(&region, map, key_a, jsval_number(1.0)) == 0,
			SUITE, CASE_NAME, "set('a',1)");
	GENERATED_TEST_ASSERT(
			jsval_map_get(&region, map, key_a, &out) == 0 &&
			out.kind == JSVAL_KIND_NUMBER && out.as.number == 1.0,
			SUITE, CASE_NAME, "get('a') != 1");
	GENERATED_TEST_ASSERT(
			jsval_map_size(&region, map, &size) == 0 && size == 1,
			SUITE, CASE_NAME, "size != 1 after one set");

	/* overwrite same key preserves size */
	GENERATED_TEST_ASSERT(
			jsval_map_set(&region, map, key_a, jsval_number(2.0)) == 0,
			SUITE, CASE_NAME, "set('a',2) overwrite");
	GENERATED_TEST_ASSERT(
			jsval_map_size(&region, map, &size) == 0 && size == 1,
			SUITE, CASE_NAME, "overwrite changed size");
	GENERATED_TEST_ASSERT(
			jsval_map_get(&region, map, key_a, &out) == 0 &&
			out.as.number == 2.0,
			SUITE, CASE_NAME, "get('a') after overwrite != 2");

	/* has */
	GENERATED_TEST_ASSERT(
			jsval_map_has(&region, map, key_a, &has) == 0 && has == 1,
			SUITE, CASE_NAME, "has('a') false");
	GENERATED_TEST_ASSERT(
			jsval_map_has(&region, map, key_missing, &has) == 0 &&
			has == 0,
			SUITE, CASE_NAME, "has('missing') true");

	/* Additional inserts; insertion-order iteration via index. */
	GENERATED_TEST_ASSERT(
			jsval_map_set(&region, map, key_b, jsval_number(10.0)) == 0 &&
			jsval_map_set(&region, map, key_c, jsval_number(20.0)) == 0 &&
			jsval_map_size(&region, map, &size) == 0 && size == 3,
			SUITE, CASE_NAME, "adds for b, c");

	GENERATED_TEST_ASSERT(
			jsval_map_key_at(&region, map, 0, &out) == 0 &&
			expect_string(&region, out, "a", "key_at(0)")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "key_at(0)");
	GENERATED_TEST_ASSERT(
			jsval_map_value_at(&region, map, 0, &out) == 0 &&
			out.kind == JSVAL_KIND_NUMBER && out.as.number == 2.0,
			SUITE, CASE_NAME, "value_at(0) != 2");
	GENERATED_TEST_ASSERT(
			jsval_map_key_at(&region, map, 1, &out) == 0 &&
			expect_string(&region, out, "b", "key_at(1)")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "key_at(1)");
	GENERATED_TEST_ASSERT(
			jsval_map_key_at(&region, map, 2, &out) == 0 &&
			expect_string(&region, out, "c", "key_at(2)")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "key_at(2)");

	/* delete + clear */
	GENERATED_TEST_ASSERT(
			jsval_map_delete(&region, map, key_a, &removed) == 0 &&
			removed == 1 &&
			jsval_map_size(&region, map, &size) == 0 && size == 2,
			SUITE, CASE_NAME, "delete 'a'");
	GENERATED_TEST_ASSERT(
			jsval_map_clear(&region, map) == 0 &&
			jsval_map_size(&region, map, &size) == 0 && size == 0,
			SUITE, CASE_NAME, "clear");

	return generated_test_pass(SUITE, CASE_NAME);
}
