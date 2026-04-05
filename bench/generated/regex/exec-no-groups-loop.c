#include <stdint.h>

#include "bench/generated/bench_util.h"

int
main(void)
{
	static const uint8_t pattern_utf8[] = "([a-z]+)(\\d+)";
	static const uint8_t global_flags_utf8[] = "g";
	static const uint8_t input_a_utf8[] = "alpha12 beta34 gamma56";
	static const uint8_t input_b_utf8[] = "omega90 sigma12 rho34";
	uint8_t storage[65536];
	uint64_t checksum = 0;
	size_t i;

	for (i = 0; i < 20000u; i++) {
		jsval_region_t region;
		jsval_t pattern;
		jsval_t flags;
		jsval_t regex;
		jsval_t input;
		jsval_t match;
		jsval_t value;
		size_t input_len = 0;
		size_t len = 0;
		double number = 0.0;
		jsmethod_error_t error = {0};
		const uint8_t *input_utf8 = NULL;

		jsval_region_init(&region, storage, sizeof(storage));
		if (jsval_string_new_utf8(&region, pattern_utf8,
				sizeof(pattern_utf8) - 1, &pattern) < 0) {
			return bench_fail_errno("jsval_string_new_utf8(pattern)");
		}
		if (jsval_string_new_utf8(&region, global_flags_utf8,
				sizeof(global_flags_utf8) - 1, &flags) < 0) {
			return bench_fail_errno("jsval_string_new_utf8(flags)");
		}
		if (jsval_regexp_new(&region, pattern, 1, flags, &regex, &error) < 0) {
			return bench_fail("jsval_regexp_new");
		}
		if ((i & 1u) == 0) {
			input_utf8 = input_a_utf8;
			input_len = sizeof(input_a_utf8) - 1;
		} else {
			input_utf8 = input_b_utf8;
			input_len = sizeof(input_b_utf8) - 1;
		}
		if (jsval_string_new_utf8(&region, input_utf8, input_len, &input) < 0) {
			return bench_fail_errno("jsval_string_new_utf8(input)");
		}

		for (;;) {
			if (jsval_regexp_exec(&region, regex, input, &match, &error) < 0) {
				return bench_fail("jsval_regexp_exec");
			}
			if (match.kind == JSVAL_KIND_NULL) {
				break;
			}
			if (jsval_object_get_utf8(&region, match, (const uint8_t *)"0", 1,
					&value) < 0) {
				return bench_fail_errno("jsval_object_get_utf8(match[0])");
			}
			if (jsval_string_copy_utf8(&region, value, NULL, 0, &len) < 0) {
				return bench_fail_errno("jsval_string_copy_utf8(match[0])");
			}
			checksum += len;
			if (jsval_object_get_utf8(&region, match, (const uint8_t *)"1", 1,
					&value) < 0) {
				return bench_fail_errno("jsval_object_get_utf8(match[1])");
			}
			if (jsval_string_copy_utf8(&region, value, NULL, 0, &len) < 0) {
				return bench_fail_errno("jsval_string_copy_utf8(match[1])");
			}
			checksum += len;
			if (jsval_object_get_utf8(&region, match, (const uint8_t *)"2", 1,
					&value) < 0) {
				return bench_fail_errno("jsval_object_get_utf8(match[2])");
			}
			if (jsval_to_number(&region, value, &number) < 0) {
				return bench_fail_errno("jsval_to_number(match[2])");
			}
			checksum += (uint64_t)number;
		}
	}

	return bench_print_u64(checksum);
}
