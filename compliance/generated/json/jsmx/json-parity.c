#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "json"
#define CASE_NAME "jsmx/json-parity"

static int
roundtrip_bytes(jsval_region_t *region, jsval_t value, const char *expected,
		const char *label)
{
	size_t len = 0;
	size_t expected_len = strlen(expected);
	uint8_t buf[128];

	if (jsval_copy_json(region, value, NULL, 0, &len) != 0 ||
			len != expected_len || len >= sizeof(buf)) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: measure (len %zu vs %zu)", label, len,
				expected_len);
	}
	if (jsval_copy_json(region, value, buf, len, NULL) != 0 ||
			memcmp(buf, expected, len) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: bytes mismatch", label);
	}
	return GENERATED_TEST_PASS;
}

int
main(void)
{
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t parsed;
	jsval_t leaf;
	jsval_t elem;

	jsval_region_init(&region, storage, sizeof(storage));

	/* Empty object. */
	GENERATED_TEST_ASSERT(
			jsval_json_parse(&region, (const uint8_t *)"{}", 2, 8,
				&parsed) == 0,
			SUITE, CASE_NAME, "parse {} failed");
	GENERATED_TEST_ASSERT(parsed.kind == JSVAL_KIND_OBJECT,
			SUITE, CASE_NAME, "expected OBJECT from {}");
	GENERATED_TEST_ASSERT(
			roundtrip_bytes(&region, parsed, "{}", "empty object")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "empty object round-trip");

	/* Empty array. */
	GENERATED_TEST_ASSERT(
			jsval_json_parse(&region, (const uint8_t *)"[]", 2, 8,
				&parsed) == 0,
			SUITE, CASE_NAME, "parse [] failed");
	GENERATED_TEST_ASSERT(parsed.kind == JSVAL_KIND_ARRAY,
			SUITE, CASE_NAME, "expected ARRAY from []");
	GENERATED_TEST_ASSERT(
			roundtrip_bytes(&region, parsed, "[]", "empty array")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "empty array round-trip");

	/* Nested mixed structure. */
	{
		const char *source = "{\"a\":1,\"b\":[2,true,null]}";
		size_t source_len = strlen(source);

		GENERATED_TEST_ASSERT(
				jsval_json_parse(&region, (const uint8_t *)source,
					source_len, 16, &parsed) == 0,
				SUITE, CASE_NAME, "parse nested failed");
		GENERATED_TEST_ASSERT(parsed.kind == JSVAL_KIND_OBJECT,
				SUITE, CASE_NAME, "nested root not OBJECT");

		/* Accessor checks. JSON-backed values are lazy — use
		 * jsval_to_number / jsval_to_bool to extract. */
		{
			double num;
			int boolean;

			GENERATED_TEST_ASSERT(
					jsval_object_get_utf8(&region, parsed,
						(const uint8_t *)"a", 1, &leaf) == 0 &&
					leaf.kind == JSVAL_KIND_NUMBER &&
					jsval_to_number(&region, leaf, &num) == 0 &&
					num == 1.0,
					SUITE, CASE_NAME, "a != 1");

			GENERATED_TEST_ASSERT(
					jsval_object_get_utf8(&region, parsed,
						(const uint8_t *)"b", 1, &leaf) == 0 &&
					leaf.kind == JSVAL_KIND_ARRAY,
					SUITE, CASE_NAME, "b not array");
			GENERATED_TEST_ASSERT(
					jsval_array_length(&region, leaf) == 3,
					SUITE, CASE_NAME, "b length != 3");
			GENERATED_TEST_ASSERT(
					jsval_array_get(&region, leaf, 0, &elem) == 0 &&
					elem.kind == JSVAL_KIND_NUMBER &&
					jsval_to_number(&region, elem, &num) == 0 &&
					num == 2.0,
					SUITE, CASE_NAME, "b[0] != 2");
			GENERATED_TEST_ASSERT(
					jsval_array_get(&region, leaf, 1, &elem) == 0 &&
					elem.kind == JSVAL_KIND_BOOL,
					SUITE, CASE_NAME, "b[1] not bool");
			boolean = 0;
			GENERATED_TEST_ASSERT(
					jsval_to_int32(&region, elem, &boolean) == 0 &&
					boolean != 0,
					SUITE, CASE_NAME, "b[1] != true");
			GENERATED_TEST_ASSERT(
					jsval_array_get(&region, leaf, 2, &elem) == 0 &&
					elem.kind == JSVAL_KIND_NULL,
					SUITE, CASE_NAME, "b[2] != null");
		}

		/* Round-trip to canonical form. */
		GENERATED_TEST_ASSERT(
				roundtrip_bytes(&region, parsed, source,
					"nested") == GENERATED_TEST_PASS,
				SUITE, CASE_NAME, "nested round-trip");
	}

	return generated_test_pass(SUITE, CASE_NAME);
}
