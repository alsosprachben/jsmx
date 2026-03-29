#include "compliance/generated/test_contract.h"

#define SUITE "strings"
#define CASE_NAME "test262/return-abrupt-from-form-as-symbol"

int
main(void)
{
	return generated_test_known_unsupported(
			SUITE, CASE_NAME,
			"Symbol coercion and JS ToString(form) TypeError semantics are host/runtime concerns above jsmx");
}
