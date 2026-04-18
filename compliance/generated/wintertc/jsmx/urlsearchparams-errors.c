#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "wintertc"
#define CASE_NAME "jsmx/urlsearchparams-errors"

static int
make_string(jsval_region_t *region, const char *text, jsval_t *value_ptr)
{
	return jsval_string_new_utf8(region, (const uint8_t *)text, strlen(text),
			value_ptr);
}

int
main(void)
{
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t init;
	jsval_t params;
	jsval_t missing_name;
	jsval_t valid_name;
	jsval_t valid_value;
	jsval_t got;

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(
			make_string(&region, "a=1", &init) == 0 &&
			jsval_url_search_params_new(&region, init, &params) == 0,
			SUITE, CASE_NAME, "setup failed");

	/* get() on a missing name is not an error: returns null. */
	GENERATED_TEST_ASSERT(
			make_string(&region, "not_there", &missing_name) == 0,
			SUITE, CASE_NAME, "missing_name");
	GENERATED_TEST_ASSERT(
			jsval_url_search_params_get(&region, params, missing_name,
				&got) == 0,
			SUITE, CASE_NAME, "get('not_there') should not error");
	GENERATED_TEST_ASSERT(got.kind == JSVAL_KIND_NULL,
			SUITE, CASE_NAME, "expected null for missing name");

	/* Accessors called on a non-URLSearchParams value fail with
	 * EINVAL rather than silently returning undefined. */
	GENERATED_TEST_ASSERT(
			make_string(&region, "key", &valid_name) == 0,
			SUITE, CASE_NAME, "valid_name");
	GENERATED_TEST_ASSERT(
			jsval_url_search_params_get(&region, jsval_undefined(),
				valid_name, &got) < 0,
			SUITE, CASE_NAME,
			"expected get on non-URLSearchParams receiver to fail");
	GENERATED_TEST_ASSERT(
			jsval_url_search_params_size(&region, jsval_number(1.0),
				&got) < 0,
			SUITE, CASE_NAME,
			"expected size on non-URLSearchParams receiver to fail");

	(void)valid_value;

	return generated_test_pass(SUITE, CASE_NAME);
}
