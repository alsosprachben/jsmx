#include "compliance/generated/test_contract.h"

#define SUITE "strings"
#define CASE_NAME "test262/return-abrupt-from-this"

int
main(void)
{
	return generated_test_known_unsupported(
			SUITE, CASE_NAME,
			"String.prototype receiver coercion and abrupt ToString(this) propagation are outside the jsmx string API");
}
