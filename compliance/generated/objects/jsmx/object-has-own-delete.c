#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "objects"
#define CASE_NAME "jsmx/object-has-own-delete"

int
main(void)
{
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t object;
	jsval_t got;
	int has;
	int deleted;

	/*
	 * Generated from repo-authored source:
	 * compliance/js/objects/jsmx/object-has-own-delete.js
	 *
	 * This idiomatic flattened translation exercises native own-property
	 * checks and deletion through the explicit jsval object helpers.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_object_new(&region, 2, &object) == 0,
			SUITE, CASE_NAME, "failed to allocate native object");
	GENERATED_TEST_ASSERT(jsval_object_set_utf8(&region, object,
			(const uint8_t *)"keep", 4, jsval_bool(1)) == 0,
			SUITE, CASE_NAME, "failed to set keep property");
	GENERATED_TEST_ASSERT(jsval_object_set_utf8(&region, object,
			(const uint8_t *)"drop", 4, jsval_number(7.0)) == 0,
			SUITE, CASE_NAME, "failed to set drop property");

	GENERATED_TEST_ASSERT(jsval_object_has_own_utf8(&region, object,
			(const uint8_t *)"keep", 4, &has) == 0,
			SUITE, CASE_NAME, "failed own-property check for keep");
	GENERATED_TEST_ASSERT(has == 1, SUITE, CASE_NAME,
			"expected keep to be an own property");
	GENERATED_TEST_ASSERT(jsval_object_has_own_utf8(&region, object,
			(const uint8_t *)"drop", 4, &has) == 0,
			SUITE, CASE_NAME, "failed own-property check for drop");
	GENERATED_TEST_ASSERT(has == 1, SUITE, CASE_NAME,
			"expected drop to be an own property");
	GENERATED_TEST_ASSERT(jsval_object_has_own_utf8(&region, object,
			(const uint8_t *)"missing", 7, &has) == 0,
			SUITE, CASE_NAME, "failed own-property check for missing");
	GENERATED_TEST_ASSERT(has == 0, SUITE, CASE_NAME,
			"missing should not be reported as an own property");

	GENERATED_TEST_ASSERT(jsval_object_delete_utf8(&region, object,
			(const uint8_t *)"drop", 4, &deleted) == 0,
			SUITE, CASE_NAME, "failed to delete drop property");
	GENERATED_TEST_ASSERT(deleted == 1, SUITE, CASE_NAME,
			"expected drop deletion to report success");
	GENERATED_TEST_ASSERT(jsval_object_has_own_utf8(&region, object,
			(const uint8_t *)"drop", 4, &has) == 0,
			SUITE, CASE_NAME, "failed own-property check after delete");
	GENERATED_TEST_ASSERT(has == 0, SUITE, CASE_NAME,
			"expected deleted property to disappear from own-property checks");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, object,
			(const uint8_t *)"drop", 4, &got) == 0,
			SUITE, CASE_NAME, "failed readback after delete");
	GENERATED_TEST_ASSERT(got.kind == JSVAL_KIND_UNDEFINED, SUITE, CASE_NAME,
			"expected deleted property to read back as undefined");
	{
		static const uint8_t expected[] = "{\"keep\":true}";
		size_t actual_len = 0;
		uint8_t actual[sizeof(expected) - 1];

		GENERATED_TEST_ASSERT(jsval_copy_json(&region, object, NULL, 0,
				&actual_len) == 0, SUITE, CASE_NAME,
				"failed to measure emitted JSON");
		GENERATED_TEST_ASSERT(actual_len == sizeof(expected) - 1, SUITE, CASE_NAME,
				"expected emitted JSON length %zu, got %zu",
				sizeof(expected) - 1, actual_len);
		GENERATED_TEST_ASSERT(jsval_copy_json(&region, object, actual, actual_len,
				NULL) == 0, SUITE, CASE_NAME,
				"failed to copy emitted JSON");
		GENERATED_TEST_ASSERT(memcmp(actual, expected, sizeof(expected) - 1) == 0,
				SUITE, CASE_NAME,
				"expected deletion to be reflected in emitted JSON");
	}
	return generated_test_pass(SUITE, CASE_NAME);
}
