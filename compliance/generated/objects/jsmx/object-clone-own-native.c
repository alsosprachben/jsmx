#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "objects"
#define CASE_NAME "jsmx/object-clone-own-native"

int
main(void)
{
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t src;
	jsval_t clone;
	jsval_t items;
	size_t actual_len = 0;

	/*
	 * Generated from repo-authored source:
	 * compliance/js/objects/jsmx/object-clone-own-native.js
	 *
	 * This idiomatic flattened translation exercises fresh shallow own-property
	 * cloning from one native object into a new native object through
	 * jsval_object_clone_own().
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_object_new(&region, 2, &src) == 0,
			SUITE, CASE_NAME, "failed to allocate source object");
	GENERATED_TEST_ASSERT(jsval_array_new(&region, 0, &items) == 0,
			SUITE, CASE_NAME, "failed to allocate items array");
	GENERATED_TEST_ASSERT(jsval_object_set_utf8(&region, src,
			(const uint8_t *)"keep", 4, jsval_bool(1)) == 0,
			SUITE, CASE_NAME, "failed to set keep property");
	GENERATED_TEST_ASSERT(jsval_object_set_utf8(&region, src,
			(const uint8_t *)"items", 5, items) == 0,
			SUITE, CASE_NAME, "failed to set items property");
	GENERATED_TEST_ASSERT(jsval_object_clone_own(&region, src, 2, &clone) == 0,
			SUITE, CASE_NAME, "failed to clone native source object");
	GENERATED_TEST_ASSERT(jsval_object_set_utf8(&region, clone,
			(const uint8_t *)"keep", 4, jsval_bool(0)) == 0,
			SUITE, CASE_NAME, "failed to mutate cloned keep property");

	{
		static const uint8_t expected_src[] = "{\"keep\":true,\"items\":[]}";
		static const uint8_t expected_clone[] = "{\"keep\":false,\"items\":[]}";
		uint8_t actual_src[sizeof(expected_src) - 1];
		uint8_t actual_clone[sizeof(expected_clone) - 1];

		GENERATED_TEST_ASSERT(jsval_copy_json(&region, src, NULL, 0, &actual_len) == 0,
				SUITE, CASE_NAME, "failed to measure emitted source JSON");
		GENERATED_TEST_ASSERT(actual_len == sizeof(expected_src) - 1,
				SUITE, CASE_NAME,
				"expected emitted source JSON length %zu, got %zu",
				sizeof(expected_src) - 1, actual_len);
		GENERATED_TEST_ASSERT(jsval_copy_json(&region, src, actual_src, actual_len,
				NULL) == 0, SUITE, CASE_NAME,
				"failed to copy emitted source JSON");
		GENERATED_TEST_ASSERT(memcmp(actual_src, expected_src,
				sizeof(expected_src) - 1) == 0, SUITE, CASE_NAME,
				"expected native source to remain unchanged after fresh clone mutation");

		GENERATED_TEST_ASSERT(jsval_copy_json(&region, clone, NULL, 0, &actual_len) == 0,
				SUITE, CASE_NAME, "failed to measure emitted clone JSON");
		GENERATED_TEST_ASSERT(actual_len == sizeof(expected_clone) - 1,
				SUITE, CASE_NAME,
				"expected emitted clone JSON length %zu, got %zu",
				sizeof(expected_clone) - 1, actual_len);
		GENERATED_TEST_ASSERT(jsval_copy_json(&region, clone, actual_clone,
				actual_len, NULL) == 0, SUITE, CASE_NAME,
				"failed to copy emitted clone JSON");
		GENERATED_TEST_ASSERT(memcmp(actual_clone, expected_clone,
				sizeof(expected_clone) - 1) == 0, SUITE, CASE_NAME,
				"expected fresh native clone to preserve key order and allow independent top-level writes");
	}

	return generated_test_pass(SUITE, CASE_NAME);
}
