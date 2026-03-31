#include <stdint.h>
#include <string.h>

#include "compliance/generated/string_pad_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/padEnd/observable-operations"

static int
append_log(char *log, size_t cap, const char *entry)
{
	size_t used = strlen(log);
	size_t len = strlen(entry);

	if (used + len + 1 > cap) {
		errno = ENOBUFS;
		return -1;
	}
	memcpy(log + used, entry, len + 1);
	return 0;
}

static int
lower_receiver(char *log, size_t cap, jsmethod_value_t *value_ptr)
{
	if (append_log(log, cap, "|toString:receiver") < 0 ||
			append_log(log, cap, "|valueOf:receiver") < 0) {
		return -1;
	}
	*value_ptr = jsmethod_value_string_utf8((const uint8_t *)"abc", 3);
	return 0;
}

static int
lower_max_length(char *log, size_t cap, jsmethod_value_t *value_ptr)
{
	if (append_log(log, cap, "|valueOf:maxLength") < 0 ||
			append_log(log, cap, "|toString:maxLength") < 0) {
		return -1;
	}
	*value_ptr = jsmethod_value_number(11.0);
	return 0;
}

static int
lower_fill_string(char *log, size_t cap, jsmethod_value_t *value_ptr)
{
	if (append_log(log, cap, "|toString:fillString") < 0 ||
			append_log(log, cap, "|valueOf:fillString") < 0) {
		return -1;
	}
	*value_ptr = jsmethod_value_string_utf8((const uint8_t *)"def", 3);
	return 0;
}

int
main(void)
{
	static const char expected_log[] =
			"|toString:receiver|valueOf:receiver|valueOf:maxLength"
			"|toString:maxLength|toString:fillString|valueOf:fillString";
	char log[256] = "";
	uint16_t storage[16];
	jsmethod_error_t error;
	jsmethod_value_t receiver_value;
	jsmethod_value_t max_length_value;
	jsmethod_value_t fill_string_value;
	jsstr16_t out;

	GENERATED_TEST_ASSERT(lower_receiver(log, sizeof(log),
			&receiver_value) == 0, SUITE, CASE_NAME,
			"failed to lower receiver");
	GENERATED_TEST_ASSERT(lower_max_length(log, sizeof(log),
			&max_length_value) == 0, SUITE, CASE_NAME,
			"failed to lower maxLength");
	GENERATED_TEST_ASSERT(lower_fill_string(log, sizeof(log),
			&fill_string_value) == 0, SUITE, CASE_NAME,
			"failed to lower fillString");
	GENERATED_TEST_ASSERT(generated_measure_and_pad(&out, storage,
			GENERATED_LEN(storage), receiver_value, 1, max_length_value,
			1, fill_string_value, jsmethod_string_pad_end_measure, jsmethod_string_pad_end,
			&error) == 0, SUITE, CASE_NAME,
			"failed observable padEnd");
	GENERATED_TEST_ASSERT(generated_expect_utf8_string(&out,
			"abcdefdefde", SUITE, CASE_NAME,
			"observable result") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected observable result");
	GENERATED_TEST_ASSERT(strcmp(log, expected_log) == 0,
			SUITE, CASE_NAME, "unexpected observable order: %s", log);
	return generated_test_pass(SUITE, CASE_NAME);
}
