#include <errno.h>
#include <stddef.h>

#include "compliance/generated/test_contract.h"
#include "unicode.h"

#define SUITE "strings"
#define CASE_NAME "test262/form-is-not-valid-throws"

typedef struct invalid_case_s {
	const char *form;
	size_t form_len;
	const char *label;
} invalid_case_t;

static int
run_case(const invalid_case_t *tc)
{
	unicode_normalization_form_t form;

	errno = 0;
	if (unicode_normalization_form_parse(tc->form, tc->form_len, &form) != -1) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: invalid form parsed successfully", tc->label);
	}
	if (errno != EINVAL) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: expected EINVAL for invalid form, got %d",
				tc->label, errno);
	}

	return GENERATED_TEST_PASS;
}

int
main(void)
{
	static const invalid_case_t cases[] = {
		{"bar", 3, "invalid form string"},
		{"NFC1", 4, "form with extra suffix"},
		{"null", 4, "coerced null string"},
	};
	size_t i;

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/normalize/form-is-not-valid-throws.js
	 *
	 * At the `jsmx` layer, JS `RangeError` surfaces as invalid-form parsing
	 * through `unicode_normalization_form_parse`, which reports `EINVAL`.
	 */

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		int rc = run_case(&cases[i]);
		if (rc != GENERATED_TEST_PASS) {
			return rc;
		}
	}

	return generated_test_pass(SUITE, CASE_NAME);
}
