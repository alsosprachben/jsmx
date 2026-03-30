#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "values"
#define CASE_NAME "test262/S11.4.9_A2.1_T1"

int
main(void)
{
	uint8_t storage[512];
	jsval_region_t region;
	jsval_t object;
	jsval_t prop;
	jsval_t x = jsval_bool(1);

	/*
	 * Generated from test262:
	 * test/language/expressions/logical-not/S11.4.9_A2.1_T1.js
	 *
	 * This idiomatic flattened translation preserves the primitive, local
	 * variable, and object-property GetValue cases through direct `jsval`
	 * values and a native object property read.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_object_new(&region, 1, &object) == 0,
			SUITE, CASE_NAME, "failed to allocate native object");
	GENERATED_TEST_ASSERT(jsval_object_set_utf8(&region, object,
			(const uint8_t *)"prop", 4, jsval_bool(1)) == 0,
			SUITE, CASE_NAME, "failed to set object.prop");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, object,
			(const uint8_t *)"prop", 4, &prop) == 0,
			SUITE, CASE_NAME, "failed to read object.prop");

	GENERATED_TEST_ASSERT((!jsval_truthy(&region, jsval_bool(1))) == 0,
			SUITE, CASE_NAME, "expected !true to be false");
	GENERATED_TEST_ASSERT((!(!jsval_truthy(&region, jsval_bool(1)))) == 1,
			SUITE, CASE_NAME, "expected !(!true) to be true");
	GENERATED_TEST_ASSERT((!jsval_truthy(&region, x)) == 0,
			SUITE, CASE_NAME, "expected !x to be false after GetValue");
	GENERATED_TEST_ASSERT((!(!jsval_truthy(&region, x))) == 1,
			SUITE, CASE_NAME, "expected !(!x) to be true after GetValue");
	GENERATED_TEST_ASSERT((!jsval_truthy(&region, prop)) == 0,
			SUITE, CASE_NAME, "expected !object.prop to be false after GetValue");

	return generated_test_pass(SUITE, CASE_NAME);
}
