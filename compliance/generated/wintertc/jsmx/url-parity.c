#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "wintertc"
#define CASE_NAME "jsmx/url-parity"

static int
make_string(jsval_region_t *region, const char *text, jsval_t *value_ptr)
{
	return jsval_string_new_utf8(region, (const uint8_t *)text, strlen(text),
			value_ptr);
}

static int
expect_string(jsval_region_t *region, jsval_t value, const char *expected,
		const char *label)
{
	size_t len = 0;
	size_t expected_len = strlen(expected);
	uint8_t buf[expected_len ? expected_len : 1];

	if (value.kind != JSVAL_KIND_STRING) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: expected string kind", label);
	}
	if (jsval_string_copy_utf8(region, value, NULL, 0, &len) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: measure failed", label);
	}
	if (len != expected_len) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: len %zu != %zu", label, len, expected_len);
	}
	if (jsval_string_copy_utf8(region, value, buf, len, NULL) < 0
			|| memcmp(buf, expected, len) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: bytes mismatch", label);
	}
	return GENERATED_TEST_PASS;
}

int
main(void)
{
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t input;
	jsval_t url;
	jsval_t component;
	jsval_t new_pathname;

	jsval_region_init(&region, storage, sizeof(storage));

	/* Construct from a fully-specified URL including userinfo,
	 * explicit port, path, query, and fragment. */
	GENERATED_TEST_ASSERT(
			make_string(&region,
				"https://user:pass@example.com:8080/path?a=1#frag",
				&input) == 0,
			SUITE, CASE_NAME, "failed to create input string");
	GENERATED_TEST_ASSERT(
			jsval_url_new(&region, input, 0, jsval_undefined(),
				&url) == 0,
			SUITE, CASE_NAME, "jsval_url_new failed on absolute URL");
	GENERATED_TEST_ASSERT(url.kind == JSVAL_KIND_URL,
			SUITE, CASE_NAME, "expected URL kind");

	/* Component getters. */
	GENERATED_TEST_ASSERT(
			jsval_url_protocol(&region, url, &component) == 0 &&
			expect_string(&region, component, "https:", "protocol")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "protocol mismatch");
	GENERATED_TEST_ASSERT(
			jsval_url_username(&region, url, &component) == 0 &&
			expect_string(&region, component, "user", "username")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "username mismatch");
	GENERATED_TEST_ASSERT(
			jsval_url_password(&region, url, &component) == 0 &&
			expect_string(&region, component, "pass", "password")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "password mismatch");
	GENERATED_TEST_ASSERT(
			jsval_url_hostname(&region, url, &component) == 0 &&
			expect_string(&region, component, "example.com", "hostname")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "hostname mismatch");
	GENERATED_TEST_ASSERT(
			jsval_url_host(&region, url, &component) == 0 &&
			expect_string(&region, component, "example.com:8080", "host")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "host mismatch");
	GENERATED_TEST_ASSERT(
			jsval_url_port(&region, url, &component) == 0 &&
			expect_string(&region, component, "8080", "port")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "port mismatch");
	GENERATED_TEST_ASSERT(
			jsval_url_pathname(&region, url, &component) == 0 &&
			expect_string(&region, component, "/path", "pathname")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "pathname mismatch");
	GENERATED_TEST_ASSERT(
			jsval_url_search(&region, url, &component) == 0 &&
			expect_string(&region, component, "?a=1", "search")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "search mismatch");
	GENERATED_TEST_ASSERT(
			jsval_url_hash(&region, url, &component) == 0 &&
			expect_string(&region, component, "#frag", "hash")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "hash mismatch");

	/* href round-trip. */
	GENERATED_TEST_ASSERT(
			jsval_url_href(&region, url, &component) == 0 &&
			expect_string(&region, component,
				"https://user:pass@example.com:8080/path?a=1#frag",
				"href") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "href mismatch");

	/* Setter round-trip: mutating pathname updates href. */
	GENERATED_TEST_ASSERT(
			make_string(&region, "/newpath", &new_pathname) == 0,
			SUITE, CASE_NAME, "failed to build new pathname");
	GENERATED_TEST_ASSERT(
			jsval_url_set_pathname(&region, url, new_pathname) == 0,
			SUITE, CASE_NAME, "jsval_url_set_pathname failed");
	GENERATED_TEST_ASSERT(
			jsval_url_pathname(&region, url, &component) == 0 &&
			expect_string(&region, component, "/newpath", "pathname2")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "pathname post-set mismatch");
	GENERATED_TEST_ASSERT(
			jsval_url_href(&region, url, &component) == 0 &&
			expect_string(&region, component,
				"https://user:pass@example.com:8080/newpath?a=1#frag",
				"href2") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "href post-set mismatch");

	return generated_test_pass(SUITE, CASE_NAME);
}
