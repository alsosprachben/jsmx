#include <errno.h>
#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "values"
#define CASE_NAME "jsmx/function-container-call-parity"

static int
generated_echo_function(jsval_region_t *region, size_t argc,
		const jsval_t *argv, jsval_t *result_ptr, jsmethod_error_t *error)
{
	(void)region;
	(void)error;
	if (result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	*result_ptr = (argc > 0 && argv != NULL) ? argv[0] : jsval_undefined();
	return 0;
}

static int
expect_string(jsval_region_t *region, jsval_t value,
		const uint8_t *expected, size_t expected_len, const char *label)
{
	size_t actual_len = 0;
	uint8_t buf[expected_len ? expected_len : 1];

	if (jsval_string_copy_utf8(region, value, NULL, 0, &actual_len) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: failed to measure string result", label);
	}
	if (actual_len != expected_len) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: expected %zu bytes, got %zu",
				label, expected_len, actual_len);
	}
	if (jsval_string_copy_utf8(region, value, buf, actual_len, NULL) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: failed to copy string result", label);
	}
	if (memcmp(buf, expected, expected_len) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: string result did not match expected output", label);
	}
	return GENERATED_TEST_PASS;
}

int
main(void)
{
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t name;
	jsval_t fn;
	jsval_t object;
	jsval_t array;
	jsval_t set;
	jsval_t map;
	jsval_t value;
	jsval_t args[1];
	int has = 0;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"echo", 4, &name) == 0,
			SUITE, CASE_NAME, "failed to allocate function name");
	GENERATED_TEST_ASSERT(jsval_function_new(&region, generated_echo_function, 1,
			1, name, &fn) == 0,
			SUITE, CASE_NAME, "failed to allocate function value");
	GENERATED_TEST_ASSERT(jsval_object_new(&region, 1, &object) == 0,
			SUITE, CASE_NAME, "failed to allocate object");
	GENERATED_TEST_ASSERT(jsval_array_new(&region, 1, &array) == 0,
			SUITE, CASE_NAME, "failed to allocate array");
	GENERATED_TEST_ASSERT(jsval_set_new(&region, 1, &set) == 0,
			SUITE, CASE_NAME, "failed to allocate set");
	GENERATED_TEST_ASSERT(jsval_map_new(&region, 1, &map) == 0,
			SUITE, CASE_NAME, "failed to allocate map");

	GENERATED_TEST_ASSERT(jsval_object_set_utf8(&region, object,
			(const uint8_t *)"fn", 2, fn) == 0,
			SUITE, CASE_NAME, "failed to store function in object");
	GENERATED_TEST_ASSERT(jsval_array_push(&region, array, fn) == 0,
			SUITE, CASE_NAME, "failed to store function in array");
	GENERATED_TEST_ASSERT(jsval_set_add(&region, set, fn) == 0,
			SUITE, CASE_NAME, "failed to store function in set");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"stored", 6, &value) == 0,
			SUITE, CASE_NAME, "failed to allocate map payload");
	GENERATED_TEST_ASSERT(jsval_map_set(&region, map, fn, value) == 0,
			SUITE, CASE_NAME, "failed to store function in map");

	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, object,
			(const uint8_t *)"fn", 2, &value) == 0
			&& jsval_strict_eq(&region, value, fn) == 1,
			SUITE, CASE_NAME,
			"expected object-stored function identity to survive");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, array, 0, &value) == 0
			&& jsval_strict_eq(&region, value, fn) == 1,
			SUITE, CASE_NAME,
			"expected array-stored function identity to survive");
	GENERATED_TEST_ASSERT(jsval_set_has(&region, set, fn, &has) == 0
			&& has == 1, SUITE, CASE_NAME,
			"expected set-stored function identity to survive");
	GENERATED_TEST_ASSERT(jsval_map_get(&region, map, fn, &value) == 0,
			SUITE, CASE_NAME, "failed to read function-keyed map entry");
	GENERATED_TEST_ASSERT(expect_string(&region, value,
			(const uint8_t *)"stored", 6, "map.get(fn)")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected function-keyed map payload");

	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"ok", 2, &args[0]) == 0,
			SUITE, CASE_NAME, "failed to allocate string call argument");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, object,
			(const uint8_t *)"fn", 2, &value) == 0,
			SUITE, CASE_NAME, "failed to reload function from object");
	GENERATED_TEST_ASSERT(jsval_function_call(&region, value, 1, args,
			&value, NULL) == 0, SUITE, CASE_NAME,
			"failed to call object-stored function");
	GENERATED_TEST_ASSERT(expect_string(&region, value,
			(const uint8_t *)"ok", 2, "box.fn(\"ok\")")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected object-stored function result");

	args[0] = jsval_number(7.0);
	GENERATED_TEST_ASSERT(jsval_array_get(&region, array, 0, &value) == 0,
			SUITE, CASE_NAME, "failed to reload function from array");
	GENERATED_TEST_ASSERT(jsval_function_call(&region, value, 1, args,
			&value, NULL) == 0, SUITE, CASE_NAME,
			"failed to call array-stored function");
	GENERATED_TEST_ASSERT(value.kind == JSVAL_KIND_NUMBER
			&& value.as.number == 7.0, SUITE, CASE_NAME,
			"expected array-stored function to echo numeric input");

	return generated_test_pass(SUITE, CASE_NAME);
}
