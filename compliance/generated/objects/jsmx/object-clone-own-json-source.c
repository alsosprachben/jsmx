#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "objects"
#define CASE_NAME "jsmx/object-clone-own-json-source"

int
main(void)
{
	static const uint8_t json_source[] = "{\"z\":1,\"a\":2}";
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t src;
	jsval_t clone;
	jsval_t key;
	size_t actual_len = 0;

	/*
	 * Generated from repo-authored source:
	 * compliance/js/objects/jsmx/object-clone-own-json-source.js
	 *
	 * This idiomatic flattened translation exercises fresh shallow own-property
	 * cloning from a JSON-backed source into a new native object through
	 * jsval_object_clone_own().
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_json_parse(&region, json_source,
			sizeof(json_source) - 1, 8, &src) == 0,
			SUITE, CASE_NAME, "failed to parse JSON source object");
	GENERATED_TEST_ASSERT(jsval_object_clone_own(&region, src, 2, &clone) == 0,
			SUITE, CASE_NAME, "failed to clone JSON-backed source object");
	GENERATED_TEST_ASSERT(jsval_is_native(clone) == 1,
			SUITE, CASE_NAME, "expected clone to be a native object");
	GENERATED_TEST_ASSERT(jsval_object_key_at(&region, clone, 0, &key) == 0,
			SUITE, CASE_NAME, "failed to read clone key 0");
	GENERATED_TEST_ASSERT(jsval_string_copy_utf8(&region, key, NULL, 0,
			&actual_len) == 0 && actual_len == 1, SUITE, CASE_NAME,
			"expected first clone key to have length 1");
	GENERATED_TEST_ASSERT(jsval_object_set_utf8(&region, clone,
			(const uint8_t *)"z", 1, jsval_number(7.0)) == 0,
			SUITE, CASE_NAME, "failed to mutate cloned z property");

	{
		static const uint8_t expected_src[] = "{\"z\":1,\"a\":2}";
		static const uint8_t expected_clone[] = "{\"z\":7,\"a\":2}";
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
				"expected JSON-backed source to remain unchanged after fresh clone mutation");

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
				"expected fresh clone from JSON source to preserve parsed key order");
	}

	return generated_test_pass(SUITE, CASE_NAME);
}
