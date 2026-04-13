#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "wintertc"
#define CASE_NAME "jsmx/fetch-response-parity"

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
		return generated_test_fail(SUITE, CASE_NAME, "%s: measure", label);
	}
	if (len != expected_len) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: len %zu != %zu", label, len, expected_len);
	}
	if (jsval_string_copy_utf8(region, value, buf, len, NULL) < 0
			|| memcmp(buf, expected, len) != 0) {
		return generated_test_fail(SUITE, CASE_NAME, "%s: bytes", label);
	}
	return GENERATED_TEST_PASS;
}

int
main(void)
{
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t body;
	jsval_t init;
	jsval_t response;
	jsval_t got;
	jsval_t name;
	jsval_t data;
	jsval_t promise;
	jsval_promise_state_t state;
	uint32_t status = 0;
	int ok = 0;

	jsval_region_init(&region, storage, sizeof(storage));

	/* new Response("hello", {status: 201, statusText: "Created"}) */
	GENERATED_TEST_ASSERT(make_string(&region, "hello", &body) == 0,
			SUITE, CASE_NAME, "body");
	GENERATED_TEST_ASSERT(jsval_object_new(&region, 4, &init) == 0
			&& jsval_object_set_utf8(&region, init,
				(const uint8_t *)"status", 6, jsval_number(201.0)) == 0
			&& make_string(&region, "Created", &got) == 0
			&& jsval_object_set_utf8(&region, init,
				(const uint8_t *)"statusText", 10, got) == 0,
			SUITE, CASE_NAME, "init");
	GENERATED_TEST_ASSERT(jsval_response_new(&region, body, 1, init, 1,
			&response) == 0, SUITE, CASE_NAME, "Response new");
	GENERATED_TEST_ASSERT(jsval_response_status(&region, response, &status)
			== 0 && status == 201, SUITE, CASE_NAME, "status 201");
	GENERATED_TEST_ASSERT(jsval_response_ok(&region, response, &ok) == 0
			&& ok == 1, SUITE, CASE_NAME, "ok true");
	GENERATED_TEST_ASSERT(jsval_response_text(&region, response, &promise)
			== 0 && jsval_promise_state(&region, promise, &state) == 0
			&& state == JSVAL_PROMISE_STATE_FULFILLED,
			SUITE, CASE_NAME, "text fulfilled");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, promise, &got) == 0
			&& expect_string(&region, got, "hello", "body")
				== GENERATED_TEST_PASS, SUITE, CASE_NAME, "body text");

	/* Response.json({ok:true}, {status:200}) */
	{
		static const char src[] = "{\"ok\":true}";
		jsval_t headers_val;
		jsval_t ct;
		jsval_t init2;

		GENERATED_TEST_ASSERT(jsval_json_parse(&region, (const uint8_t *)src,
				sizeof(src) - 1, 16, &data) == 0,
				SUITE, CASE_NAME, "parse json src");
		GENERATED_TEST_ASSERT(jsval_object_new(&region, 1, &init2) == 0
				&& jsval_object_set_utf8(&region, init2,
					(const uint8_t *)"status", 6, jsval_number(200.0)) == 0,
				SUITE, CASE_NAME, "init2");
		GENERATED_TEST_ASSERT(jsval_response_json(&region, data, 1, init2,
				&response) == 0, SUITE, CASE_NAME, "Response.json");
		GENERATED_TEST_ASSERT(jsval_response_headers(&region, response,
				&headers_val) == 0, SUITE, CASE_NAME, "headers");
		GENERATED_TEST_ASSERT(make_string(&region, "Content-Type", &name) == 0
				&& jsval_headers_get(&region, headers_val, name, &ct) == 0
				&& expect_string(&region, ct, "application/json", "ct")
					== GENERATED_TEST_PASS, SUITE, CASE_NAME,
				"Response.json content-type");
	}

	/* Response.error() */
	GENERATED_TEST_ASSERT(jsval_response_error(&region, &response) == 0,
			SUITE, CASE_NAME, "Response.error");
	GENERATED_TEST_ASSERT(jsval_response_status(&region, response, &status)
			== 0 && status == 0, SUITE, CASE_NAME, "error status 0");
	GENERATED_TEST_ASSERT(jsval_response_type(&region, response, &got) == 0
			&& expect_string(&region, got, "error", "type")
				== GENERATED_TEST_PASS, SUITE, CASE_NAME, "error type");

	return generated_test_pass(SUITE, CASE_NAME);
}
