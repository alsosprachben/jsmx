#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "values"
#define CASE_NAME "jsmx/ternary-container-identity-parity"

int
main(void)
{
	static const uint8_t input[] = "{\"obj\":{},\"arr\":[]}";
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t root;
	jsval_t obj;
	jsval_t arr;
	jsval_t native_obj;
	jsval_t other_obj;
	jsval_t native_arr;
	jsval_t other_arr;
	jsval_t result;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_json_parse(&region, input, sizeof(input) - 1, 12,
			&root) == 0, SUITE, CASE_NAME,
			"failed to parse input JSON into a jsval region");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"obj", 3, &obj) == 0,
			SUITE, CASE_NAME, "failed to read parsed object");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"arr", 3, &arr) == 0,
			SUITE, CASE_NAME, "failed to read parsed array");
	GENERATED_TEST_ASSERT(jsval_object_new(&region, 0, &native_obj) == 0
			&& jsval_object_new(&region, 0, &other_obj) == 0
			&& jsval_array_new(&region, 0, &native_arr) == 0
			&& jsval_array_new(&region, 0, &other_arr) == 0,
			SUITE, CASE_NAME, "failed to allocate native containers");

	if (jsval_truthy(&region, obj)) {
		result = obj;
	} else {
		result = other_obj;
	}
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, result, obj) == 1, SUITE,
			CASE_NAME,
			"expected parsed object truthiness to preserve object identity");

	if (jsval_truthy(&region, arr)) {
		result = arr;
	} else {
		result = other_arr;
	}
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, result, arr) == 1, SUITE,
			CASE_NAME,
			"expected parsed array truthiness to preserve array identity");

	if (jsval_truthy(&region, native_obj)) {
		result = native_obj;
	} else {
		result = other_obj;
	}
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, result, native_obj) == 1,
			SUITE, CASE_NAME,
			"expected native object truthiness to preserve object identity");

	if (jsval_truthy(&region, native_arr)) {
		result = native_arr;
	} else {
		result = other_arr;
	}
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, result, native_arr) == 1,
			SUITE, CASE_NAME,
			"expected native array truthiness to preserve array identity");

	if (jsval_truthy(&region, jsval_undefined())) {
		result = native_obj;
	} else {
		result = other_obj;
	}
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, result, other_obj) == 1,
			SUITE, CASE_NAME,
			"expected undefined ternary to select the else object");

	return generated_test_pass(SUITE, CASE_NAME);
}
