#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "objects"
#define CASE_NAME "jsmx/object-key-delete-compaction"

int
main(void)
{
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t object;
	jsval_t items;
	jsval_t keys;
	jsval_t key;
	size_t len;
	size_t i;
	int deleted;

	/*
	 * Generated from repo-authored source:
	 * compliance/js/objects/jsmx/object-key-delete-compaction.js
	 *
	 * This idiomatic flattened translation exercises ordered native object
	 * key access after delete compacts later properties.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_object_new(&region, 3, &object) == 0,
			SUITE, CASE_NAME, "failed to allocate native object");
	GENERATED_TEST_ASSERT(jsval_array_new(&region, 1, &items) == 0,
			SUITE, CASE_NAME, "failed to allocate items array");
	GENERATED_TEST_ASSERT(jsval_object_set_utf8(&region, object,
			(const uint8_t *)"keep", 4, jsval_bool(1)) == 0,
			SUITE, CASE_NAME, "failed to set keep property");
	GENERATED_TEST_ASSERT(jsval_object_set_utf8(&region, object,
			(const uint8_t *)"drop", 4, jsval_number(7.0)) == 0,
			SUITE, CASE_NAME, "failed to set drop property");
	GENERATED_TEST_ASSERT(jsval_object_set_utf8(&region, object,
			(const uint8_t *)"items", 5, items) == 0,
			SUITE, CASE_NAME, "failed to set items property");
	GENERATED_TEST_ASSERT(jsval_object_delete_utf8(&region, object,
			(const uint8_t *)"drop", 4, &deleted) == 0,
			SUITE, CASE_NAME, "failed to delete drop property");
	GENERATED_TEST_ASSERT(deleted == 1, SUITE, CASE_NAME,
			"expected drop deletion to report success");

	len = jsval_object_size(&region, object);
	GENERATED_TEST_ASSERT(jsval_array_new(&region, len, &keys) == 0,
			SUITE, CASE_NAME, "failed to allocate keys array");
	for (i = 0; i < len; i++) {
		GENERATED_TEST_ASSERT(jsval_object_key_at(&region, object, i, &key) == 0,
				SUITE, CASE_NAME, "failed to read compacted key");
		GENERATED_TEST_ASSERT(jsval_array_push(&region, keys, key) == 0,
				SUITE, CASE_NAME, "failed to append compacted key");
	}

	{
		static const uint8_t expected[] = "[\"keep\",\"items\"]";
		size_t actual_len = 0;
		uint8_t actual[sizeof(expected) - 1];

		GENERATED_TEST_ASSERT(jsval_copy_json(&region, keys, NULL, 0,
				&actual_len) == 0, SUITE, CASE_NAME,
				"failed to measure compacted keys JSON");
		GENERATED_TEST_ASSERT(actual_len == sizeof(expected) - 1, SUITE, CASE_NAME,
				"expected compacted keys JSON length %zu, got %zu",
				sizeof(expected) - 1, actual_len);
		GENERATED_TEST_ASSERT(jsval_copy_json(&region, keys, actual, actual_len,
				NULL) == 0, SUITE, CASE_NAME,
				"failed to copy compacted keys JSON");
		GENERATED_TEST_ASSERT(memcmp(actual, expected, sizeof(expected) - 1) == 0,
				SUITE, CASE_NAME,
				"expected delete to compact later native keys");
	}

	return generated_test_pass(SUITE, CASE_NAME);
}
