#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "objects"
#define CASE_NAME "jsmx/json-object-value-order"

int
main(void)
{
	static const uint8_t input[] = "{\"z\":1,\"a\":2,\"m\":3}";
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t object;
	jsval_t values;
	jsval_t value;
	size_t len;
	size_t i;

	/*
	 * Generated from repo-authored source:
	 * compliance/js/objects/jsmx/json-object-value-order.js
	 *
	 * This idiomatic flattened translation exercises ordered value access over
	 * a JSON-backed object without implicit promotion.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_json_parse(&region, input, sizeof(input) - 1, 16,
			&object) == 0, SUITE, CASE_NAME,
			"failed to parse JSON-backed object");

	len = jsval_object_size(&region, object);
	GENERATED_TEST_ASSERT(jsval_array_new(&region, len, &values) == 0,
			SUITE, CASE_NAME, "failed to allocate values array");
	for (i = 0; i < len; i++) {
		GENERATED_TEST_ASSERT(jsval_object_value_at(&region, object, i, &value) == 0,
				SUITE, CASE_NAME, "failed to read JSON-backed value");
		GENERATED_TEST_ASSERT(jsval_array_push(&region, values, value) == 0,
				SUITE, CASE_NAME, "failed to append JSON-backed value");
	}

	{
		static const uint8_t expected[] = "[1,2,3]";
		size_t actual_len = 0;
		uint8_t actual[sizeof(expected) - 1];

		GENERATED_TEST_ASSERT(jsval_copy_json(&region, values, NULL, 0,
				&actual_len) == 0, SUITE, CASE_NAME,
				"failed to measure JSON-backed values output");
		GENERATED_TEST_ASSERT(actual_len == sizeof(expected) - 1, SUITE, CASE_NAME,
				"expected JSON-backed values length %zu, got %zu",
				sizeof(expected) - 1, actual_len);
		GENERATED_TEST_ASSERT(jsval_copy_json(&region, values, actual, actual_len,
				NULL) == 0, SUITE, CASE_NAME,
				"failed to copy JSON-backed values output");
		GENERATED_TEST_ASSERT(memcmp(actual, expected, sizeof(expected) - 1) == 0,
				SUITE, CASE_NAME,
				"expected JSON-backed values to preserve parsed source order");
	}

	return generated_test_pass(SUITE, CASE_NAME);
}
