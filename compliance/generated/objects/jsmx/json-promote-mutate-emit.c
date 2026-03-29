#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "objects"
#define CASE_NAME "jsmx/json-promote-mutate-emit"

static int
expect_string(jsval_region_t *region, jsval_t value, const char *expected,
		const char *label)
{
	size_t expected_len = strlen(expected);
	size_t actual_len = 0;
	uint8_t buf[expected_len ? expected_len : 1];

	if (jsval_string_copy_utf8(region, value, NULL, 0, &actual_len) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: failed to measure string value", label);
	}
	if (actual_len != expected_len) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: expected %zu bytes, got %zu",
				label, expected_len, actual_len);
	}
	if (jsval_string_copy_utf8(region, value, buf, actual_len, NULL) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: failed to copy string value", label);
	}
	if (memcmp(buf, expected, expected_len) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: string value did not match expected output", label);
	}
	return GENERATED_TEST_PASS;
}

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
		"{\"profile\":{\"name\":\"Ada\",\"active\":true},\"scores\":[1,2,3],\"note\":\"hi\"}";
	static const char expected[] =
		"{\"profile\":{\"name\":\"Ada\",\"active\":false},\"scores\":[7,2,3],\"note\":\"updated\"}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t profile;
	jsval_t name;
	jsval_t active;
	jsval_t scores;
	jsval_t second_score;
	jsval_t updated_note;

	/*
	 * Generated from repo-authored source:
	 * compliance/js/objects/jsmx/json-promote-mutate-emit.js
	 *
	 * This fixture demonstrates the intended flattened lowering pattern:
	 * JSON-backed reads stay token-backed, semantic mutation fails until the
	 * root is promoted, and native writes then emit the updated JSON document.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_json_parse(&region, input, sizeof(input) - 1, 32,
			&root) == 0, SUITE, CASE_NAME,
			"failed to parse input JSON into a jsval region");
	GENERATED_TEST_ASSERT(jsval_is_json_backed(root) == 1, SUITE, CASE_NAME,
			"expected parsed root to remain JSON-backed before mutation");

	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"profile", 7, &profile) == 0,
			SUITE, CASE_NAME, "failed to read nested profile object");
	GENERATED_TEST_ASSERT(jsval_is_json_backed(profile) == 1, SUITE, CASE_NAME,
			"expected nested profile object to stay JSON-backed before promotion");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, profile,
			(const uint8_t *)"name", 4, &name) == 0,
			SUITE, CASE_NAME, "failed to read nested profile.name");
	GENERATED_TEST_ASSERT(expect_string(&region, name, "Ada",
			"profile.name before promotion") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected profile.name before promotion");

	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"scores", 6, &scores) == 0,
			SUITE, CASE_NAME, "failed to read scores array");
	GENERATED_TEST_ASSERT(jsval_is_json_backed(scores) == 1, SUITE, CASE_NAME,
			"expected scores array to stay JSON-backed before promotion");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, scores, 1, &second_score) == 0,
			SUITE, CASE_NAME, "failed to read scores[1]");
	GENERATED_TEST_ASSERT(second_score.kind == JSVAL_KIND_NUMBER
			&& jsval_strict_eq(&region, second_score, jsval_number(2.0)) == 1,
			SUITE, CASE_NAME, "expected scores[1] to stay numeric before promotion");

	errno = 0;
	GENERATED_TEST_ASSERT(jsval_object_set_utf8(&region, root,
			(const uint8_t *)"note", 4, jsval_null()) == -1,
			SUITE, CASE_NAME,
			"expected JSON-backed object mutation to fail before promotion");
	GENERATED_TEST_ASSERT(errno == ENOTSUP, SUITE, CASE_NAME,
			"expected ENOTSUP from JSON-backed object mutation");

	errno = 0;
	GENERATED_TEST_ASSERT(jsval_array_set(&region, scores, 0,
			jsval_number(7.0)) == -1,
			SUITE, CASE_NAME,
			"expected JSON-backed array mutation to fail before promotion");
	GENERATED_TEST_ASSERT(errno == ENOTSUP, SUITE, CASE_NAME,
			"expected ENOTSUP from JSON-backed array mutation");

	GENERATED_TEST_ASSERT(jsval_region_promote_root(&region, &root) == 0,
			SUITE, CASE_NAME, "failed to promote root for native mutation");
	GENERATED_TEST_ASSERT(jsval_is_native(root) == 1, SUITE, CASE_NAME,
			"expected promoted root to be native");

	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"profile", 7, &profile) == 0,
			SUITE, CASE_NAME, "failed to re-read profile after promotion");
	GENERATED_TEST_ASSERT(jsval_is_native(profile) == 1, SUITE, CASE_NAME,
			"expected promoted child object to be native");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"scores", 6, &scores) == 0,
			SUITE, CASE_NAME, "failed to re-read scores after promotion");
	GENERATED_TEST_ASSERT(jsval_is_native(scores) == 1, SUITE, CASE_NAME,
			"expected promoted array to be native");

	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, profile,
			(const uint8_t *)"active", 6, &active) == 0,
			SUITE, CASE_NAME, "failed to read profile.active after promotion");
	GENERATED_TEST_ASSERT(active.kind == JSVAL_KIND_BOOL && active.as.boolean == 1,
			SUITE, CASE_NAME, "expected profile.active to start as true");

	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"updated", 7, &updated_note) == 0,
			SUITE, CASE_NAME, "failed to construct updated note string");
	GENERATED_TEST_ASSERT(jsval_object_set_utf8(&region, profile,
			(const uint8_t *)"active", 6, jsval_bool(0)) == 0,
			SUITE, CASE_NAME, "failed to update nested active flag");
	GENERATED_TEST_ASSERT(jsval_array_set(&region, scores, 0,
			jsval_number(7.0)) == 0, SUITE, CASE_NAME,
			"failed to update first score after promotion");
	GENERATED_TEST_ASSERT(jsval_object_set_utf8(&region, root,
			(const uint8_t *)"note", 4, updated_note) == 0,
			SUITE, CASE_NAME, "failed to update note after promotion");

	GENERATED_TEST_ASSERT(expect_json(&region, root, expected) == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected emitted JSON after native mutation");

	return generated_test_pass(SUITE, CASE_NAME);
}
