#include "compliance/generated/test_contract.h"

#define SUITE "strings"
#define CASE_NAME "test262/return-abrupt-from-form"

int
main(void)
{
	return generated_test_known_unsupported(
			SUITE, CASE_NAME,
			"JS ToString(form) abrupt completion and exception propagation live above the jsmx string layer");
}
