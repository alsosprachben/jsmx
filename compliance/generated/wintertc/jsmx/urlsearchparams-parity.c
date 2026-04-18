#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "wintertc"
#define CASE_NAME "jsmx/urlsearchparams-parity"

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
	jsval_t init;
	jsval_t params;
	jsval_t name_a;
	jsval_t name_b;
	jsval_t name_c;
	jsval_t name_missing;
	jsval_t value_new;
	jsval_t value_sole;
	jsval_t size_val;
	jsval_t has_val;
	jsval_t got;

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(
			make_string(&region, "a=1&b=two%20words&a=3", &init) == 0,
			SUITE, CASE_NAME, "failed to create init string");
	GENERATED_TEST_ASSERT(
			jsval_url_search_params_new(&region, init, &params) == 0,
			SUITE, CASE_NAME, "jsval_url_search_params_new failed");
	GENERATED_TEST_ASSERT(
			params.kind == JSVAL_KIND_URL_SEARCH_PARAMS,
			SUITE, CASE_NAME, "expected URL_SEARCH_PARAMS kind");

	GENERATED_TEST_ASSERT(make_string(&region, "a", &name_a) == 0,
			SUITE, CASE_NAME, "name_a");
	GENERATED_TEST_ASSERT(make_string(&region, "b", &name_b) == 0,
			SUITE, CASE_NAME, "name_b");
	GENERATED_TEST_ASSERT(make_string(&region, "c", &name_c) == 0,
			SUITE, CASE_NAME, "name_c");
	GENERATED_TEST_ASSERT(
			make_string(&region, "missing", &name_missing) == 0,
			SUITE, CASE_NAME, "name_missing");

	/* size === 3 */
	GENERATED_TEST_ASSERT(
			jsval_url_search_params_size(&region, params,
				&size_val) == 0 &&
			size_val.kind == JSVAL_KIND_NUMBER &&
			size_val.as.number == 3.0,
			SUITE, CASE_NAME, "expected initial size 3");

	/* has("a") === true, has("missing") === false */
	GENERATED_TEST_ASSERT(
			jsval_url_search_params_has(&region, params, name_a,
				&has_val) == 0 &&
			has_val.kind == JSVAL_KIND_BOOL &&
			has_val.as.boolean != 0,
			SUITE, CASE_NAME, "expected has('a') true");
	GENERATED_TEST_ASSERT(
			jsval_url_search_params_has(&region, params, name_missing,
				&has_val) == 0 &&
			has_val.kind == JSVAL_KIND_BOOL &&
			has_val.as.boolean == 0,
			SUITE, CASE_NAME, "expected has('missing') false");

	/* get("a") === "1" */
	GENERATED_TEST_ASSERT(
			jsval_url_search_params_get(&region, params, name_a,
				&got) == 0 &&
			expect_string(&region, got, "1", "get(a)")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "get('a') mismatch");

	/* get("b") === "two words" (percent-decoded) */
	GENERATED_TEST_ASSERT(
			jsval_url_search_params_get(&region, params, name_b,
				&got) == 0 &&
			expect_string(&region, got, "two words", "get(b)")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "get('b') mismatch (%%20 decode)");

	/* Mutate: append("c", "new"); set("a", "sole"); delete("b"); */
	GENERATED_TEST_ASSERT(make_string(&region, "new", &value_new) == 0,
			SUITE, CASE_NAME, "value_new");
	GENERATED_TEST_ASSERT(make_string(&region, "sole", &value_sole) == 0,
			SUITE, CASE_NAME, "value_sole");

	GENERATED_TEST_ASSERT(
			jsval_url_search_params_append(&region, params, name_c,
				value_new) == 0,
			SUITE, CASE_NAME, "append('c') failed");
	GENERATED_TEST_ASSERT(
			jsval_url_search_params_set(&region, params, name_a,
				value_sole) == 0,
			SUITE, CASE_NAME, "set('a') failed");
	GENERATED_TEST_ASSERT(
			jsval_url_search_params_delete(&region, params, name_b) == 0,
			SUITE, CASE_NAME, "delete('b') failed");

	/* After mutations: size === 2, has('b') === false, get('a') === "sole". */
	GENERATED_TEST_ASSERT(
			jsval_url_search_params_size(&region, params,
				&size_val) == 0 &&
			size_val.kind == JSVAL_KIND_NUMBER &&
			size_val.as.number == 2.0,
			SUITE, CASE_NAME, "expected post-mutation size 2");
	GENERATED_TEST_ASSERT(
			jsval_url_search_params_has(&region, params, name_b,
				&has_val) == 0 &&
			has_val.as.boolean == 0,
			SUITE, CASE_NAME, "expected has('b') false post-delete");
	GENERATED_TEST_ASSERT(
			jsval_url_search_params_get(&region, params, name_a,
				&got) == 0 &&
			expect_string(&region, got, "sole", "get(a)2")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "get('a') post-set mismatch");

	/* sort + toString == "a=sole&c=new" (a < c lexicographically). */
	GENERATED_TEST_ASSERT(
			jsval_url_search_params_sort(&region, params) == 0,
			SUITE, CASE_NAME, "sort failed");
	GENERATED_TEST_ASSERT(
			jsval_url_search_params_to_string(&region, params,
				&got) == 0 &&
			expect_string(&region, got, "a=sole&c=new", "toString")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "toString mismatch");

	return generated_test_pass(SUITE, CASE_NAME);
}
