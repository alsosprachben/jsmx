#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "values"
#define CASE_NAME "jsmx/container-identity-truthiness"

int
main(void)
{
	static const uint8_t input[] = "{\"obj\":{},\"obj2\":{},\"arr\":[],\"arr2\":[]}";
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t root;
	jsval_t obj;
	jsval_t obj_again;
	jsval_t obj2;
	jsval_t arr;
	jsval_t arr_again;
	jsval_t arr2;
	jsval_t native_obj;
	jsval_t native_obj_other;
	jsval_t native_arr;
	jsval_t native_arr_other;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_json_parse(&region, input, sizeof(input) - 1, 24,
			&root) == 0, SUITE, CASE_NAME,
			"failed to parse input JSON into a jsval region");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"obj", 3, &obj) == 0
			&& jsval_object_get_utf8(&region, root,
			(const uint8_t *)"obj", 3, &obj_again) == 0
			&& jsval_object_get_utf8(&region, root,
			(const uint8_t *)"obj2", 4, &obj2) == 0,
			SUITE, CASE_NAME, "failed to read parsed object operands");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"arr", 3, &arr) == 0
			&& jsval_object_get_utf8(&region, root,
			(const uint8_t *)"arr", 3, &arr_again) == 0
			&& jsval_object_get_utf8(&region, root,
			(const uint8_t *)"arr2", 4, &arr2) == 0,
			SUITE, CASE_NAME, "failed to read parsed array operands");
	GENERATED_TEST_ASSERT(jsval_object_new(&region, 0, &native_obj) == 0
			&& jsval_object_new(&region, 0, &native_obj_other) == 0
			&& jsval_array_new(&region, 0, &native_arr) == 0
			&& jsval_array_new(&region, 0, &native_arr_other) == 0,
			SUITE, CASE_NAME, "failed to allocate native container operands");

	GENERATED_TEST_ASSERT(jsval_truthy(&region, native_obj) == 1
			&& jsval_truthy(&region, native_arr) == 1
			&& jsval_truthy(&region, obj) == 1
			&& jsval_truthy(&region, arr) == 1,
			SUITE, CASE_NAME, "expected object and array values to be truthy");

	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, native_obj, native_obj) == 1
			&& jsval_strict_eq(&region, native_obj, native_obj_other) == 0,
			SUITE, CASE_NAME, "unexpected native object identity result");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, native_arr, native_arr) == 1
			&& jsval_strict_eq(&region, native_arr, native_arr_other) == 0,
			SUITE, CASE_NAME, "unexpected native array identity result");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, obj, obj_again) == 1
			&& jsval_strict_eq(&region, obj, obj2) == 0,
			SUITE, CASE_NAME, "unexpected parsed object identity result");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, arr, arr_again) == 1
			&& jsval_strict_eq(&region, arr, arr2) == 0,
			SUITE, CASE_NAME, "unexpected parsed array identity result");

	return generated_test_pass(SUITE, CASE_NAME);
}
