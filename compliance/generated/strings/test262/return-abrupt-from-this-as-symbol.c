#include "compliance/generated/test_contract.h"

#define SUITE "strings"
#define CASE_NAME "test262/return-abrupt-from-this-as-symbol"

int
main(void)
{
	return generated_test_known_unsupported(
			SUITE, CASE_NAME,
			"Symbol receiver coercion and JS method-dispatch TypeError semantics are above the jsmx string layer");
}
