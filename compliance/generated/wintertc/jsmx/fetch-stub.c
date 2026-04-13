#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "wintertc"
#define CASE_NAME "jsmx/fetch-stub"

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
	jsval_t promise;
	jsval_t reason;
	jsval_t got;
	jsval_promise_state_t state;
	size_t len = 0;
	static const char expected[] = "network not implemented yet";
	uint8_t buf[sizeof(expected) - 1];

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(make_string(&region, "https://example.com", &url)
			== 0, SUITE, CASE_NAME, "url");
	GENERATED_TEST_ASSERT(jsval_fetch(&region, url, jsval_undefined(), 0,
			&promise) == 0, SUITE, CASE_NAME, "jsval_fetch");
	GENERATED_TEST_ASSERT(promise.kind == JSVAL_KIND_PROMISE,
			SUITE, CASE_NAME, "promise kind");
	GENERATED_TEST_ASSERT(jsval_promise_state(&region, promise, &state) == 0
			&& state == JSVAL_PROMISE_STATE_REJECTED,
			SUITE, CASE_NAME, "rejected");
	GENERATED_TEST_ASSERT(jsval_promise_result(&region, promise, &reason)
			== 0, SUITE, CASE_NAME, "reason");
	GENERATED_TEST_ASSERT(jsval_dom_exception_name(&region, reason, &got)
			== 0 && got.kind == JSVAL_KIND_STRING,
			SUITE, CASE_NAME, "exception.name");
	len = 0;
	GENERATED_TEST_ASSERT(jsval_string_copy_utf8(&region, got, NULL, 0, &len)
			== 0 && len == 9, SUITE, CASE_NAME, "name len TypeError");
	{
		uint8_t nbuf[9];
		GENERATED_TEST_ASSERT(jsval_string_copy_utf8(&region, got, nbuf, 9,
				NULL) == 0 && memcmp(nbuf, "TypeError", 9) == 0,
				SUITE, CASE_NAME, "name TypeError");
	}
	GENERATED_TEST_ASSERT(jsval_dom_exception_message(&region, reason, &got)
			== 0 && got.kind == JSVAL_KIND_STRING,
			SUITE, CASE_NAME, "exception.message");
	GENERATED_TEST_ASSERT(jsval_string_copy_utf8(&region, got, NULL, 0, &len)
			== 0 && len == sizeof(expected) - 1,
			SUITE, CASE_NAME, "message len");
	GENERATED_TEST_ASSERT(jsval_string_copy_utf8(&region, got, buf,
			sizeof(buf), NULL) == 0
			&& memcmp(buf, expected, sizeof(buf)) == 0,
			SUITE, CASE_NAME, "message text");

	return generated_test_pass(SUITE, CASE_NAME);
}
