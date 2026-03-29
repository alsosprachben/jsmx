#include <errno.h>
#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "objects"
#define CASE_NAME "jsmx/planned-capacity-errors"

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
		"{\"keep\":7,\"items\":[1,2],\"nested\":{\"flag\":true}}";
	static const char expected[] =
		"{\"keep\":9,\"items\":[1,8,3],\"nested\":{\"flag\":true}}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t items;
	size_t bytes;

	/*
	 * Generated from repo-authored source:
	 * compliance/js/objects/jsmx/planned-capacity-errors.js
	 *
	 * This idiomatic flattened translation records the mnvkd-style contract:
	 * undersized shallow-promotion plans fail explicitly, exact plans succeed,
	 * and dense native arrays still reject mutation past their planned
	 * capacity.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_json_parse(&region, input, sizeof(input) - 1, 24,
			&root) == 0, SUITE, CASE_NAME,
			"failed to parse input JSON into a jsval region");

	errno = 0;
	GENERATED_TEST_ASSERT(jsval_promote_object_shallow_measure(&region, root, 2,
			&bytes) == -1, SUITE, CASE_NAME,
			"expected undersized shallow object plan to fail");
	GENERATED_TEST_ASSERT(errno == ENOBUFS, SUITE, CASE_NAME,
			"expected ENOBUFS from undersized shallow object plan");
	GENERATED_TEST_ASSERT(jsval_promote_object_shallow_in_place(&region, &root, 3)
			== 0, SUITE, CASE_NAME,
			"failed to shallow-promote root with exact property capacity");
	GENERATED_TEST_ASSERT(jsval_region_set_root(&region, root) == 0,
			SUITE, CASE_NAME, "failed to persist promoted root");
	GENERATED_TEST_ASSERT(jsval_object_set_utf8(&region, root,
			(const uint8_t *)"keep", 4, jsval_number(9.0)) == 0,
			SUITE, CASE_NAME, "failed to overwrite keep after promotion");

	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"items", 5, &items) == 0,
			SUITE, CASE_NAME, "failed to read items after root promotion");
	errno = 0;
	GENERATED_TEST_ASSERT(jsval_promote_array_shallow_measure(&region, items, 1,
			&bytes) == -1, SUITE, CASE_NAME,
			"expected undersized shallow array plan to fail");
	GENERATED_TEST_ASSERT(errno == ENOBUFS, SUITE, CASE_NAME,
			"expected ENOBUFS from undersized shallow array plan");
	GENERATED_TEST_ASSERT(jsval_promote_array_shallow_in_place(&region, &items, 3)
			== 0, SUITE, CASE_NAME,
			"failed to shallow-promote child array with exact capacity");
	GENERATED_TEST_ASSERT(jsval_object_set_utf8(&region, root,
			(const uint8_t *)"items", 5, items) == 0,
			SUITE, CASE_NAME, "failed to write promoted child array back");

	GENERATED_TEST_ASSERT(jsval_array_set(&region, items, 1, jsval_number(8.0))
			== 0, SUITE, CASE_NAME, "failed to overwrite items[1]");
	GENERATED_TEST_ASSERT(jsval_array_push(&region, items, jsval_number(3.0)) == 0,
			SUITE, CASE_NAME, "failed to push final in-capacity value");

	errno = 0;
	GENERATED_TEST_ASSERT(jsval_array_push(&region, items, jsval_number(4.0))
			== -1, SUITE, CASE_NAME,
			"expected push past planned capacity to fail");
	GENERATED_TEST_ASSERT(errno == ENOBUFS, SUITE, CASE_NAME,
			"expected ENOBUFS from push past planned capacity");
	errno = 0;
	GENERATED_TEST_ASSERT(jsval_array_set_length(&region, items, 4) == -1,
			SUITE, CASE_NAME,
			"expected length past planned capacity to fail");
	GENERATED_TEST_ASSERT(errno == ENOBUFS, SUITE, CASE_NAME,
			"expected ENOBUFS from length past planned capacity");

	GENERATED_TEST_ASSERT(expect_json(&region, root, expected)
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected emitted JSON after exact planned-capacity execution");

	return generated_test_pass(SUITE, CASE_NAME);
}
