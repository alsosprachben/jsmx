#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "objects"
#define CASE_NAME "jsmx/object-value-order"

int
main(void)
{
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t object;
	jsval_t values;
	jsval_t value;
	size_t len;
	size_t i;

	/*
	 * Generated from repo-authored source:
	 * compliance/js/objects/jsmx/object-value-order.js
	 *
	 * This idiomatic flattened translation exercises ordered native object
	 * value access through jsval_object_size() and jsval_object_value_at().
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

	GENERATED_TEST_ASSERT(jsval_object_value_at(&region, object, 1, &value) == 0,
			SUITE, CASE_NAME, "failed to read overwritten value");
	GENERATED_TEST_ASSERT(value.kind == JSVAL_KIND_NUMBER, SUITE, CASE_NAME,
			"expected overwritten value to be numeric");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, value, jsval_number(9.0)) == 1,
			SUITE, CASE_NAME, "expected overwrite to change the stored value");

	len = jsval_object_size(&region, object);
	GENERATED_TEST_ASSERT(jsval_array_new(&region, len, &values) == 0,
			SUITE, CASE_NAME, "failed to allocate values array");
	for (i = 0; i < len; i++) {
		GENERATED_TEST_ASSERT(jsval_object_value_at(&region, object, i, &value) == 0,
				SUITE, CASE_NAME, "failed to read native object value");
		GENERATED_TEST_ASSERT(jsval_array_push(&region, values, value) == 0,
				SUITE, CASE_NAME, "failed to append native value");
	}

	{
		static const uint8_t expected[] = "[1,9,3]";
		size_t actual_len = 0;
		uint8_t actual[sizeof(expected) - 1];

		GENERATED_TEST_ASSERT(jsval_copy_json(&region, values, NULL, 0,
				&actual_len) == 0, SUITE, CASE_NAME,
				"failed to measure emitted values JSON");
		GENERATED_TEST_ASSERT(actual_len == sizeof(expected) - 1, SUITE, CASE_NAME,
				"expected emitted values JSON length %zu, got %zu",
				sizeof(expected) - 1, actual_len);
		GENERATED_TEST_ASSERT(jsval_copy_json(&region, values, actual, actual_len,
				NULL) == 0, SUITE, CASE_NAME,
				"failed to copy emitted values JSON");
		GENERATED_TEST_ASSERT(memcmp(actual, expected, sizeof(expected) - 1) == 0,
				SUITE, CASE_NAME,
				"expected overwrite to preserve native value order");
	}

	return generated_test_pass(SUITE, CASE_NAME);
}
