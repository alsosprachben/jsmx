#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "wintertc"
#define CASE_NAME "jsmx/fetch-request-body-consumed-error"

static int
expect_rejected_body_used(jsval_region_t *region, jsval_t promise,
		const char *label)
{
	jsval_promise_state_t state;
	jsval_t reason;
	jsval_t name_value;
	jsval_t message_value;
	size_t len = 0;
	uint8_t buf[32];

	if (jsval_promise_state(region, promise, &state) != 0 ||
			state != JSVAL_PROMISE_STATE_REJECTED) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: expected rejected Promise", label);
	}
	if (jsval_promise_result(region, promise, &reason) != 0 ||
			reason.kind != JSVAL_KIND_DOM_EXCEPTION) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: rejection reason not DOMException", label);
	}
	if (jsval_dom_exception_name(region, reason, &name_value) != 0 ||
			jsval_string_copy_utf8(region, name_value, NULL, 0,
				&len) != 0 || len >= sizeof(buf) ||
			jsval_string_copy_utf8(region, name_value, buf, len,
				NULL) != 0 ||
			len != 9 || memcmp(buf, "TypeError", 9) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: name != 'TypeError'", label);
	}
	if (jsval_dom_exception_message(region, reason, &message_value)
				!= 0 ||
			jsval_string_copy_utf8(region, message_value, NULL, 0,
				&len) != 0 || len >= sizeof(buf) ||
			jsval_string_copy_utf8(region, message_value, buf, len,
				NULL) != 0 ||
			len != 17 || memcmp(buf, "body already used", 17) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: message != 'body already used'", label);
	}
	return GENERATED_TEST_PASS;
}

int
main(void)
{
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t url;
	jsval_t method;
	jsval_t body;
	jsval_t init;
	jsval_t request;
	jsval_t first_promise;
	jsval_t second_promise;
	jsval_t cross_promise;
	jsval_promise_state_t state;
	int used = 0;

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(
			jsval_string_new_utf8(&region,
				(const uint8_t *)"https://ex.com", 14, &url) == 0 &&
			jsval_string_new_utf8(&region, (const uint8_t *)"POST", 4,
				&method) == 0 &&
			jsval_string_new_utf8(&region, (const uint8_t *)"hello", 5,
				&body) == 0 &&
			jsval_object_new(&region, 4, &init) == 0 &&
			jsval_object_set_utf8(&region, init,
				(const uint8_t *)"method", 6, method) == 0 &&
			jsval_object_set_utf8(&region, init,
				(const uint8_t *)"body", 4, body) == 0 &&
			jsval_request_new(&region, url, init, 1, &request) == 0,
			SUITE, CASE_NAME, "Request setup");

	/* First text() consumes successfully. */
	GENERATED_TEST_ASSERT(
			jsval_request_text(&region, request, &first_promise)
				== 0 &&
			jsval_promise_state(&region, first_promise, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_FULFILLED,
			SUITE, CASE_NAME, "first text() not fulfilled");
	GENERATED_TEST_ASSERT(
			jsval_request_body_used(&region, request, &used) == 0 &&
			used == 1,
			SUITE, CASE_NAME, "bodyUsed not set after first consume");

	/* Second text() on same Request rejects. */
	GENERATED_TEST_ASSERT(
			jsval_request_text(&region, request, &second_promise)
				== 0,
			SUITE, CASE_NAME, "second text() C-level call should succeed");
	GENERATED_TEST_ASSERT(
			expect_rejected_body_used(&region, second_promise,
				"second text()") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "second text() should be rejected");

	/* Cross-consumer: json on same used Request also rejects. */
	GENERATED_TEST_ASSERT(
			jsval_request_json(&region, request, &cross_promise)
				== 0,
			SUITE, CASE_NAME, "json() C-level call should succeed");
	GENERATED_TEST_ASSERT(
			expect_rejected_body_used(&region, cross_promise,
				"cross json()") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "cross json() should be rejected");

	/* bodyUsed stays true. */
	GENERATED_TEST_ASSERT(
			jsval_request_body_used(&region, request, &used) == 0 &&
			used == 1,
			SUITE, CASE_NAME, "bodyUsed should remain true");

	return generated_test_pass(SUITE, CASE_NAME);
}
