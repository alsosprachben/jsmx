#include "compliance/generated/test_contract.h"

#define SUITE "strings"
#define CASE_NAME "test262/this-is-null-throws"

int
main(void)
{
	return generated_test_known_unsupported(
			SUITE, CASE_NAME,
			"RequireObjectCoercible(this) for String.prototype.normalize is a JS method-layer concern, not a jsstr primitive");
}
