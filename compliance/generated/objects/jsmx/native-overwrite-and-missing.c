#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "objects"
#define CASE_NAME "jsmx/native-overwrite-and-missing"

static int
expect_json(jsval_region_t *region, jsval_t value, const char *expected)
{
	size_t expected_len = strlen(expected);
	size_t actual_len = 0;
	uint8_t buf[expected_len ? expected_len : 1];

	if (jsval_copy_json(region, value, NULL, 0, &actual_len) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"failed to measure emitted JSON");
	}
	if (actual_len != expected_len) {
		return generated_test_fail(SUITE, CASE_NAME,
				"expected %zu JSON bytes, got %zu",
				expected_len, actual_len);
	}
	if (jsval_copy_json(region, value, buf, actual_len, NULL) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"failed to copy emitted JSON");
	}
	if (memcmp(buf, expected, expected_len) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"emitted JSON did not match expected output");
	}
	return GENERATED_TEST_PASS;
}

int
main(void)
{
	static const char expected[] =
		"{\"keep\":9,\"items\":[1,8],\"nested\":{\"flag\":true}}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t items;
	jsval_t nested;
	jsval_t got;
	size_t object_size;
	size_t array_len;

	/*
	 * Generated from repo-authored source:
	 * compliance/js/objects/jsmx/native-overwrite-and-missing.js
	 *
	 * This idiomatic flattened translation exercises native overwrite behavior
	 * directly and proves that missing property/index reads still return
	 * undefined without changing object size or array length.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_object_new(&region, 3, &root) == 0,
			SUITE, CASE_NAME, "failed to allocate native root object");
	GENERATED_TEST_ASSERT(jsval_array_new(&region, 2, &items) == 0,
			SUITE, CASE_NAME, "failed to allocate native array");
	GENERATED_TEST_ASSERT(jsval_object_new(&region, 1, &nested) == 0,
			SUITE, CASE_NAME, "failed to allocate nested object");
	GENERATED_TEST_ASSERT(jsval_object_set_utf8(&region, nested,
			(const uint8_t *)"flag", 4, jsval_bool(1)) == 0,
			SUITE, CASE_NAME, "failed to set nested.flag");
	GENERATED_TEST_ASSERT(jsval_array_set(&region, items, 0, jsval_number(1.0))
			== 0, SUITE, CASE_NAME, "failed to set items[0]");
	GENERATED_TEST_ASSERT(jsval_array_set(&region, items, 1, jsval_number(2.0))
			== 0, SUITE, CASE_NAME, "failed to set items[1]");
	GENERATED_TEST_ASSERT(jsval_object_set_utf8(&region, root,
			(const uint8_t *)"keep", 4, jsval_number(7.0)) == 0,
			SUITE, CASE_NAME, "failed to set keep");
	GENERATED_TEST_ASSERT(jsval_object_set_utf8(&region, root,
			(const uint8_t *)"items", 5, items) == 0,
			SUITE, CASE_NAME, "failed to set items");
	GENERATED_TEST_ASSERT(jsval_object_set_utf8(&region, root,
			(const uint8_t *)"nested", 6, nested) == 0,
			SUITE, CASE_NAME, "failed to set nested");

	object_size = jsval_object_size(&region, root);
	GENERATED_TEST_ASSERT(jsval_object_set_utf8(&region, root,
			(const uint8_t *)"keep", 4, jsval_number(9.0)) == 0,
			SUITE, CASE_NAME, "failed to overwrite keep");
	GENERATED_TEST_ASSERT(jsval_object_size(&region, root) == object_size,
			SUITE, CASE_NAME, "object overwrite unexpectedly changed size");

	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"missing", 7, &got) == 0,
			SUITE, CASE_NAME, "failed missing-property read");
	GENERATED_TEST_ASSERT(got.kind == JSVAL_KIND_UNDEFINED, SUITE, CASE_NAME,
			"expected missing property read to return undefined");

	array_len = jsval_array_length(&region, items);
	GENERATED_TEST_ASSERT(jsval_array_set(&region, items, 1, jsval_number(8.0))
			== 0, SUITE, CASE_NAME, "failed to overwrite items[1]");
	GENERATED_TEST_ASSERT(jsval_array_length(&region, items) == array_len,
			SUITE, CASE_NAME, "array overwrite unexpectedly changed length");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, items, 4, &got) == 0,
			SUITE, CASE_NAME, "failed out-of-range native array read");
	GENERATED_TEST_ASSERT(got.kind == JSVAL_KIND_UNDEFINED, SUITE, CASE_NAME,
			"expected out-of-range native array read to return undefined");

	GENERATED_TEST_ASSERT(expect_json(&region, root, expected)
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected emitted JSON after native overwrites");

	return generated_test_pass(SUITE, CASE_NAME);
}
