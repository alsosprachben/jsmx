#include <errno.h>
#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "objects"
#define CASE_NAME "jsmx/array-clone-dense-capacity-error"

int
main(void)
{
	static const uint8_t json_source[] = "[1,2]";
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t src;
	jsval_t out;

	/*
	 * Generated from repo-authored source:
	 * compliance/js/objects/jsmx/array-clone-dense-capacity-error.js
	 *
	 * This idiomatic flattened translation exercises the explicit ENOBUFS
	 * capacity contract on jsval_array_clone_dense().
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_json_parse(&region, json_source,
			sizeof(json_source) - 1, 8, &src) == 0,
			SUITE, CASE_NAME, "failed to parse JSON source array");
	out = jsval_null();
	errno = 0;
	GENERATED_TEST_ASSERT(jsval_array_clone_dense(&region, src, 1, &out) == -1,
			SUITE, CASE_NAME,
			"expected clone with undersized capacity to fail");
	GENERATED_TEST_ASSERT(errno == ENOBUFS, SUITE, CASE_NAME,
			"expected ENOBUFS from undersized clone capacity");
	GENERATED_TEST_ASSERT(out.kind == JSVAL_KIND_NULL, SUITE, CASE_NAME,
			"expected failed clone output to remain unchanged");

	return generated_test_pass(SUITE, CASE_NAME);
}
