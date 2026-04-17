#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "promises"
#define CASE_NAME "jsmx/any-all-rejected"

static int
expect_error_name(jsval_region_t *region, jsval_t exception,
		const char *expected_name)
{
	jsval_t name_value;
	size_t name_len = 0;
	uint8_t buf[32];

	if (jsval_dom_exception_name(region, exception, &name_value) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"failed to read exception.name");
	}
	if (jsval_string_copy_utf8(region, name_value, NULL, 0,
			&name_len) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"failed to measure exception.name");
	}
	if (name_len != strlen(expected_name) ||
			name_len >= sizeof(buf)) {
		return generated_test_fail(SUITE, CASE_NAME,
				"exception.name length mismatch");
	}
	if (jsval_string_copy_utf8(region, name_value, buf, name_len,
			NULL) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"failed to read exception.name bytes");
	}
	if (memcmp(buf, expected_name, name_len) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"expected exception.name == %s", expected_name);
	}
	return GENERATED_TEST_PASS;
}

int
main(void)
{
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t inputs[3];
	jsval_t any;
	jsval_t reason;
	jsval_t errors;
	jsval_t elem;
	jsval_promise_state_t state;

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(
			jsval_promise_new(&region, &inputs[0]) == 0 &&
			jsval_promise_reject(&region, inputs[0],
					jsval_number(1.0)) == 0,
			SUITE, CASE_NAME, "failed to pre-reject inputs[0]");
	GENERATED_TEST_ASSERT(
			jsval_promise_new(&region, &inputs[1]) == 0 &&
			jsval_promise_reject(&region, inputs[1],
					jsval_number(2.0)) == 0,
			SUITE, CASE_NAME, "failed to pre-reject inputs[1]");
	GENERATED_TEST_ASSERT(
			jsval_promise_new(&region, &inputs[2]) == 0 &&
			jsval_promise_reject(&region, inputs[2],
					jsval_number(3.0)) == 0,
			SUITE, CASE_NAME, "failed to pre-reject inputs[2]");

	GENERATED_TEST_ASSERT(
			jsval_promise_any(&region, inputs, 3, &any) == 0,
			SUITE, CASE_NAME, "jsval_promise_any failed");
	GENERATED_TEST_ASSERT(
			jsval_promise_state(&region, any, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_REJECTED,
			SUITE, CASE_NAME, "expected REJECTED");
	GENERATED_TEST_ASSERT(
			jsval_promise_result(&region, any, &reason) == 0 &&
			reason.kind == JSVAL_KIND_DOM_EXCEPTION,
			SUITE, CASE_NAME, "expected DOMException reason");
	GENERATED_TEST_ASSERT(
			expect_error_name(&region, reason, "AggregateError")
					== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "expected AggregateError");
	GENERATED_TEST_ASSERT(
			jsval_dom_exception_errors(&region, reason,
					&errors) == 0 &&
			errors.kind == JSVAL_KIND_ARRAY &&
			jsval_array_length(&region, errors) == 3,
			SUITE, CASE_NAME, "expected 3-element errors array");
	GENERATED_TEST_ASSERT(
			jsval_array_get(&region, errors, 0, &elem) == 0 &&
			elem.kind == JSVAL_KIND_NUMBER &&
			elem.as.number == 1.0,
			SUITE, CASE_NAME, "errors[0] mismatch");
	GENERATED_TEST_ASSERT(
			jsval_array_get(&region, errors, 1, &elem) == 0 &&
			elem.kind == JSVAL_KIND_NUMBER &&
			elem.as.number == 2.0,
			SUITE, CASE_NAME, "errors[1] mismatch");
	GENERATED_TEST_ASSERT(
			jsval_array_get(&region, errors, 2, &elem) == 0 &&
			elem.kind == JSVAL_KIND_NUMBER &&
			elem.as.number == 3.0,
			SUITE, CASE_NAME, "errors[2] mismatch");

	return generated_test_pass(SUITE, CASE_NAME);
}
