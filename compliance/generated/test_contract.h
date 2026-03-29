#ifndef JSMX_COMPLIANCE_TEST_CONTRACT_H
#define JSMX_COMPLIANCE_TEST_CONTRACT_H

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define GENERATED_TEST_PASS 0
#define GENERATED_TEST_WRONG_RESULT 1
#define GENERATED_TEST_KNOWN_UNSUPPORTED 2

static inline int
generated_test_vreport(int code, const char *status, const char *suite,
		const char *case_name, const char *fmt, va_list ap)
{
	fprintf(stderr, "[%s] %s/%s", status, suite, case_name);
	if (fmt && *fmt) {
		fprintf(stderr, ": ");
		vfprintf(stderr, fmt, ap);
	}
	fputc('\n', stderr);
	return code;
}

static inline int
generated_test_report(int code, const char *status, const char *suite,
		const char *case_name, const char *fmt, ...)
{
	va_list ap;
	int rc;

	va_start(ap, fmt);
	rc = generated_test_vreport(code, status, suite, case_name, fmt, ap);
	va_end(ap);
	return rc;
}

static inline int
generated_test_pass(const char *suite, const char *case_name)
{
	return generated_test_report(GENERATED_TEST_PASS, "PASS", suite,
			case_name, NULL);
}

static inline int
generated_test_fail(const char *suite, const char *case_name,
		const char *fmt, ...)
{
	va_list ap;
	int rc;

	va_start(ap, fmt);
	rc = generated_test_vreport(GENERATED_TEST_WRONG_RESULT, "FAIL", suite,
			case_name, fmt, ap);
	va_end(ap);
	return rc;
}

static inline int
generated_test_known_unsupported(const char *suite, const char *case_name,
		const char *fmt, ...)
{
	va_list ap;
	int rc;

	va_start(ap, fmt);
	rc = generated_test_vreport(GENERATED_TEST_KNOWN_UNSUPPORTED,
			"KNOWN_UNSUPPORTED", suite, case_name, fmt, ap);
	va_end(ap);
	return rc;
}

#define GENERATED_TEST_ASSERT(cond, suite, case_name, ...) \
	do { \
		if (!(cond)) \
			return generated_test_fail((suite), (case_name), \
					__VA_ARGS__); \
	} while (0)

#endif
