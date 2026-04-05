#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "objects"
#define CASE_NAME "jsmx/object-entry-order"

int
main(void)
{
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t object;
	jsval_t entries;
	jsval_t pair;
	jsval_t key;
	jsval_t value;
	size_t len;
	size_t i;

	/*
	 * Generated from repo-authored source:
	 * compliance/js/objects/jsmx/object-entry-order.js
	 *
	 * This idiomatic flattened translation exercises Object.entries-style
	 * lowering through jsval_object_key_at() and jsval_object_value_at().
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

	len = jsval_object_size(&region, object);
	GENERATED_TEST_ASSERT(jsval_array_new(&region, len, &entries) == 0,
			SUITE, CASE_NAME, "failed to allocate entries array");
	for (i = 0; i < len; i++) {
		GENERATED_TEST_ASSERT(jsval_array_new(&region, 2, &pair) == 0,
				SUITE, CASE_NAME, "failed to allocate entry pair");
		GENERATED_TEST_ASSERT(jsval_object_key_at(&region, object, i, &key) == 0,
				SUITE, CASE_NAME, "failed to read entry key");
		GENERATED_TEST_ASSERT(jsval_object_value_at(&region, object, i, &value) == 0,
				SUITE, CASE_NAME, "failed to read entry value");
		GENERATED_TEST_ASSERT(jsval_array_push(&region, pair, key) == 0,
				SUITE, CASE_NAME, "failed to append entry key");
		GENERATED_TEST_ASSERT(jsval_array_push(&region, pair, value) == 0,
				SUITE, CASE_NAME, "failed to append entry value");
		GENERATED_TEST_ASSERT(jsval_array_push(&region, entries, pair) == 0,
				SUITE, CASE_NAME, "failed to append entry pair");
	}

	{
		static const uint8_t expected[] = "[[\"z\",1],[\"a\",9],[\"m\",3]]";
		size_t actual_len = 0;
		uint8_t actual[sizeof(expected) - 1];

		GENERATED_TEST_ASSERT(jsval_copy_json(&region, entries, NULL, 0,
				&actual_len) == 0, SUITE, CASE_NAME,
				"failed to measure emitted entries JSON");
		GENERATED_TEST_ASSERT(actual_len == sizeof(expected) - 1, SUITE, CASE_NAME,
				"expected emitted entries JSON length %zu, got %zu",
				sizeof(expected) - 1, actual_len);
		GENERATED_TEST_ASSERT(jsval_copy_json(&region, entries, actual, actual_len,
				NULL) == 0, SUITE, CASE_NAME,
				"failed to copy emitted entries JSON");
		GENERATED_TEST_ASSERT(memcmp(actual, expected, sizeof(expected) - 1) == 0,
				SUITE, CASE_NAME,
				"expected overwrite to preserve native entry order");
	}

	return generated_test_pass(SUITE, CASE_NAME);
}
