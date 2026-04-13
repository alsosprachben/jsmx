#include <errno.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "wintertc"
#define CASE_NAME "jsmx/fetch-request-errors"

static int
make_string(jsval_region_t *region, const char *text, jsval_t *value_ptr)
{
	return jsval_string_new_utf8(region, (const uint8_t *)text, strlen(text),
			value_ptr);
}

static int
try_forbidden_method(jsval_region_t *region, const char *method_name)
{
	jsval_t init;
	jsval_t method_val;
	jsval_t url;
	jsval_t request;

	if (jsval_object_new(region, 2, &init) < 0) { return -1; }
	if (make_string(region, method_name, &method_val) < 0) { return -1; }
	if (jsval_object_set_utf8(region, init, (const uint8_t *)"method", 6,
			method_val) < 0) {
		return -1;
	}
	if (make_string(region, "https://example.com", &url) < 0) { return -1; }
	errno = 0;
	return jsval_request_new(region, url, init, 1, &request);
}

int
main(void)
{
	uint8_t storage[65536];
	jsval_region_t region;

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(try_forbidden_method(&region, "CONNECT") < 0,
			SUITE, CASE_NAME, "CONNECT should reject");
	GENERATED_TEST_ASSERT(try_forbidden_method(&region, "TRACE") < 0,
			SUITE, CASE_NAME, "TRACE should reject");
	GENERATED_TEST_ASSERT(try_forbidden_method(&region, "TRACK") < 0,
			SUITE, CASE_NAME, "TRACK should reject");

	return generated_test_pass(SUITE, CASE_NAME);
}
