#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "wintertc"
#define CASE_NAME "jsmx/fetch-request-parity"

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
				"%s: expected string", label);
	}
	if (jsval_string_copy_utf8(region, value, NULL, 0, &len) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: failed to measure", label);
	}
	if (len != expected_len) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: expected %zu bytes, got %zu", label, expected_len, len);
	}
	if (jsval_string_copy_utf8(region, value, buf, len, NULL) < 0
			|| memcmp(buf, expected, len) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: unexpected bytes", label);
	}
	return GENERATED_TEST_PASS;
}

int
main(void)
{
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t init;
	jsval_t url;
	jsval_t request;
	jsval_t inner_headers;
	jsval_t method_val;
	jsval_t headers_obj;
	jsval_t body_val;
	jsval_t ct_name;
	jsval_t ct_value;
	jsval_t promise;
	jsval_t got;
	jsval_promise_state_t state;
	int used = 0;

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(jsval_object_new(&region, 4, &init) == 0,
			SUITE, CASE_NAME, "init object");
	GENERATED_TEST_ASSERT(make_string(&region, "POST", &method_val) == 0
			&& jsval_object_set_utf8(&region, init,
				(const uint8_t *)"method", 6, method_val) == 0,
			SUITE, CASE_NAME, "init.method");

	GENERATED_TEST_ASSERT(jsval_object_new(&region, 2, &headers_obj) == 0
			&& make_string(&region, "Content-Type", &ct_name) == 0
			&& make_string(&region, "application/json", &ct_value) == 0
			&& jsval_object_set_utf8(&region, headers_obj,
				(const uint8_t *)"Content-Type", 12, ct_value) == 0
			&& jsval_object_set_utf8(&region, init,
				(const uint8_t *)"headers", 7, headers_obj) == 0,
			SUITE, CASE_NAME, "init.headers");

	GENERATED_TEST_ASSERT(make_string(&region, "{\"x\":1}", &body_val) == 0
			&& jsval_object_set_utf8(&region, init,
				(const uint8_t *)"body", 4, body_val) == 0,
			SUITE, CASE_NAME, "init.body");

	GENERATED_TEST_ASSERT(make_string(&region, "https://example.com/api",
			&url) == 0, SUITE, CASE_NAME, "url string");
	GENERATED_TEST_ASSERT(jsval_request_new(&region, url, init, 1, &request)
			== 0, SUITE, CASE_NAME, "Request new");

	GENERATED_TEST_ASSERT(jsval_request_method(&region, request, &got) == 0
			&& expect_string(&region, got, "POST", "method")
				== GENERATED_TEST_PASS, SUITE, CASE_NAME, "request.method");

	GENERATED_TEST_ASSERT(jsval_request_url(&region, request, &got) == 0
			&& expect_string(&region, got, "https://example.com/api", "url")
				== GENERATED_TEST_PASS, SUITE, CASE_NAME, "request.url");

	GENERATED_TEST_ASSERT(jsval_request_headers(&region, request,
			&inner_headers) == 0, SUITE, CASE_NAME, "request.headers");
	GENERATED_TEST_ASSERT(make_string(&region, "content-type", &ct_name) == 0
			&& jsval_headers_get(&region, inner_headers, ct_name, &got) == 0
			&& expect_string(&region, got, "application/json", "ct")
				== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"content-type passthrough");

	GENERATED_TEST_ASSERT(jsval_request_body_used(&region, request, &used)
			== 0 && used == 0, SUITE, CASE_NAME, "bodyUsed before text()");

	GENERATED_TEST_ASSERT(jsval_request_text(&region, request, &promise) == 0
			&& jsval_promise_state(&region, promise, &state) == 0
			&& state == JSVAL_PROMISE_STATE_FULFILLED,
			SUITE, CASE_NAME, "text() fulfilled");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, promise, &got) == 0
			&& expect_string(&region, got, "{\"x\":1}", "body text")
				== GENERATED_TEST_PASS, SUITE, CASE_NAME, "body text");

	GENERATED_TEST_ASSERT(jsval_request_body_used(&region, request, &used)
			== 0 && used == 1, SUITE, CASE_NAME, "bodyUsed after text()");

	return generated_test_pass(SUITE, CASE_NAME);
}
