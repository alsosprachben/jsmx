#include <errno.h>
#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "objects"
#define CASE_NAME "jsmx/array-splice-dense-capacity-error"

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
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t array;
	jsval_t removed;
	jsval_t insert_values[] = {
		jsval_number(7.0),
		jsval_number(8.0),
		jsval_number(9.0)
	};

	/*
	 * Generated from repo-authored source:
	 * compliance/js/objects/jsmx/array-splice-dense-capacity-error.js
	 *
	 * This idiomatic flattened translation exercises the explicit ENOBUFS
	 * capacity contract on jsval_array_splice_dense().
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_array_new(&region, 4, &array) == 0,
			SUITE, CASE_NAME, "failed to allocate dense array");
	GENERATED_TEST_ASSERT(jsval_array_push(&region, array, jsval_number(1.0)) == 0
			&& jsval_array_push(&region, array, jsval_number(2.0)) == 0
			&& jsval_array_push(&region, array, jsval_number(3.0)) == 0
			&& jsval_array_push(&region, array, jsval_number(4.0)) == 0,
			SUITE, CASE_NAME, "failed to initialize dense array");
	removed = jsval_null();
	errno = 0;
	GENERATED_TEST_ASSERT(jsval_array_splice_dense(&region, array, 1, 1,
			insert_values, 3, &removed) == -1, SUITE, CASE_NAME,
			"expected dense splice with undersized capacity to fail");
	GENERATED_TEST_ASSERT(errno == ENOBUFS, SUITE, CASE_NAME,
			"expected ENOBUFS from undersized dense splice");
	GENERATED_TEST_ASSERT(removed.kind == JSVAL_KIND_NULL, SUITE, CASE_NAME,
			"expected failed dense splice removed output to remain unchanged");
	GENERATED_TEST_ASSERT(expect_json(&region, array, "[1,2,3,4]")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME,
			"expected undersized dense splice to leave the target unchanged");

	return generated_test_pass(SUITE, CASE_NAME);
}
