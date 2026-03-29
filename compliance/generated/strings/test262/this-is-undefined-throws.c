#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsmethod.h"

#define SUITE "strings"
#define CASE_NAME "test262/this-is-undefined-throws"

int
main(void)
{
	uint16_t storage[16];
	uint32_t workspace[32];
	jsstr16_t out;
	jsmethod_error_t error;

	jsstr16_init_from_buf(&out, (const char *)storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsmethod_string_normalize(&out,
			jsmethod_value_undefined(),
			0, jsmethod_value_undefined(),
			NULL, 0,
			workspace, sizeof(workspace) / sizeof(workspace[0]),
			&error) == -1, SUITE, CASE_NAME,
			"expected undefined receiver to fail RequireObjectCoercible");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_TYPE,
			SUITE, CASE_NAME,
			"expected undefined receiver to surface as TYPE, got %d",
			(int)error.kind);

	return generated_test_pass(SUITE, CASE_NAME);
}
