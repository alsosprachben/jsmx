#include <errno.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "wintertc"
#define CASE_NAME "jsmx/fetch-response-errors"

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
	jsval_t url;
	jsval_t response;
	jsval_t body;
	jsval_t init;

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(make_string(&region, "https://example.com",
			&url) == 0, SUITE, CASE_NAME, "url");

	errno = 0;
	GENERATED_TEST_ASSERT(jsval_response_redirect(&region, url, 1, 200,
			&response) < 0, SUITE, CASE_NAME,
			"Response.redirect(200) should reject");

	errno = 0;
	GENERATED_TEST_ASSERT(jsval_response_redirect(&region, url, 1, 404,
			&response) < 0, SUITE, CASE_NAME,
			"Response.redirect(404) should reject");

	GENERATED_TEST_ASSERT(jsval_response_redirect(&region, url, 1, 302,
			&response) == 0, SUITE, CASE_NAME,
			"Response.redirect(302) should accept");

	GENERATED_TEST_ASSERT(make_string(&region, "body", &body) == 0
			&& jsval_object_new(&region, 2, &init) == 0
			&& jsval_object_set_utf8(&region, init,
				(const uint8_t *)"status", 6, jsval_number(99.0)) == 0,
			SUITE, CASE_NAME, "init with bad status");
	errno = 0;
	GENERATED_TEST_ASSERT(jsval_response_new(&region, body, 1, init, 1,
			&response) < 0, SUITE, CASE_NAME,
			"new Response with status 99 should reject");

	return generated_test_pass(SUITE, CASE_NAME);
}
