#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsmethod.h"

#define SUITE "strings"
#define CASE_NAME "test262/S15.5.4.18_A1_T1"

typedef struct receiver_ctx_s {
	const uint16_t *text;
	size_t len;
} receiver_ctx_t;

static int
receiver_to_string(void *opaque, jsstr16_t *out, jsmethod_error_t *error)
{
	receiver_ctx_t *ctx = (receiver_ctx_t *)opaque;

	(void)error;
	if (jsstr16_set_from_utf16(out, ctx->text, ctx->len) != ctx->len) {
		errno = ENOBUFS;
		return -1;
	}
	return 0;
}

static int
expect_utf16(jsstr16_t *actual, const uint16_t *expected, size_t expected_len)
{
	size_t i;

	if (actual->len != expected_len) {
		return generated_test_fail(SUITE, CASE_NAME,
				"expected %zu code units, got %zu",
				expected_len, actual->len);
	}
	for (i = 0; i < expected_len; i++) {
		if (actual->codeunits[i] != expected[i]) {
			return generated_test_fail(SUITE, CASE_NAME,
					"mismatch at code unit %zu", i);
		}
	}
	return GENERATED_TEST_PASS;
}

int
main(void)
{
	static const uint16_t receiver_text[] = {'t', 'r', 'u', 'e'};
	static const uint16_t expected[] = {'T', 'R', 'U', 'E'};
	uint16_t storage[16];
	jsstr16_t out;
	jsmethod_error_t error;
	receiver_ctx_t ctx = {
		receiver_text,
		sizeof(receiver_text) / sizeof(receiver_text[0]),
	};

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/toUpperCase/S15.5.4.18_A1_T1.js
	 *
	 * This is an idiomatic slow-path translation. The boxed Boolean receiver
	 * is lowered to an explicit translator callback that materializes its
	 * string coercion result, after which `jsmethod` performs the real string
	 * operation.
	 */
	jsstr16_init_from_buf(&out, (const char *)storage, sizeof(storage));
	if (jsmethod_string_to_upper_case(&out,
			jsmethod_value_coercible(&ctx, receiver_to_string),
			&error) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"slow-path receiver coercion should still uppercase successfully");
	}

	return expect_utf16(&out, expected, sizeof(expected) / sizeof(expected[0]));
}
