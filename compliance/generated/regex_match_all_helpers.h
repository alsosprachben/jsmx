#ifndef JSMX_COMPLIANCE_REGEX_MATCH_ALL_HELPERS_H
#define JSMX_COMPLIANCE_REGEX_MATCH_ALL_HELPERS_H

#include <errno.h>

#include "compliance/generated/regex_core_helpers.h"
#include "compliance/generated/regex_exec_match_helpers.h"

static inline int
generated_expect_match_iterator_next(jsval_region_t *region, jsval_t iterator,
		jsval_t *result_ptr, const char *suite, const char *case_name,
		const char *label)
{
	jsmethod_error_t error;
	int done = 0;

	if (jsval_match_iterator_next(region, iterator, &done, result_ptr,
			&error) < 0) {
		return generated_test_fail(suite, case_name,
				"%s: jsval_match_iterator_next failed: errno=%d kind=%d",
				label, errno, (int)error.kind);
	}
	if (done) {
		return generated_test_fail(suite, case_name,
				"%s: expected iterator result, got done", label);
	}
	return GENERATED_TEST_PASS;
}

static inline int
generated_expect_match_iterator_done(jsval_region_t *region, jsval_t iterator,
		const char *suite, const char *case_name, const char *label)
{
	jsmethod_error_t error;
	jsval_t value;
	int done = 0;

	if (jsval_match_iterator_next(region, iterator, &done, &value, &error) < 0) {
		return generated_test_fail(suite, case_name,
				"%s: jsval_match_iterator_next failed: errno=%d kind=%d",
				label, errno, (int)error.kind);
	}
	if (!done || value.kind != JSVAL_KIND_UNDEFINED) {
		return generated_test_fail(suite, case_name,
				"%s: expected done + undefined", label);
	}
	return GENERATED_TEST_PASS;
}

#endif
