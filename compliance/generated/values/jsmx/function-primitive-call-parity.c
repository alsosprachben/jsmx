#include <errno.h>
#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "values"
#define CASE_NAME "jsmx/function-primitive-call-parity"

static int
generated_sum_function(jsval_region_t *region, size_t argc,
		const jsval_t *argv, jsval_t *result_ptr, jsmethod_error_t *error)
{
	(void)error;
	if (region == NULL || result_ptr == NULL || argc < 2 || argv == NULL) {
		errno = EINVAL;
		return -1;
	}
	return jsval_add(region, argv[0], argv[1], result_ptr);
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
	jsval_t named;
	jsval_t alias;
	jsval_t other;
	jsval_t result;
	jsval_t args[2];
	int equal = 0;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"sum", 3, &name) == 0,
			SUITE, CASE_NAME, "failed to allocate function name");
	GENERATED_TEST_ASSERT(jsval_function_new(&region, generated_sum_function, 2,
			1, name, &named) == 0,
			SUITE, CASE_NAME, "failed to allocate named function");
	alias = named;
	GENERATED_TEST_ASSERT(jsval_function_new(&region, generated_sum_function, 2,
			1, name, &other) == 0,
			SUITE, CASE_NAME, "failed to allocate distinct function");

	GENERATED_TEST_ASSERT(jsval_typeof(&region, named, &result) == 0,
			SUITE, CASE_NAME, "failed to typeof function");
	GENERATED_TEST_ASSERT(expect_string(&region, result,
			(const uint8_t *)"function", 8, "typeof function")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected typeof function result");
	GENERATED_TEST_ASSERT(jsval_truthy(&region, named) == 1,
			SUITE, CASE_NAME, "expected function to be truthy");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, named, alias) == 1,
			SUITE, CASE_NAME,
			"expected alias to preserve function identity");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, named, other) == 0,
			SUITE, CASE_NAME,
			"expected distinct allocations to stay distinct");
	GENERATED_TEST_ASSERT(jsval_abstract_eq(&region, named, alias, &equal) == 0
			&& equal == 1, SUITE, CASE_NAME,
			"expected alias to stay abstract-equal");
	GENERATED_TEST_ASSERT(jsval_abstract_eq(&region, named, other, &equal) == 0
			&& equal == 0, SUITE, CASE_NAME,
			"expected distinct function allocations to stay abstract-inequal");

	GENERATED_TEST_ASSERT(jsval_function_name(&region, named, &result) == 0,
			SUITE, CASE_NAME, "failed to read function name");
	GENERATED_TEST_ASSERT(expect_string(&region, result,
			(const uint8_t *)"sum", 3, "function.name")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected function name");
	GENERATED_TEST_ASSERT(jsval_function_length(&region, named, &result) == 0,
			SUITE, CASE_NAME, "failed to read function length");
	GENERATED_TEST_ASSERT(result.kind == JSVAL_KIND_NUMBER
			&& result.as.number == 2.0, SUITE, CASE_NAME,
			"expected function length to stay 2");

	args[0] = jsval_number(20.0);
	args[1] = jsval_number(22.0);
	GENERATED_TEST_ASSERT(jsval_function_call(&region, named, 2, args,
			&result, NULL) == 0, SUITE, CASE_NAME,
			"failed to call named function");
	GENERATED_TEST_ASSERT(result.kind == JSVAL_KIND_NUMBER
			&& result.as.number == 42.0, SUITE, CASE_NAME,
			"expected function call to return 42");

	return generated_test_pass(SUITE, CASE_NAME);
}
