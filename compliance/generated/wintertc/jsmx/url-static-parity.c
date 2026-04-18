#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "wintertc"
#define CASE_NAME "jsmx/url-static-parity"

static int
make_string(jsval_region_t *region, const char *text, jsval_t *value_ptr)
{
	return jsval_string_new_utf8(region, (const uint8_t *)text, strlen(text),
			value_ptr);
}

static int
expect_href(jsval_region_t *region, jsval_t url_value, const char *expected)
{
	jsval_t href_value;
	size_t len = 0;
	size_t expected_len = strlen(expected);
	uint8_t buf[128];

	if (url_value.kind != JSVAL_KIND_URL) {
		return generated_test_fail(SUITE, CASE_NAME,
				"expected URL kind, got %d", (int)url_value.kind);
	}
	if (jsval_url_href(region, url_value, &href_value) != 0 ||
			jsval_string_copy_utf8(region, href_value, NULL, 0,
				&len) != 0 ||
			len != expected_len || len >= sizeof(buf)) {
		return generated_test_fail(SUITE, CASE_NAME,
				"href length mismatch (want %zu)", expected_len);
	}
	if (jsval_string_copy_utf8(region, href_value, buf, len, NULL) != 0
			|| memcmp(buf, expected, len) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"href bytes mismatch (want %s)", expected);
	}
	return GENERATED_TEST_PASS;
}

int
main(void)
{
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t input;
	jsval_t base;
	jsval_t ok_bool, ok_url;
	jsval_t bad_bool, bad_url;
	jsval_t rel_bool, rel_url;
	jsval_t inv_bool, inv_url;

	jsval_region_init(&region, storage, sizeof(storage));

	/* Happy path: valid absolute URL. */
	GENERATED_TEST_ASSERT(
			make_string(&region, "https://example.com/", &input) == 0,
			SUITE, CASE_NAME, "build absolute input");
	GENERATED_TEST_ASSERT(
			jsval_url_can_parse(&region, input, 0, jsval_undefined(),
				&ok_bool) == 0 &&
			ok_bool.kind == JSVAL_KIND_BOOL &&
			ok_bool.as.boolean != 0,
			SUITE, CASE_NAME, "canParse valid → true");
	GENERATED_TEST_ASSERT(
			jsval_url_parse(&region, input, 0, jsval_undefined(),
				&ok_url) == 0,
			SUITE, CASE_NAME, "parse valid did not fail");
	GENERATED_TEST_ASSERT(
			expect_href(&region, ok_url, "https://example.com/")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "parse valid href");

	/* Relative URL with no base: false / null. */
	GENERATED_TEST_ASSERT(
			make_string(&region, "/just/a/path", &input) == 0,
			SUITE, CASE_NAME, "build relative-no-base input");
	GENERATED_TEST_ASSERT(
			jsval_url_can_parse(&region, input, 0, jsval_undefined(),
				&bad_bool) == 0 &&
			bad_bool.kind == JSVAL_KIND_BOOL &&
			bad_bool.as.boolean == 0,
			SUITE, CASE_NAME, "canParse relative-no-base → false");
	GENERATED_TEST_ASSERT(
			jsval_url_parse(&region, input, 0, jsval_undefined(),
				&bad_url) == 0 &&
			bad_url.kind == JSVAL_KIND_NULL,
			SUITE, CASE_NAME, "parse relative-no-base → null");

	/* Relative URL with a base: true / resolved URL. */
	GENERATED_TEST_ASSERT(
			make_string(&region, "/foo", &input) == 0 &&
			make_string(&region, "https://example.com/", &base) == 0,
			SUITE, CASE_NAME, "build relative-with-base inputs");
	GENERATED_TEST_ASSERT(
			jsval_url_can_parse(&region, input, 1, base, &rel_bool) == 0 &&
			rel_bool.kind == JSVAL_KIND_BOOL &&
			rel_bool.as.boolean != 0,
			SUITE, CASE_NAME, "canParse relative-with-base → true");
	GENERATED_TEST_ASSERT(
			jsval_url_parse(&region, input, 1, base, &rel_url) == 0,
			SUITE, CASE_NAME, "parse relative-with-base did not fail");
	GENERATED_TEST_ASSERT(
			expect_href(&region, rel_url, "https://example.com/foo")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "parse relative-with-base href");

	/* Invalid hostname: false / null. */
	GENERATED_TEST_ASSERT(
			make_string(&region, "https://a..b/", &input) == 0,
			SUITE, CASE_NAME, "build invalid-host input");
	GENERATED_TEST_ASSERT(
			jsval_url_can_parse(&region, input, 0, jsval_undefined(),
				&inv_bool) == 0 &&
			inv_bool.kind == JSVAL_KIND_BOOL &&
			inv_bool.as.boolean == 0,
			SUITE, CASE_NAME, "canParse invalid-host → false");
	GENERATED_TEST_ASSERT(
			jsval_url_parse(&region, input, 0, jsval_undefined(),
				&inv_url) == 0 &&
			inv_url.kind == JSVAL_KIND_NULL,
			SUITE, CASE_NAME, "parse invalid-host → null");

	return generated_test_pass(SUITE, CASE_NAME);
}
