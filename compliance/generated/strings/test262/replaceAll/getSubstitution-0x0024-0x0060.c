#include <stdint.h>

#include "compliance/generated/string_replace_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/replaceAll/getSubstitution-0x0024-0x0060"

int
main(void)
{
	static const uint8_t subject_utf8[] =
		"Ninguém é igual a ninguém. Todo o ser humano é um estranho ímpar.";
	static const uint8_t ninguem_lower_utf8[] = "ninguém";
	static const uint8_t ninguem_upper_utf8[] = "Ninguém";
	uint16_t storage[1024];
	jsmethod_error_t error;
	jsstr16_t out;
	jsmethod_value_t subject = jsmethod_value_string_utf8(subject_utf8,
			sizeof(subject_utf8) - 1);

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/replaceAll/getSubstitution-0x0024-0x0060.js
	 */

	GENERATED_TEST_ASSERT(generated_measure_and_replace_all(&out, storage,
			GENERATED_LEN(storage), subject,
			jsmethod_value_string_utf8(ninguem_lower_utf8,
					sizeof(ninguem_lower_utf8) - 1),
			jsmethod_value_string_utf8((const uint8_t *)"$`", 2),
			&error) == 0, SUITE, CASE_NAME,
			"failed \"$`\" replacement for \"ninguém\"");
	GENERATED_TEST_ASSERT(generated_expect_utf8_string(&out,
			"Ninguém é igual a Ninguém é igual a . Todo o ser humano é um estranho ímpar.",
			SUITE, CASE_NAME, "\"ninguém\" -> \"$`\"") ==
			GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected \"$`\" replacement for \"ninguém\"");

	GENERATED_TEST_ASSERT(generated_measure_and_replace_all(&out, storage,
			GENERATED_LEN(storage), subject,
			jsmethod_value_string_utf8(ninguem_upper_utf8,
					sizeof(ninguem_upper_utf8) - 1),
			jsmethod_value_string_utf8((const uint8_t *)"$`", 2),
			&error) == 0, SUITE, CASE_NAME,
			"failed \"$`\" replacement for leading token");
	GENERATED_TEST_ASSERT(generated_expect_utf8_string(&out,
			" é igual a ninguém. Todo o ser humano é um estranho ímpar.",
			SUITE, CASE_NAME, "\"Ninguém\" -> \"$`\"") ==
			GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected \"$`\" replacement for leading token");

	GENERATED_TEST_ASSERT(generated_measure_and_replace_all(&out, storage,
			GENERATED_LEN(storage), subject,
			jsmethod_value_string_utf8(ninguem_lower_utf8,
					sizeof(ninguem_lower_utf8) - 1),
			jsmethod_value_string_utf8((const uint8_t *)"($`)", 4),
			&error) == 0, SUITE, CASE_NAME,
			"failed \"($`)\" replacement for \"ninguém\"");
	GENERATED_TEST_ASSERT(generated_expect_utf8_string(&out,
			"Ninguém é igual a (Ninguém é igual a ). Todo o ser humano é um estranho ímpar.",
			SUITE, CASE_NAME, "\"ninguém\" -> \"($`)\"") ==
			GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected \"($`)\" replacement for \"ninguém\"");
	return generated_test_pass(SUITE, CASE_NAME);
}
