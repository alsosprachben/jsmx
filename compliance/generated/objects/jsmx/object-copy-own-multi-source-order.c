#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "objects"
#define CASE_NAME "jsmx/object-copy-own-multi-source-order"

int
main(void)
{
	static const uint8_t json_source[] = "{\"b\":9,\"c\":3}";
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t dst;
	jsval_t first;
	jsval_t second;
	size_t actual_len = 0;

	/*
	 * Generated from repo-authored source:
	 * compliance/js/objects/jsmx/object-copy-own-multi-source-order.js
	 *
	 * This idiomatic flattened translation exercises repeated shallow
	 * own-property copy in Object.assign-style left-to-right source order.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_object_new(&region, 3, &dst) == 0,
			SUITE, CASE_NAME, "failed to allocate destination object");
	GENERATED_TEST_ASSERT(jsval_object_new(&region, 2, &first) == 0,
			SUITE, CASE_NAME, "failed to allocate first source object");
	GENERATED_TEST_ASSERT(jsval_json_parse(&region, json_source,
			sizeof(json_source) - 1, 8, &second) == 0,
			SUITE, CASE_NAME, "failed to parse second source object");
	GENERATED_TEST_ASSERT(jsval_object_set_utf8(&region, first,
			(const uint8_t *)"a", 1, jsval_number(1.0)) == 0,
			SUITE, CASE_NAME, "failed to set first source a property");
	GENERATED_TEST_ASSERT(jsval_object_set_utf8(&region, first,
			(const uint8_t *)"b", 1, jsval_number(2.0)) == 0,
			SUITE, CASE_NAME, "failed to set first source b property");
	GENERATED_TEST_ASSERT(jsval_object_copy_own(&region, dst, first) == 0,
			SUITE, CASE_NAME, "failed to copy first source own properties");
	GENERATED_TEST_ASSERT(jsval_object_copy_own(&region, dst, second) == 0,
			SUITE, CASE_NAME, "failed to copy second source own properties");

	{
		static const uint8_t expected[] = "{\"a\":1,\"b\":9,\"c\":3}";
		uint8_t actual[sizeof(expected) - 1];

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
				"expected repeated own-property copy to preserve overwrite and append order");
	}

	return generated_test_pass(SUITE, CASE_NAME);
}
