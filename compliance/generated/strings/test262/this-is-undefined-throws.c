#include "compliance/generated/test_contract.h"

#define SUITE "strings"
#define CASE_NAME "test262/this-is-undefined-throws"

int
main(void)
{
	return generated_test_known_unsupported(
			SUITE, CASE_NAME,
			"Undefined receiver rejection for String.prototype.normalize belongs to the JS method/runtime layer above jsmx");
}
