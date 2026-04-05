#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "objects"
#define CASE_NAME "jsmx/object-spread-merge-order"

int
main(void)
{
	static const uint8_t json_source[] = "{\"drop\":9,\"tail\":10}";
	static const uint8_t expected[] =
		"{\"keep\":true,\"drop\":9,\"items\":[],\"tail\":10}";
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t first;
	jsval_t second;
	jsval_t merged;
	jsval_t items;
	uint8_t actual[sizeof(expected) - 1];
	size_t actual_len = 0;

	/*
	 * Generated from repo-authored source:
	 * compliance/js/objects/jsmx/object-spread-merge-order.js
	 *
	 * This idiomatic flattened translation exercises object spread merge order
	 * through jsval_object_clone_own() plus jsval_object_copy_own().
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_object_new(&region, 3, &first) == 0,
			SUITE, CASE_NAME, "failed to allocate first source object");
	GENERATED_TEST_ASSERT(jsval_array_new(&region, 0, &items) == 0,
			SUITE, CASE_NAME, "failed to allocate items array");
	GENERATED_TEST_ASSERT(jsval_object_set_utf8(&region, first,
			(const uint8_t *)"keep", 4, jsval_bool(1)) == 0
			&& jsval_object_set_utf8(&region, first,
			(const uint8_t *)"drop", 4, jsval_number(7.0)) == 0
			&& jsval_object_set_utf8(&region, first,
			(const uint8_t *)"items", 5, items) == 0,
			SUITE, CASE_NAME, "failed to initialize first source object");
	GENERATED_TEST_ASSERT(jsval_json_parse(&region, json_source,
			sizeof(json_source) - 1, 8, &second) == 0,
			SUITE, CASE_NAME, "failed to parse second source object");
	GENERATED_TEST_ASSERT(jsval_object_clone_own(&region, first, 4, &merged) == 0,
			SUITE, CASE_NAME, "failed to clone first spread source object");
	GENERATED_TEST_ASSERT(jsval_object_copy_own(&region, merged, second) == 0,
			SUITE, CASE_NAME, "failed to merge second spread source object");
	GENERATED_TEST_ASSERT(jsval_copy_json(&region, merged, NULL, 0, &actual_len) == 0,
			SUITE, CASE_NAME, "failed to measure emitted merged JSON");
	GENERATED_TEST_ASSERT(actual_len == sizeof(expected) - 1, SUITE, CASE_NAME,
			"expected emitted merged JSON length %zu, got %zu",
			sizeof(expected) - 1, actual_len);
	GENERATED_TEST_ASSERT(jsval_copy_json(&region, merged, actual, actual_len,
			NULL) == 0, SUITE, CASE_NAME,
			"failed to copy emitted merged JSON");
	GENERATED_TEST_ASSERT(memcmp(actual, expected, sizeof(expected) - 1) == 0,
			SUITE, CASE_NAME,
			"expected object spread merge to preserve overwrite and append order");

	return generated_test_pass(SUITE, CASE_NAME);
}
