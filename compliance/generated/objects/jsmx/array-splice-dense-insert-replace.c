#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "objects"
#define CASE_NAME "jsmx/array-splice-dense-insert-replace"

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
	static const char expected_json[] =
		"{\"insertTarget\":[1,7,8,4],\"insertRemoved\":[],\"replaceTarget\":[1,9,4],\"replaceRemoved\":[2,3]}";
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t output;
	jsval_t insert_target;
	jsval_t insert_removed;
	jsval_t replace_target;
	jsval_t replace_removed;
	jsval_t insert_values[] = {jsval_number(7.0), jsval_number(8.0)};
	jsval_t replace_values[] = {jsval_number(9.0)};

	/*
	 * Generated from repo-authored source:
	 * compliance/js/objects/jsmx/array-splice-dense-insert-replace.js
	 *
	 * This idiomatic flattened translation exercises insert-only and replace
	 * edits through jsval_array_splice_dense().
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_object_new(&region, 4, &output) == 0,
			SUITE, CASE_NAME, "failed to allocate output object");
	GENERATED_TEST_ASSERT(jsval_array_new(&region, 4, &insert_target) == 0,
			SUITE, CASE_NAME, "failed to allocate insert target");
	GENERATED_TEST_ASSERT(jsval_array_push(&region, insert_target, jsval_number(1.0)) == 0
			&& jsval_array_push(&region, insert_target, jsval_number(4.0)) == 0,
			SUITE, CASE_NAME, "failed to initialize insert target");
	GENERATED_TEST_ASSERT(jsval_array_splice_dense(&region, insert_target, 1, 0,
			insert_values, 2, &insert_removed) == 0, SUITE, CASE_NAME,
			"failed to perform dense insertion splice");

	GENERATED_TEST_ASSERT(jsval_array_new(&region, 4, &replace_target) == 0,
			SUITE, CASE_NAME, "failed to allocate replace target");
	GENERATED_TEST_ASSERT(jsval_array_push(&region, replace_target, jsval_number(1.0)) == 0
			&& jsval_array_push(&region, replace_target, jsval_number(2.0)) == 0
			&& jsval_array_push(&region, replace_target, jsval_number(3.0)) == 0
			&& jsval_array_push(&region, replace_target, jsval_number(4.0)) == 0,
			SUITE, CASE_NAME, "failed to initialize replace target");
	GENERATED_TEST_ASSERT(jsval_array_splice_dense(&region, replace_target, 1, 2,
			replace_values, 1, &replace_removed) == 0, SUITE, CASE_NAME,
			"failed to perform dense replacement splice");

	GENERATED_TEST_ASSERT(jsval_object_set_utf8(&region, output,
			(const uint8_t *)"insertTarget", 12, insert_target) == 0
			&& jsval_object_set_utf8(&region, output,
			(const uint8_t *)"insertRemoved", 13, insert_removed) == 0
			&& jsval_object_set_utf8(&region, output,
			(const uint8_t *)"replaceTarget", 13, replace_target) == 0
			&& jsval_object_set_utf8(&region, output,
			(const uint8_t *)"replaceRemoved", 14, replace_removed) == 0,
			SUITE, CASE_NAME, "failed to build splice output object");
	GENERATED_TEST_ASSERT(expect_json(&region, output, expected_json)
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME,
			"expected dense splice to support both insertion and replacement with removed results");

	return generated_test_pass(SUITE, CASE_NAME);
}
