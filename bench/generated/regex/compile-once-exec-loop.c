#include <stdint.h>

#include "bench/generated/bench_util.h"
#include "jsregex.h"

#define BENCH_UTF16_LEN(a) (sizeof(a) / sizeof((a)[0]))

static uint64_t
bench_parse_decimal_utf16(const uint16_t *subject, size_t start, size_t end)
{
	uint64_t value = 0;
	size_t i;

	for (i = start; i < end; i++) {
		value = value * 10u + (uint64_t)(subject[i] - (uint16_t)'0');
	}
	return value;
}

int
main(void)
{
	static const uint16_t pattern_utf16[] = {
		'(', '[', 'a', '-', 'z', ']', '+', ')',
		'(', '\\', 'd', '+', ')'
	};
	static const uint16_t global_flags_utf16[] = {'g'};
	static const uint16_t input_a_utf16[] = {
		'a', 'l', 'p', 'h', 'a', '1', '2', ' ',
		'b', 'e', 't', 'a', '3', '4', ' ',
		'g', 'a', 'm', 'm', 'a', '5', '6'
	};
	static const uint16_t input_b_utf16[] = {
		'o', 'm', 'e', 'g', 'a', '9', '0', ' ',
		's', 'i', 'g', 'm', 'a', '1', '2', ' ',
		'r', 'h', 'o', '3', '4'
	};
	jsregex_compiled_t compiled = {0};
	uint64_t checksum = 0;
	size_t i;

	if (jsregex_compile_utf16(pattern_utf16, BENCH_UTF16_LEN(pattern_utf16),
			global_flags_utf16, BENCH_UTF16_LEN(global_flags_utf16),
			&compiled) < 0) {
		return bench_fail_errno("jsregex_compile_utf16");
	}

	for (i = 0; i < 20000u; i++) {
		const uint16_t *subject = NULL;
		size_t subject_len = 0;
		size_t start_index = 0;

		if ((i & 1u) == 0) {
			subject = input_a_utf16;
			subject_len = BENCH_UTF16_LEN(input_a_utf16);
		} else {
			subject = input_b_utf16;
			subject_len = BENCH_UTF16_LEN(input_b_utf16);
		}

		for (;;) {
			jsregex_exec_result_t result;
			size_t offsets[6];
			size_t next_index;

			if (jsregex_exec_utf16(&compiled, subject, subject_len,
					start_index, offsets, BENCH_UTF16_LEN(offsets),
					&result) < 0) {
				jsregex_release(&compiled);
				return bench_fail_errno("jsregex_exec_utf16");
			}
			if (!result.matched) {
				break;
			}
			checksum += (uint64_t)(offsets[1] - offsets[0]);
			checksum += (uint64_t)(offsets[3] - offsets[2]);
			checksum += bench_parse_decimal_utf16(subject, offsets[4],
					offsets[5]);

			next_index = result.end;
			if (next_index <= start_index) {
				if (jsregex_advance_string_index_utf16(subject, subject_len,
						start_index, 0, &next_index) < 0) {
					jsregex_release(&compiled);
					return bench_fail_errno(
							"jsregex_advance_string_index_utf16");
				}
			}
			start_index = next_index;
		}
	}

	jsregex_release(&compiled);
	return bench_print_u64(checksum);
}
