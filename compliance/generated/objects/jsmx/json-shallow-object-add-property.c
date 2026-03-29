#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "objects"
#define CASE_NAME "jsmx/json-shallow-object-add-property"

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
	static const uint8_t input[] =
		"{\"profile\":{\"name\":\"Ada\"},\"scores\":[1,2],\"status\":\"ok\"}";
	static const char expected[] =
		"{\"profile\":{\"name\":\"Ada\"},\"scores\":[1,2],\"status\":\"ok\",\"ready\":true}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t profile;
	jsval_t scores;
	size_t bytes;
	size_t before_used;

	/*
	 * Generated from repo-authored source:
	 * compliance/js/objects/jsmx/json-shallow-object-add-property.js
	 *
	 * This idiomatic flattened translation shallow-promotes only the parsed
	 * root object with translator-planned property capacity. Child values stay
	 * JSON-backed until the translator chooses to promote them separately.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_json_parse(&region, input, sizeof(input) - 1, 24,
			&root) == 0, SUITE, CASE_NAME,
			"failed to parse input JSON into a jsval region");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"profile", 7, &profile) == 0,
			SUITE, CASE_NAME, "failed to read profile before promotion");
	GENERATED_TEST_ASSERT(jsval_is_json_backed(profile) == 1, SUITE, CASE_NAME,
			"expected nested profile to remain JSON-backed before promotion");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"scores", 6, &scores) == 0,
			SUITE, CASE_NAME, "failed to read scores before promotion");
	GENERATED_TEST_ASSERT(jsval_is_json_backed(scores) == 1, SUITE, CASE_NAME,
			"expected nested scores to remain JSON-backed before promotion");

	before_used = region.used;
	GENERATED_TEST_ASSERT(jsval_promote_object_shallow_measure(&region, root, 4,
			&bytes) == 0, SUITE, CASE_NAME,
			"failed to measure shallow root promotion");
	GENERATED_TEST_ASSERT(jsval_promote_object_shallow_in_place(&region, &root, 4)
			== 0, SUITE, CASE_NAME,
			"failed to shallow-promote root with explicit property capacity");
	GENERATED_TEST_ASSERT(region.used == before_used + bytes, SUITE, CASE_NAME,
			"shallow root promotion consumed an unexpected number of bytes");
	GENERATED_TEST_ASSERT(jsval_region_set_root(&region, root) == 0,
			SUITE, CASE_NAME, "failed to persist promoted root");
	GENERATED_TEST_ASSERT(jsval_is_native(root) == 1, SUITE, CASE_NAME,
			"expected shallow-promoted root to be native");

	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"profile", 7, &profile) == 0,
			SUITE, CASE_NAME, "failed to read profile after root promotion");
	GENERATED_TEST_ASSERT(jsval_is_json_backed(profile) == 1, SUITE, CASE_NAME,
			"expected profile to remain JSON-backed after shallow root promotion");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"scores", 6, &scores) == 0,
			SUITE, CASE_NAME, "failed to read scores after root promotion");
	GENERATED_TEST_ASSERT(jsval_is_json_backed(scores) == 1, SUITE, CASE_NAME,
			"expected scores to remain JSON-backed after shallow root promotion");

	GENERATED_TEST_ASSERT(jsval_object_set_utf8(&region, root,
			(const uint8_t *)"ready", 5, jsval_bool(1)) == 0,
			SUITE, CASE_NAME, "failed to add ready property");
	GENERATED_TEST_ASSERT(expect_json(&region, root, expected)
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected emitted JSON after shallow root promotion");

	return generated_test_pass(SUITE, CASE_NAME);
}
