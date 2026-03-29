#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "objects"
#define CASE_NAME "jsmx/array-push-length"

int
main(void)
{
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t array;
	jsval_t got;

	/*
	 * Generated from repo-authored source:
	 * compliance/js/objects/jsmx/array-push-length.js
	 *
	 * This idiomatic flattened translation exercises dense native array growth
	 * through push and explicit logical-length writes.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_array_new(&region, 4, &array) == 0,
			SUITE, CASE_NAME, "failed to allocate native array");
	GENERATED_TEST_ASSERT(jsval_array_push(&region, array, jsval_number(1.0)) == 0,
			SUITE, CASE_NAME, "failed to push first array value");
	GENERATED_TEST_ASSERT(jsval_array_push(&region, array, jsval_number(2.0)) == 0,
			SUITE, CASE_NAME, "failed to push second array value");
	GENERATED_TEST_ASSERT(jsval_array_set_length(&region, array, 4) == 0,
			SUITE, CASE_NAME, "failed to grow array length");
	GENERATED_TEST_ASSERT(jsval_array_length(&region, array) == 4,
			SUITE, CASE_NAME, "expected grown length to be 4");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, array, 2, &got) == 0,
			SUITE, CASE_NAME, "failed to read grown dense slot 2");
	GENERATED_TEST_ASSERT(got.kind == JSVAL_KIND_UNDEFINED, SUITE, CASE_NAME,
			"expected grown dense slot 2 to read back as undefined");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, array, 3, &got) == 0,
			SUITE, CASE_NAME, "failed to read grown dense slot 3");
	GENERATED_TEST_ASSERT(got.kind == JSVAL_KIND_UNDEFINED, SUITE, CASE_NAME,
			"expected grown dense slot 3 to read back as undefined");

	GENERATED_TEST_ASSERT(jsval_array_set_length(&region, array, 2) == 0,
			SUITE, CASE_NAME, "failed to shrink array length");
	GENERATED_TEST_ASSERT(jsval_array_length(&region, array) == 2,
			SUITE, CASE_NAME, "expected shrunk length to be 2");

	{
		static const uint8_t expected[] = "[1,2]";
		size_t actual_len = 0;
		uint8_t actual[sizeof(expected) - 1];

		GENERATED_TEST_ASSERT(jsval_copy_json(&region, array, NULL, 0,
				&actual_len) == 0, SUITE, CASE_NAME,
				"failed to measure emitted JSON");
		GENERATED_TEST_ASSERT(actual_len == sizeof(expected) - 1, SUITE, CASE_NAME,
				"expected emitted JSON length %zu, got %zu",
				sizeof(expected) - 1, actual_len);
		GENERATED_TEST_ASSERT(jsval_copy_json(&region, array, actual, actual_len,
				NULL) == 0, SUITE, CASE_NAME,
				"failed to copy emitted JSON");
		GENERATED_TEST_ASSERT(memcmp(actual, expected,
				sizeof(expected) - 1) == 0, SUITE, CASE_NAME,
				"expected truncation to be reflected in emitted JSON");
	}

	return generated_test_pass(SUITE, CASE_NAME);
}
