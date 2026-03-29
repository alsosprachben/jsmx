#include <errno.h>
#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "objects"
#define CASE_NAME "jsmx/json-readonly-lookups"

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
		"{\"keep\":7,\"items\":[1,2],\"nested\":{\"flag\":true}}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t items;
	jsval_t got;
	int has;

	/*
	 * Generated from repo-authored source:
	 * compliance/js/objects/jsmx/json-readonly-lookups.js
	 *
	 * This idiomatic flattened translation proves the current JSON-backed
	 * lookup semantics directly and also records the lowering contract that
	 * semantic mutation still fails before promotion.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_json_parse(&region, input, sizeof(input) - 1, 24,
			&root) == 0, SUITE, CASE_NAME,
			"failed to parse input JSON into a jsval region");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"keep", 4, &got) == 0,
			SUITE, CASE_NAME, "failed to read keep before promotion");
	GENERATED_TEST_ASSERT(got.kind == JSVAL_KIND_NUMBER
			&& jsval_strict_eq(&region, got, jsval_number(7.0)) == 1,
			SUITE, CASE_NAME, "expected keep to read back as 7");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"missing", 7, &got) == 0,
			SUITE, CASE_NAME, "failed missing-property read");
	GENERATED_TEST_ASSERT(got.kind == JSVAL_KIND_UNDEFINED, SUITE, CASE_NAME,
			"expected missing property read to return undefined");
	GENERATED_TEST_ASSERT(jsval_object_has_own_utf8(&region, root,
			(const uint8_t *)"keep", 4, &has) == 0,
			SUITE, CASE_NAME, "failed own-property check for keep");
	GENERATED_TEST_ASSERT(has == 1, SUITE, CASE_NAME,
			"expected keep to be reported as an own property");
	GENERATED_TEST_ASSERT(jsval_object_has_own_utf8(&region, root,
			(const uint8_t *)"missing", 7, &has) == 0,
			SUITE, CASE_NAME, "failed own-property check for missing");
	GENERATED_TEST_ASSERT(has == 0, SUITE, CASE_NAME,
			"expected missing to stay absent before promotion");

	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"items", 5, &items) == 0,
			SUITE, CASE_NAME, "failed to read items before promotion");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, items, 4, &got) == 0,
			SUITE, CASE_NAME, "failed out-of-range array read");
	GENERATED_TEST_ASSERT(got.kind == JSVAL_KIND_UNDEFINED, SUITE, CASE_NAME,
			"expected out-of-range JSON-backed array read to return undefined");

	errno = 0;
	GENERATED_TEST_ASSERT(jsval_object_set_utf8(&region, root,
			(const uint8_t *)"keep", 4, jsval_number(9.0)) == -1,
			SUITE, CASE_NAME,
			"expected JSON-backed object overwrite to fail before promotion");
	GENERATED_TEST_ASSERT(errno == ENOTSUP, SUITE, CASE_NAME,
			"expected ENOTSUP from JSON-backed object overwrite");
	errno = 0;
	GENERATED_TEST_ASSERT(jsval_array_push(&region, items, jsval_number(3.0))
			== -1, SUITE, CASE_NAME,
			"expected JSON-backed array push to fail before promotion");
	GENERATED_TEST_ASSERT(errno == ENOTSUP, SUITE, CASE_NAME,
			"expected ENOTSUP from JSON-backed array push");

	GENERATED_TEST_ASSERT(expect_json(&region, root, expected)
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected emitted JSON for lookup-only fixture");

	return generated_test_pass(SUITE, CASE_NAME);
}
