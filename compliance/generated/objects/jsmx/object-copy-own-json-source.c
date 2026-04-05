#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "objects"
#define CASE_NAME "jsmx/object-copy-own-json-source"

int
main(void)
{
	static const uint8_t json_source[] = "{\"z\":1,\"a\":2}";
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t dst;
	jsval_t src;
	size_t actual_len = 0;

	/*
	 * Generated from repo-authored source:
	 * compliance/js/objects/jsmx/object-copy-own-json-source.js
	 *
	 * This idiomatic flattened translation exercises shallow own-property
	 * copy from a JSON-backed source object into a native destination.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_object_new(&region, 3, &dst) == 0,
			SUITE, CASE_NAME, "failed to allocate destination object");
	GENERATED_TEST_ASSERT(jsval_json_parse(&region, json_source,
			sizeof(json_source) - 1, 8, &src) == 0,
			SUITE, CASE_NAME, "failed to parse JSON source object");
	GENERATED_TEST_ASSERT(jsval_object_set_utf8(&region, dst,
			(const uint8_t *)"keep", 4, jsval_bool(1)) == 0,
			SUITE, CASE_NAME, "failed to set keep property");
	GENERATED_TEST_ASSERT(jsval_object_copy_own(&region, dst, src) == 0,
			SUITE, CASE_NAME, "failed to copy JSON-backed own properties");

	{
		static const uint8_t expected[] = "{\"keep\":true,\"z\":1,\"a\":2}";
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
				"expected JSON-backed own-property copy to preserve parsed source order");
	}

	return generated_test_pass(SUITE, CASE_NAME);
}
