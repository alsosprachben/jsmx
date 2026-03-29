#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsmethod.h"

#define SUITE "strings"
#define CASE_NAME "test262/return-abrupt-from-form-as-symbol"

int
main(void)
{
	static const uint8_t receiver[] = "foo";
	uint16_t storage[16];
	uint16_t form_storage[8];
	uint32_t workspace[32];
	jsstr16_t out;
	jsmethod_error_t error;

	jsstr16_init_from_buf(&out, (const char *)storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsmethod_string_normalize(&out,
			jsmethod_value_string_utf8(receiver, sizeof(receiver) - 1),
			1, jsmethod_value_symbol(),
			form_storage, sizeof(form_storage) / sizeof(form_storage[0]),
			workspace, sizeof(workspace) / sizeof(workspace[0]),
			&error) == -1, SUITE, CASE_NAME,
			"expected symbol form coercion to fail");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_TYPE,
			SUITE, CASE_NAME,
			"expected symbol form coercion to surface as TYPE, got %d",
			(int)error.kind);

	return generated_test_pass(SUITE, CASE_NAME);
}
