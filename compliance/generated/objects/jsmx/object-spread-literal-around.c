#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "objects"
#define CASE_NAME "jsmx/object-spread-literal-around"

static int
expect_json(jsval_region_t *region, jsval_t value, const char *expected)
{
	size_t expected_len = strlen(expected);
	size_t actual_len = 0;
	uint8_t buf[expected_len ? expected_len : 1];

	if (jsval_copy_json(region, value, NULL, 0, &actual_len) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"failed to measure emitted JSON");
	}
	if (actual_len != expected_len) {
		return generated_test_fail(SUITE, CASE_NAME,
				"expected %zu JSON bytes, got %zu",
				expected_len, actual_len);
	}
	if (jsval_copy_json(region, value, buf, actual_len, NULL) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"failed to copy emitted JSON");
	}
	if (memcmp(buf, expected, expected_len) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"emitted JSON did not match expected output");
	}
	return GENERATED_TEST_PASS;
}

int
main(void)
{
	static const uint8_t json_source[] = "{\"mid\":7,\"tail\":8}";
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t src;
	jsval_t out;

	/*
	 * Generated from repo-authored source:
	 * compliance/js/objects/jsmx/object-spread-literal-around.js
	 *
	 * This idiomatic flattened translation exercises literal properties
	 * around an object spread segment through explicit set plus copy_own.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_json_parse(&region, json_source,
			sizeof(json_source) - 1, 8, &src) == 0,
			SUITE, CASE_NAME, "failed to parse spread source object");
	GENERATED_TEST_ASSERT(jsval_object_new(&region, 3, &out) == 0,
			SUITE, CASE_NAME, "failed to allocate output object");
	GENERATED_TEST_ASSERT(jsval_object_set_utf8(&region, out,
			(const uint8_t *)"head", 4, jsval_number(1.0)) == 0,
			SUITE, CASE_NAME, "failed to set head literal property");
	GENERATED_TEST_ASSERT(jsval_object_copy_own(&region, out, src) == 0,
			SUITE, CASE_NAME, "failed to spread source object");
	GENERATED_TEST_ASSERT(jsval_object_set_utf8(&region, out,
			(const uint8_t *)"tail", 4, jsval_number(2.0)) == 0,
			SUITE, CASE_NAME, "failed to overwrite tail literal property");
	GENERATED_TEST_ASSERT(expect_json(&region, out,
			"{\"head\":1,\"mid\":7,\"tail\":2}") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME,
			"expected literal properties around object spread to preserve key order");

	return generated_test_pass(SUITE, CASE_NAME);
}
