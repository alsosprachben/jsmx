#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "objects"
#define CASE_NAME "jsmx/object-key-order"

int
main(void)
{
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t object;
	jsval_t keys;
	jsval_t key;
	size_t len;
	size_t i;

	/*
	 * Generated from repo-authored source:
	 * compliance/js/objects/jsmx/object-key-order.js
	 *
	 * This idiomatic flattened translation exercises ordered native object
	 * key access through jsval_object_size() and jsval_object_key_at().
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_object_new(&region, 3, &object) == 0,
			SUITE, CASE_NAME, "failed to allocate native object");
	GENERATED_TEST_ASSERT(jsval_object_set_utf8(&region, object,
			(const uint8_t *)"z", 1, jsval_number(1.0)) == 0,
			SUITE, CASE_NAME, "failed to set z property");
	GENERATED_TEST_ASSERT(jsval_object_set_utf8(&region, object,
			(const uint8_t *)"a", 1, jsval_number(2.0)) == 0,
			SUITE, CASE_NAME, "failed to set a property");
	GENERATED_TEST_ASSERT(jsval_object_set_utf8(&region, object,
			(const uint8_t *)"m", 1, jsval_number(3.0)) == 0,
			SUITE, CASE_NAME, "failed to set m property");
	GENERATED_TEST_ASSERT(jsval_object_set_utf8(&region, object,
			(const uint8_t *)"a", 1, jsval_number(9.0)) == 0,
			SUITE, CASE_NAME, "failed to overwrite a property");

	GENERATED_TEST_ASSERT(jsval_object_key_at(&region, object, 0, &key) == 0,
			SUITE, CASE_NAME, "failed to read key 0");
	GENERATED_TEST_ASSERT(key.kind == JSVAL_KIND_STRING, SUITE, CASE_NAME,
			"expected key 0 to be a string");
	{
		static const uint8_t expected_key[] = "z";
		size_t actual_len = 0;
		uint8_t actual[sizeof(expected_key) - 1];

		GENERATED_TEST_ASSERT(jsval_string_copy_utf8(&region, key, NULL, 0,
				&actual_len) == 0, SUITE, CASE_NAME,
				"failed to measure key 0");
		GENERATED_TEST_ASSERT(actual_len == sizeof(expected_key) - 1,
				SUITE, CASE_NAME, "unexpected key 0 length");
		GENERATED_TEST_ASSERT(jsval_string_copy_utf8(&region, key, actual,
				actual_len, NULL) == 0, SUITE, CASE_NAME,
				"failed to copy key 0");
		GENERATED_TEST_ASSERT(memcmp(actual, expected_key,
				sizeof(expected_key) - 1) == 0, SUITE, CASE_NAME,
				"expected overwrite to preserve the first key");
	}

	len = jsval_object_size(&region, object);
	GENERATED_TEST_ASSERT(jsval_array_new(&region, len, &keys) == 0,
			SUITE, CASE_NAME, "failed to allocate keys array");
	for (i = 0; i < len; i++) {
		GENERATED_TEST_ASSERT(jsval_object_key_at(&region, object, i, &key) == 0,
				SUITE, CASE_NAME, "failed to read native object key");
		GENERATED_TEST_ASSERT(jsval_array_push(&region, keys, key) == 0,
				SUITE, CASE_NAME, "failed to append native key");
	}

	{
		static const uint8_t expected[] = "[\"z\",\"a\",\"m\"]";
		size_t actual_len = 0;
		uint8_t actual[sizeof(expected) - 1];

		GENERATED_TEST_ASSERT(jsval_copy_json(&region, keys, NULL, 0,
				&actual_len) == 0, SUITE, CASE_NAME,
				"failed to measure emitted keys JSON");
		GENERATED_TEST_ASSERT(actual_len == sizeof(expected) - 1, SUITE, CASE_NAME,
				"expected emitted keys JSON length %zu, got %zu",
				sizeof(expected) - 1, actual_len);
		GENERATED_TEST_ASSERT(jsval_copy_json(&region, keys, actual, actual_len,
				NULL) == 0, SUITE, CASE_NAME,
				"failed to copy emitted keys JSON");
		GENERATED_TEST_ASSERT(memcmp(actual, expected, sizeof(expected) - 1) == 0,
				SUITE, CASE_NAME,
				"expected overwrite to preserve native key order");
	}

	return generated_test_pass(SUITE, CASE_NAME);
}
