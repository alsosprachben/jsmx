#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "values"
#define CASE_NAME "jsmx/nullish-ternary-regexp-parity"

int
main(void)
{
	uint8_t storage[4096];
	jsval_region_t region;
	jsmethod_error_t error;
	jsval_t pattern;
	jsval_t global_flags;
	jsval_t regex;
	jsval_t subject;
	jsval_t iterator;
	jsval_t fallback;
	jsval_t other;
	jsval_t result;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region, (const uint8_t *)"a", 1,
			&pattern) == 0, SUITE, CASE_NAME,
			"failed to construct regex pattern");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region, (const uint8_t *)"g", 1,
			&global_flags) == 0, SUITE, CASE_NAME,
			"failed to construct regex flags");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region, (const uint8_t *)"a", 1,
			&subject) == 0, SUITE, CASE_NAME,
			"failed to construct subject string");
	GENERATED_TEST_ASSERT(jsval_object_new(&region, 0, &fallback) == 0
			&& jsval_object_new(&region, 0, &other) == 0,
			SUITE, CASE_NAME, "failed to allocate fallback containers");
	GENERATED_TEST_ASSERT(jsval_regexp_new(&region, pattern, 1, global_flags,
			&regex, &error) == 0, SUITE, CASE_NAME,
			"failed to construct regexp value");
	GENERATED_TEST_ASSERT(jsval_method_string_match_all(&region, subject, 1,
			regex, &iterator, &error) == 0, SUITE, CASE_NAME,
			"failed to construct matchAll iterator");

	GENERATED_TEST_ASSERT(jsval_is_nullish(regex) == 0, SUITE, CASE_NAME,
			"expected regexp value to be non-nullish");
	GENERATED_TEST_ASSERT(jsval_is_nullish(iterator) == 0, SUITE, CASE_NAME,
			"expected matchAll iterator to be non-nullish");

	if (jsval_is_nullish(regex)) {
		result = fallback;
	} else {
		result = regex;
	}
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, result, regex) == 1, SUITE,
			CASE_NAME, "expected regexp ?? fallback to preserve the regexp");

	if (jsval_is_nullish(iterator)) {
		result = fallback;
	} else {
		result = iterator;
	}
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, result, iterator) == 1,
			SUITE, CASE_NAME,
			"expected iterator ?? fallback to preserve the iterator");

	if (jsval_truthy(&region, regex)) {
		result = regex;
	} else {
		result = other;
	}
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, result, regex) == 1, SUITE,
			CASE_NAME, "expected regexp ternary to preserve the regexp");

	if (jsval_truthy(&region, iterator)) {
		result = iterator;
	} else {
		result = other;
	}
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, result, iterator) == 1,
			SUITE, CASE_NAME,
			"expected iterator ternary to preserve the iterator");

	return generated_test_pass(SUITE, CASE_NAME);
}
