#include <errno.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "wintertc"
#define CASE_NAME "jsmx/url-errors"

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
	jsval_t input;
	jsval_t url;

	jsval_region_init(&region, storage, sizeof(storage));

	/* Relative URL with no base fails. */
	GENERATED_TEST_ASSERT(
			make_string(&region, "/just/a/path", &input) == 0,
			SUITE, CASE_NAME, "failed to create relative input");
	errno = 0;
	GENERATED_TEST_ASSERT(
			jsval_url_new(&region, input, 0, jsval_undefined(),
				&url) < 0,
			SUITE, CASE_NAME,
			"expected jsval_url_new to fail on relative URL with no base");

	/* Invalid hostname (empty label) fails. */
	GENERATED_TEST_ASSERT(
			make_string(&region, "https://a..b/", &input) == 0,
			SUITE, CASE_NAME, "failed to create invalid-host input");
	errno = 0;
	GENERATED_TEST_ASSERT(
			jsval_url_new(&region, input, 0, jsval_undefined(),
				&url) < 0,
			SUITE, CASE_NAME,
			"expected jsval_url_new to fail on invalid hostname");

	return generated_test_pass(SUITE, CASE_NAME);
}
