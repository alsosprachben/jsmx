#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "objects"
#define CASE_NAME "jsmx/object-copy-own-native"

int
main(void)
{
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t dst;
	jsval_t src;
	jsval_t items;
	jsval_t key;
	size_t actual_len = 0;

	/*
	 * Generated from repo-authored source:
	 * compliance/js/objects/jsmx/object-copy-own-native.js
	 *
	 * This idiomatic flattened translation exercises shallow own-property
	 * copy from one native object into another through jsval_object_copy_own().
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_object_new(&region, 3, &dst) == 0,
			SUITE, CASE_NAME, "failed to allocate destination object");
	GENERATED_TEST_ASSERT(jsval_object_new(&region, 2, &src) == 0,
			SUITE, CASE_NAME, "failed to allocate source object");
	GENERATED_TEST_ASSERT(jsval_array_new(&region, 0, &items) == 0,
			SUITE, CASE_NAME, "failed to allocate items array");
	GENERATED_TEST_ASSERT(jsval_object_set_utf8(&region, dst,
			(const uint8_t *)"keep", 4, jsval_bool(1)) == 0,
			SUITE, CASE_NAME, "failed to set keep property");
	GENERATED_TEST_ASSERT(jsval_object_set_utf8(&region, dst,
			(const uint8_t *)"drop", 4, jsval_number(7.0)) == 0,
			SUITE, CASE_NAME, "failed to set drop property");
	GENERATED_TEST_ASSERT(jsval_object_set_utf8(&region, src,
			(const uint8_t *)"drop", 4, jsval_number(8.0)) == 0,
			SUITE, CASE_NAME, "failed to set source drop property");
	GENERATED_TEST_ASSERT(jsval_object_set_utf8(&region, src,
			(const uint8_t *)"items", 5, items) == 0,
			SUITE, CASE_NAME, "failed to set source items property");
	GENERATED_TEST_ASSERT(jsval_object_copy_own(&region, dst, src) == 0,
			SUITE, CASE_NAME, "failed to copy native own properties");

	{
		static const uint8_t expected[] = "{\"keep\":true,\"drop\":8,\"items\":[]}";
		uint8_t actual[sizeof(expected) - 1];

		GENERATED_TEST_ASSERT(jsval_object_key_at(&region, dst, 0, &key) == 0,
				SUITE, CASE_NAME, "failed to read destination key 0");
		GENERATED_TEST_ASSERT(jsval_string_copy_utf8(&region, key, actual,
				4, NULL) == 0 && memcmp(actual, "keep", 4) == 0,
				SUITE, CASE_NAME, "expected keep to remain the first key");
		GENERATED_TEST_ASSERT(jsval_object_key_at(&region, dst, 1, &key) == 0,
				SUITE, CASE_NAME, "failed to read destination key 1");
		GENERATED_TEST_ASSERT(jsval_string_copy_utf8(&region, key, actual,
				4, NULL) == 0 && memcmp(actual, "drop", 4) == 0,
				SUITE, CASE_NAME, "expected drop overwrite to preserve key order");
		GENERATED_TEST_ASSERT(jsval_copy_json(&region, dst, NULL, 0, &actual_len) == 0,
				SUITE, CASE_NAME, "failed to measure emitted destination JSON");
		GENERATED_TEST_ASSERT(actual_len == sizeof(expected) - 1, SUITE, CASE_NAME,
				"expected emitted destination JSON length %zu, got %zu",
				sizeof(expected) - 1, actual_len);
		GENERATED_TEST_ASSERT(jsval_copy_json(&region, dst, actual, actual_len,
				NULL) == 0, SUITE, CASE_NAME,
				"failed to copy emitted destination JSON");
		GENERATED_TEST_ASSERT(memcmp(actual, expected, sizeof(expected) - 1) == 0,
				SUITE, CASE_NAME,
				"expected native own-property copy to overwrite in place and append new keys");
	}

	return generated_test_pass(SUITE, CASE_NAME);
}
