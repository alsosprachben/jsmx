#include <errno.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "wintertc"
#define CASE_NAME "jsmx/fetch-headers-errors"

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
	jsval_t headers;
	jsval_t name;
	jsval_t value;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_headers_new(&region,
			JSVAL_HEADERS_GUARD_NONE, &headers) == 0, SUITE, CASE_NAME,
			"failed to construct Headers");

	errno = 0;
	GENERATED_TEST_ASSERT(make_string(&region, "bad name", &name) == 0
			&& make_string(&region, "v", &value) == 0
			&& jsval_headers_append(&region, headers, name, value) < 0,
			SUITE, CASE_NAME, "bad name should reject");

	errno = 0;
	GENERATED_TEST_ASSERT(make_string(&region, "X-Ok", &name) == 0
			&& make_string(&region, "has\nnewline", &value) == 0
			&& jsval_headers_append(&region, headers, name, value) < 0,
			SUITE, CASE_NAME, "newline in value should reject");

	errno = 0;
	GENERATED_TEST_ASSERT(make_string(&region, "", &name) == 0
			&& make_string(&region, "value", &value) == 0
			&& jsval_headers_append(&region, headers, name, value) < 0,
			SUITE, CASE_NAME, "empty name should reject");

	return generated_test_pass(SUITE, CASE_NAME);
}
