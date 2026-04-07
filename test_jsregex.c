#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

#include "jsregex.h"

#if JSMX_WITH_REGEX

static void
test_regex8_compile_exec(void)
{
	static const uint8_t subject[] = {'a','1','2','b'};
	static const uint8_t pattern[] = {
		'(','[','0','-','9',']','[','0','-','9',']',')',
		'(','[','a','-','z',']',')','?'
	};
	static const uint8_t sticky_pattern[] = {'b'};
	static const uint8_t search_subject[] = {'f','o','o','B','a','r'};
	static const uint8_t pattern_bar[] = {'b','a','r'};
	static const uint8_t flags_g[] = {'g'};
	static const uint8_t flags_i[] = {'i'};
	static const uint8_t flags_y[] = {'y'};
	static const uint8_t grep_line[] = {
		'G','E','T',' ','h','t','t','p',':','/','/','e','x','a','m','p','l','e'
	};
	static const uint8_t grep_pattern[] = {'h','t','t','p'};
	jsregex8_compiled_t compiled;
	jsregex8_exec_result_t result;
	jsregex8_search_result_t search_result;
	size_t offsets[6];

	assert(jsregex8_compile_utf8(pattern,
			sizeof(pattern) / sizeof(pattern[0]), NULL, 0, &compiled) == 0);
	assert(compiled.capture_count == 2);
	assert(jsregex8_exec_utf8(&compiled, subject,
			sizeof(subject) / sizeof(subject[0]), 0,
			offsets, sizeof(offsets) / sizeof(offsets[0]), &result) == 0);
	assert(result.matched == 1);
	assert(result.start == 1);
	assert(result.end == 4);
	assert(result.slot_count == 3);
	assert(offsets[0] == 1 && offsets[1] == 4);
	assert(offsets[2] == 1 && offsets[3] == 3);
	assert(offsets[4] == 3 && offsets[5] == 4);
	jsregex8_release(&compiled);

	assert(jsregex8_compile_utf8_jit(pattern,
			sizeof(pattern) / sizeof(pattern[0]), NULL, 0, &compiled) == 0);
	assert(compiled.capture_count == 2);
	assert(jsregex8_exec_utf8(&compiled, subject,
			sizeof(subject) / sizeof(subject[0]), 0,
			offsets, sizeof(offsets) / sizeof(offsets[0]), &result) == 0);
	assert(result.matched == 1);
	assert(result.start == 1);
	assert(result.end == 4);
	assert(result.slot_count == 3);
	jsregex8_release(&compiled);

	assert(jsregex8_compile_utf8(sticky_pattern,
			sizeof(sticky_pattern) / sizeof(sticky_pattern[0]),
			flags_y, sizeof(flags_y) / sizeof(flags_y[0]), &compiled) == 0);
	assert(jsregex8_exec_utf8(&compiled, subject,
			sizeof(subject) / sizeof(subject[0]), 0,
			offsets, 2, &result) == 0);
	assert(result.matched == 0);
	assert(jsregex8_exec_utf8(&compiled, subject,
			sizeof(subject) / sizeof(subject[0]), 3,
			offsets, 2, &result) == 0);
	assert(result.matched == 1);
	assert(result.start == 3);
	assert(result.end == 4);
	jsregex8_release(&compiled);

	assert(jsregex8_compile_utf8(sticky_pattern,
			sizeof(sticky_pattern) / sizeof(sticky_pattern[0]),
			flags_g, sizeof(flags_g) / sizeof(flags_g[0]), &compiled) == 0);
	assert(jsregex8_exec_utf8(&compiled, subject,
			sizeof(subject) / sizeof(subject[0]), 0,
			offsets, 2, &result) == 0);
	assert(result.matched == 1);
	jsregex8_release(&compiled);

	assert(jsregex8_search_utf8(search_subject,
			sizeof(search_subject) / sizeof(search_subject[0]),
			pattern_bar, sizeof(pattern_bar) / sizeof(pattern_bar[0]),
			flags_i, sizeof(flags_i) / sizeof(flags_i[0]), &search_result) == 0);
	assert(search_result.matched == 1);
	assert(search_result.start == 3);
	assert(search_result.end == 6);

	assert(jsregex8_search_utf8(search_subject,
			sizeof(search_subject) / sizeof(search_subject[0]),
			(const uint8_t[]){'['}, 1, NULL, 0, &search_result) == -1);
	assert(errno == EINVAL);

	assert(jsregex8_search_utf8(search_subject,
			sizeof(search_subject) / sizeof(search_subject[0]),
			pattern_bar, sizeof(pattern_bar) / sizeof(pattern_bar[0]),
			(const uint8_t[]){'z'}, 1, &search_result) == -1);
	assert(errno == ENOTSUP);

	assert(jsregex8_compile_utf8_jit(grep_pattern,
			sizeof(grep_pattern) / sizeof(grep_pattern[0]),
			NULL, 0, &compiled) == 0);
	assert(jsregex8_exec_utf8(&compiled, grep_line,
			sizeof(grep_line) / sizeof(grep_line[0]), 0,
			offsets, 2, &result) == 0);
	assert(result.matched == 1);
	assert(result.start == 4);
	assert(result.end == 8);
	jsregex8_release(&compiled);
}

static void
test_regex_search(void)
{
	jsregex_search_result_t result;
	static const uint16_t subject[] = {'f','o','o','B','a','r'};
	static const uint16_t pattern_bar[] = {'b','a','r'};
	static const uint16_t flags_i[] = {'i'};
	static const uint16_t flags_all[] = {'g','i','m','s','u','y'};
	static const uint16_t pattern_foo[] = {'f','o','o'};
	static const uint16_t flags_y[] = {'y'};
	static const uint16_t bad_flags[] = {'z'};
	static const uint16_t dup_flags[] = {'i','i'};

	assert(jsregex_search_utf16(subject,
			sizeof(subject) / sizeof(subject[0]),
			pattern_bar, sizeof(pattern_bar) / sizeof(pattern_bar[0]),
			flags_i, sizeof(flags_i) / sizeof(flags_i[0]), &result) == 0);
	assert(result.matched == 1);
	assert(result.start == 3);
	assert(result.end == 6);

	assert(jsregex_search_utf16(subject,
			sizeof(subject) / sizeof(subject[0]),
			pattern_foo, sizeof(pattern_foo) / sizeof(pattern_foo[0]),
			flags_y, sizeof(flags_y) / sizeof(flags_y[0]), &result) == 0);
	assert(result.matched == 1);
	assert(result.start == 0);
	assert(result.end == 3);

	assert(jsregex_search_utf16(subject,
			sizeof(subject) / sizeof(subject[0]),
			pattern_bar, sizeof(pattern_bar) / sizeof(pattern_bar[0]),
			flags_y, sizeof(flags_y) / sizeof(flags_y[0]), &result) == 0);
	assert(result.matched == 0);

	assert(jsregex_search_utf16(subject,
			sizeof(subject) / sizeof(subject[0]),
			(const uint16_t[]){'['}, 1, NULL, 0, &result) == -1);
	assert(errno == EINVAL);

	assert(jsregex_search_utf16(subject,
			sizeof(subject) / sizeof(subject[0]),
			pattern_bar, sizeof(pattern_bar) / sizeof(pattern_bar[0]),
			bad_flags, sizeof(bad_flags) / sizeof(bad_flags[0]), &result) == -1);
	assert(errno == ENOTSUP);

	assert(jsregex_search_utf16(subject,
			sizeof(subject) / sizeof(subject[0]),
			pattern_bar, sizeof(pattern_bar) / sizeof(pattern_bar[0]),
			dup_flags, sizeof(dup_flags) / sizeof(dup_flags[0]), &result) == -1);
	assert(errno == EINVAL);

	{
		jsregex_compiled_t compiled;

		assert(jsregex_compile_utf16(pattern_bar,
				sizeof(pattern_bar) / sizeof(pattern_bar[0]),
				flags_all, sizeof(flags_all) / sizeof(flags_all[0]),
				&compiled) == 0);
		assert(compiled.flags
				== (JSREGEX_FLAG_GLOBAL
				| JSREGEX_FLAG_IGNORE_CASE
				| JSREGEX_FLAG_MULTILINE
				| JSREGEX_FLAG_DOT_ALL
				| JSREGEX_FLAG_UNICODE
				| JSREGEX_FLAG_STICKY));
		jsregex_release(&compiled);
	}
}

static void
test_regex_compile_exec(void)
{
	static const uint16_t subject[] = {'a','1','2','b'};
	static const uint16_t pattern[] = {'(','[','0','-','9',']','[','0','-','9',']',')','(','[','a','-','z',']',')','?'};
	static const uint16_t sticky_pattern[] = {'b'};
	static const uint16_t flags_g[] = {'g'};
	static const uint16_t flags_y[] = {'y'};
	jsregex_compiled_t compiled;
	jsregex_exec_result_t result;
	size_t offsets[6];
	size_t next_index;

	assert(jsregex_compile_utf16(pattern,
			sizeof(pattern) / sizeof(pattern[0]), NULL, 0, &compiled) == 0);
	assert(compiled.capture_count == 2);
	assert(jsregex_exec_utf16(&compiled, subject,
			sizeof(subject) / sizeof(subject[0]), 0,
			offsets, sizeof(offsets) / sizeof(offsets[0]), &result) == 0);
	assert(result.matched == 1);
	assert(result.start == 1);
	assert(result.end == 4);
	assert(result.slot_count == 3);
	assert(offsets[0] == 1 && offsets[1] == 4);
	assert(offsets[2] == 1 && offsets[3] == 3);
	assert(offsets[4] == 3 && offsets[5] == 4);
	jsregex_release(&compiled);

	assert(jsregex_compile_utf16_jit(pattern,
			sizeof(pattern) / sizeof(pattern[0]), NULL, 0, &compiled) == 0);
	assert(compiled.capture_count == 2);
	assert(jsregex_exec_utf16(&compiled, subject,
			sizeof(subject) / sizeof(subject[0]), 0,
			offsets, sizeof(offsets) / sizeof(offsets[0]), &result) == 0);
	assert(result.matched == 1);
	assert(result.start == 1);
	assert(result.end == 4);
	assert(result.slot_count == 3);
	assert(offsets[0] == 1 && offsets[1] == 4);
	assert(offsets[2] == 1 && offsets[3] == 3);
	assert(offsets[4] == 3 && offsets[5] == 4);
	jsregex_release(&compiled);

	assert(jsregex_compile_utf16(sticky_pattern,
			sizeof(sticky_pattern) / sizeof(sticky_pattern[0]),
			flags_y, sizeof(flags_y) / sizeof(flags_y[0]), &compiled) == 0);
	assert(jsregex_exec_utf16(&compiled, subject,
			sizeof(subject) / sizeof(subject[0]), 0,
			offsets, 2, &result) == 0);
	assert(result.matched == 0);
	assert(jsregex_exec_utf16(&compiled, subject,
			sizeof(subject) / sizeof(subject[0]), 3,
			offsets, 2, &result) == 0);
	assert(result.matched == 1);
	assert(result.start == 3);
	assert(result.end == 4);
	jsregex_release(&compiled);

	assert(jsregex_compile_utf16(sticky_pattern,
			sizeof(sticky_pattern) / sizeof(sticky_pattern[0]),
			flags_g, sizeof(flags_g) / sizeof(flags_g[0]), &compiled) == 0);
	assert(jsregex_exec_utf16(&compiled, subject,
			sizeof(subject) / sizeof(subject[0]), 0,
			offsets, 2, &result) == 0);
	assert(result.matched == 1);
	jsregex_release(&compiled);

	assert(jsregex_advance_string_index_utf16(
			(const uint16_t[]){0xD834, 0xDF06}, 2, 0, 1, &next_index) == 0);
	assert(next_index == 2);
	assert(jsregex_advance_string_index_utf16(subject,
			sizeof(subject) / sizeof(subject[0]), 1, 0, &next_index) == 0);
	assert(next_index == 2);
}

static void
test_regex_named_groups(void)
{
	static const uint16_t pattern[] = {
		'(','?','<','d','i','g','i','t','s','>','[','0','-','9',']','+',
		')','(','?','<','t','a','i','l','>','[','a','-','z',']',')','?'
	};
	static const uint16_t digits_name[] = {'d','i','g','i','t','s'};
	static const uint16_t tail_name[] = {'t','a','i','l'};
	jsregex_compiled_t compiled;
	uint32_t capture_index;
	size_t name_len = 0;
	uint16_t name_buf[8];

	assert(jsregex_compile_utf16(pattern,
			sizeof(pattern) / sizeof(pattern[0]), NULL, 0, &compiled) == 0);
	assert(compiled.capture_count == 2);
	assert(compiled.named_group_count == 2);

	assert(jsregex_named_group_utf16(&compiled, 0, &capture_index, NULL, 0,
			&name_len) == 0);
	assert(capture_index == 1);
	assert(name_len == sizeof(digits_name) / sizeof(digits_name[0]));
	assert(jsregex_named_group_utf16(&compiled, 0, &capture_index, name_buf,
			sizeof(name_buf) / sizeof(name_buf[0]), &name_len) == 0);
	assert(capture_index == 1);
	assert(name_len == sizeof(digits_name) / sizeof(digits_name[0]));
	assert(memcmp(name_buf, digits_name, sizeof(digits_name)) == 0);

	assert(jsregex_named_group_utf16(&compiled, 1, &capture_index, name_buf,
			sizeof(name_buf) / sizeof(name_buf[0]), &name_len) == 0);
	assert(capture_index == 2);
	assert(name_len == sizeof(tail_name) / sizeof(tail_name[0]));
	assert(memcmp(name_buf, tail_name, sizeof(tail_name)) == 0);

	assert(jsregex_named_group_utf16(&compiled, 2, &capture_index, name_buf,
			sizeof(name_buf) / sizeof(name_buf[0]), &name_len) == -1);
	assert(errno == EINVAL);
	assert(jsregex_named_group_utf16(&compiled, 0, &capture_index, name_buf, 2,
			&name_len) == -1);
	assert(errno == ENOBUFS);

	jsregex_release(&compiled);

	assert(jsregex_compile_utf16_jit(pattern,
			sizeof(pattern) / sizeof(pattern[0]), NULL, 0, &compiled) == 0);
	assert(compiled.capture_count == 2);
	assert(compiled.named_group_count == 2);
	assert(jsregex_named_group_utf16(&compiled, 0, &capture_index, NULL, 0,
			&name_len) == 0);
	assert(capture_index == 1);
	assert(name_len == sizeof(digits_name) / sizeof(digits_name[0]));
	jsregex_release(&compiled);
}

static void
test_u_literal_surrogate_exec(void)
{
	static const uint16_t paired[] = {0xD834, 0xDF06};
	static const uint16_t lone_low[] = {0xDF06};
	static const uint16_t lone_high[] = {0xD834};
	static const uint16_t isolated_low[] = {'a', 0xDF06, 'b'};
	static const uint16_t pair_then_lone_low[] = {0xD834, 0xDF06, 0xDF06};
	static const uint16_t pair_then_lone_high[] = {0xD834, 0xDF06, 0xD834};
	jsregex_exec_result_t result;
	jsregex_search_result_t search_result;
	int matched = 0;

	assert(jsregex_exec_u_literal_surrogate_utf16(paired,
			sizeof(paired) / sizeof(paired[0]), 0xDF06, 0, &result) == 0);
	assert(result.matched == 0);

	assert(jsregex_exec_u_literal_surrogate_utf16(paired,
			sizeof(paired) / sizeof(paired[0]), 0xD834, 0, &result) == 0);
	assert(result.matched == 0);

	assert(jsregex_exec_u_literal_surrogate_utf16(lone_low,
			sizeof(lone_low) / sizeof(lone_low[0]), 0xDF06, 0, &result) == 0);
	assert(result.matched == 1);
	assert(result.start == 0);
	assert(result.end == 1);
	assert(result.slot_count == 1);

	assert(jsregex_exec_u_literal_surrogate_utf16(lone_high,
			sizeof(lone_high) / sizeof(lone_high[0]), 0xD834, 0, &result) == 0);
	assert(result.matched == 1);
	assert(result.start == 0);
	assert(result.end == 1);
	assert(result.slot_count == 1);

	assert(jsregex_exec_u_literal_surrogate_utf16(isolated_low,
			sizeof(isolated_low) / sizeof(isolated_low[0]), 0xDF06, 0,
			&result) == 0);
	assert(result.matched == 1);
	assert(result.start == 1);
	assert(result.end == 2);
	assert(result.slot_count == 1);

	assert(jsregex_exec_u_literal_surrogate_utf16(paired,
			sizeof(paired) / sizeof(paired[0]), 0xDF06, 1, &result) == 0);
	assert(result.matched == 0);

	assert(jsregex_exec_u_literal_surrogate_utf16(lone_low,
			sizeof(lone_low) / sizeof(lone_low[0]), 'a', 0, &result) == -1);
	assert(errno == EINVAL);

	assert(jsregex_test_u_literal_surrogate_utf16(lone_low,
			sizeof(lone_low) / sizeof(lone_low[0]), 0xDF06, 0,
			&matched) == 0);
	assert(matched == 1);

	assert(jsregex_test_u_literal_surrogate_utf16(paired,
			sizeof(paired) / sizeof(paired[0]), 0xDF06, 0,
			&matched) == 0);
	assert(matched == 0);

	assert(jsregex_test_u_literal_surrogate_utf16(lone_high,
			sizeof(lone_high) / sizeof(lone_high[0]), 0xD834, 0,
			&matched) == 0);
	assert(matched == 1);

	assert(jsregex_test_u_literal_surrogate_utf16(paired,
			sizeof(paired) / sizeof(paired[0]), 0xD834, 0,
			&matched) == 0);
	assert(matched == 0);

	assert(jsregex_search_u_literal_surrogate_utf16(pair_then_lone_low,
			sizeof(pair_then_lone_low) / sizeof(pair_then_lone_low[0]),
			0xDF06, 0, &search_result) == 0);
	assert(search_result.matched == 1);
	assert(search_result.start == 2);
	assert(search_result.end == 3);

	assert(jsregex_search_u_literal_surrogate_utf16(pair_then_lone_high,
			sizeof(pair_then_lone_high) / sizeof(pair_then_lone_high[0]),
			0xD834, 0, &search_result) == 0);
	assert(search_result.matched == 1);
	assert(search_result.start == 2);
	assert(search_result.end == 3);

	assert(jsregex_search_u_literal_surrogate_utf16(paired,
			sizeof(paired) / sizeof(paired[0]), 0xDF06, 0,
			&search_result) == 0);
	assert(search_result.matched == 0);

	assert(jsregex_search_u_literal_surrogate_utf16(paired,
			sizeof(paired) / sizeof(paired[0]), 0xD834, 0,
			&search_result) == 0);
	assert(search_result.matched == 0);
}

static void
test_u_literal_sequence_exec(void)
{
	static const uint16_t pair_then_low_b[] = {
		'A', 0xD834, 0xDF06, 0xDF06, 'B', 0xDF06, 'B'
	};
	static const uint16_t pair_then_high_b[] = {
		'A', 0xD834, 0xDF06, 0xD834, 'B', 0xD834, 'B'
	};
	static const uint16_t pair_subject[] = {'A', 0xD834, 0xDF06, 'B'};
	static const uint16_t low_b_pattern[] = {0xDF06, 'B'};
	static const uint16_t high_b_pattern[] = {0xD834, 'B'};
	static const uint16_t pair_pattern[] = {0xD834, 0xDF06};
	static const uint16_t a_high_pattern[] = {'A', 0xD834};
	jsregex_exec_result_t result;
	jsregex_search_result_t search_result;

	assert(jsregex_exec_u_literal_sequence_utf16(pair_then_low_b,
			sizeof(pair_then_low_b) / sizeof(pair_then_low_b[0]),
			low_b_pattern, sizeof(low_b_pattern) / sizeof(low_b_pattern[0]),
			0, &result) == 0);
	assert(result.matched == 1);
	assert(result.start == 3);
	assert(result.end == 5);
	assert(result.slot_count == 1);

	assert(jsregex_search_u_literal_sequence_utf16(pair_then_low_b,
			sizeof(pair_then_low_b) / sizeof(pair_then_low_b[0]),
			low_b_pattern, sizeof(low_b_pattern) / sizeof(low_b_pattern[0]),
			0, &search_result) == 0);
	assert(search_result.matched == 1);
	assert(search_result.start == 3);
	assert(search_result.end == 5);

	assert(jsregex_search_u_literal_sequence_utf16(pair_then_high_b,
			sizeof(pair_then_high_b) / sizeof(pair_then_high_b[0]),
			high_b_pattern, sizeof(high_b_pattern) / sizeof(high_b_pattern[0]),
			0, &search_result) == 0);
	assert(search_result.matched == 1);
	assert(search_result.start == 3);
	assert(search_result.end == 5);

	assert(jsregex_search_u_literal_sequence_utf16(pair_subject,
			sizeof(pair_subject) / sizeof(pair_subject[0]),
			a_high_pattern, sizeof(a_high_pattern) / sizeof(a_high_pattern[0]),
			0, &search_result) == 0);
	assert(search_result.matched == 0);

	assert(jsregex_search_u_literal_sequence_utf16(pair_subject,
			sizeof(pair_subject) / sizeof(pair_subject[0]),
			pair_pattern, sizeof(pair_pattern) / sizeof(pair_pattern[0]),
			0, &search_result) == 0);
	assert(search_result.matched == 1);
	assert(search_result.start == 1);
	assert(search_result.end == 3);

	assert(jsregex_exec_u_literal_sequence_utf16(pair_subject,
			sizeof(pair_subject) / sizeof(pair_subject[0]),
			low_b_pattern, sizeof(low_b_pattern) / sizeof(low_b_pattern[0]),
			0, &result) == 0);
	assert(result.matched == 0);

	assert(jsregex_exec_u_literal_sequence_utf16(pair_subject,
			sizeof(pair_subject) / sizeof(pair_subject[0]),
			NULL, 1, 0, &result) == -1);
	assert(errno == EINVAL);

	assert(jsregex_exec_u_literal_sequence_utf16(pair_subject,
			sizeof(pair_subject) / sizeof(pair_subject[0]),
			pair_pattern, 0, 0, &result) == -1);
	assert(errno == EINVAL);
}

static void
test_u_literal_class_exec(void)
{
	static const uint16_t pair_then_low_d[] = {
		'A', 0xD834, 0xDF06, 0xDF06, 'B', 'D'
	};
	static const uint16_t pair_then_high_c[] = {
		'A', 0xD834, 0xDF06, 0xD834, 'C'
	};
	static const uint16_t pair_only[] = {0xD834, 0xDF06};
	static const uint16_t low_d_members[] = {0xDF06, 'D'};
	static const uint16_t high_c_members[] = {0xD834, 'C'};
	static const uint16_t surrogate_members[] = {0xD834, 0xDF06};
	jsregex_exec_result_t result;
	jsregex_search_result_t search_result;

	assert(jsregex_exec_u_literal_class_utf16(pair_then_low_d,
			sizeof(pair_then_low_d) / sizeof(pair_then_low_d[0]),
			low_d_members,
			sizeof(low_d_members) / sizeof(low_d_members[0]), 0,
			&result) == 0);
	assert(result.matched == 1);
	assert(result.start == 3);
	assert(result.end == 4);
	assert(result.slot_count == 1);

	assert(jsregex_search_u_literal_class_utf16(pair_then_low_d,
			sizeof(pair_then_low_d) / sizeof(pair_then_low_d[0]),
			low_d_members,
			sizeof(low_d_members) / sizeof(low_d_members[0]), 4,
			&search_result) == 0);
	assert(search_result.matched == 1);
	assert(search_result.start == 5);
	assert(search_result.end == 6);

	assert(jsregex_search_u_literal_class_utf16(pair_then_high_c,
			sizeof(pair_then_high_c) / sizeof(pair_then_high_c[0]),
			high_c_members,
			sizeof(high_c_members) / sizeof(high_c_members[0]), 0,
			&search_result) == 0);
	assert(search_result.matched == 1);
	assert(search_result.start == 3);
	assert(search_result.end == 4);

	assert(jsregex_search_u_literal_class_utf16(pair_only,
			sizeof(pair_only) / sizeof(pair_only[0]), surrogate_members,
			sizeof(surrogate_members) / sizeof(surrogate_members[0]), 0,
			&search_result) == 0);
	assert(search_result.matched == 0);

	assert(jsregex_exec_u_literal_class_utf16(pair_then_low_d,
			sizeof(pair_then_low_d) / sizeof(pair_then_low_d[0]), NULL, 1, 0,
			&result) == -1);
	assert(errno == EINVAL);

	assert(jsregex_exec_u_literal_class_utf16(pair_then_low_d,
			sizeof(pair_then_low_d) / sizeof(pair_then_low_d[0]),
			low_d_members, 0, 0, &result) == -1);
	assert(errno == EINVAL);
}

static void
test_u_literal_negated_class_exec(void)
{
	static const uint16_t pair_then_low_b_d[] = {
		0xD834, 0xDF06, 0xDF06, 'B', 'D'
	};
	static const uint16_t pair_then_high_c_b[] = {
		0xD834, 0xDF06, 0xD834, 'C', 'B'
	};
	static const uint16_t pair_only[] = {0xD834, 0xDF06};
	static const uint16_t low_d_members[] = {0xDF06, 'D'};
	static const uint16_t high_c_members[] = {0xD834, 'C'};
	static const uint16_t surrogate_members[] = {0xD834, 0xDF06};
	jsregex_exec_result_t result;
	jsregex_search_result_t search_result;

	assert(jsregex_exec_u_literal_negated_class_utf16(pair_then_low_b_d,
			sizeof(pair_then_low_b_d) / sizeof(pair_then_low_b_d[0]),
			low_d_members,
			sizeof(low_d_members) / sizeof(low_d_members[0]), 0,
			&result) == 0);
	assert(result.matched == 1);
	assert(result.start == 3);
	assert(result.end == 4);
	assert(result.slot_count == 1);

	assert(jsregex_search_u_literal_negated_class_utf16(pair_then_high_c_b,
			sizeof(pair_then_high_c_b) / sizeof(pair_then_high_c_b[0]),
			high_c_members,
			sizeof(high_c_members) / sizeof(high_c_members[0]), 0,
			&search_result) == 0);
	assert(search_result.matched == 1);
	assert(search_result.start == 4);
	assert(search_result.end == 5);

	assert(jsregex_search_u_literal_negated_class_utf16(pair_only,
			sizeof(pair_only) / sizeof(pair_only[0]), surrogate_members,
			sizeof(surrogate_members) / sizeof(surrogate_members[0]), 0,
			&search_result) == 0);
	assert(search_result.matched == 0);

	assert(jsregex_exec_u_literal_negated_class_utf16(pair_then_low_b_d,
			sizeof(pair_then_low_b_d) / sizeof(pair_then_low_b_d[0]), NULL,
			1, 0, &result) == -1);
	assert(errno == EINVAL);

	assert(jsregex_exec_u_literal_negated_class_utf16(pair_then_low_b_d,
			sizeof(pair_then_low_b_d) / sizeof(pair_then_low_b_d[0]),
			low_d_members, 0, 0, &result) == -1);
	assert(errno == EINVAL);
}

static void
test_u_literal_range_class_exec(void)
{
	static const uint16_t pair_then_b_d_low[] = {
		'A', 0xD834, 0xDF06, 'B', 'D', 0xDF06, 'Z'
	};
	static const uint16_t pair_then_high_c[] = {
		'A', 0xD834, 0xDF06, 0xD834, 'C'
	};
	static const uint16_t pair_only[] = {0xD834, 0xDF06};
	static const uint16_t b_to_d_and_low_ranges[] = {
		'B', 'D', 0xDF06, 0xDF06
	};
	static const uint16_t high_range[] = {0xD834, 0xD834};
	static const uint16_t surrogate_range[] = {0xD800, 0xDFFF};
	static const uint16_t malformed_ranges[] = {'D', 'B'};
	jsregex_exec_result_t result;
	jsregex_search_result_t search_result;

	assert(jsregex_exec_u_literal_range_class_utf16(pair_then_b_d_low,
			sizeof(pair_then_b_d_low) / sizeof(pair_then_b_d_low[0]),
			b_to_d_and_low_ranges,
			sizeof(b_to_d_and_low_ranges) /
			(2 * sizeof(b_to_d_and_low_ranges[0])), 0, &result) == 0);
	assert(result.matched == 1);
	assert(result.start == 3);
	assert(result.end == 4);
	assert(result.slot_count == 1);

	assert(jsregex_search_u_literal_range_class_utf16(pair_then_b_d_low,
			sizeof(pair_then_b_d_low) / sizeof(pair_then_b_d_low[0]),
			b_to_d_and_low_ranges,
			sizeof(b_to_d_and_low_ranges) /
			(2 * sizeof(b_to_d_and_low_ranges[0])), 4,
			&search_result) == 0);
	assert(search_result.matched == 1);
	assert(search_result.start == 4);
	assert(search_result.end == 5);

	assert(jsregex_search_u_literal_range_class_utf16(pair_then_high_c,
			sizeof(pair_then_high_c) / sizeof(pair_then_high_c[0]),
			high_range, sizeof(high_range) / (2 * sizeof(high_range[0])), 0,
			&search_result) == 0);
	assert(search_result.matched == 1);
	assert(search_result.start == 3);
	assert(search_result.end == 4);

	assert(jsregex_search_u_literal_range_class_utf16(pair_only,
			sizeof(pair_only) / sizeof(pair_only[0]), surrogate_range,
			sizeof(surrogate_range) / (2 * sizeof(surrogate_range[0])), 0,
			&search_result) == 0);
	assert(search_result.matched == 0);

	assert(jsregex_exec_u_literal_range_class_utf16(pair_then_b_d_low,
			sizeof(pair_then_b_d_low) / sizeof(pair_then_b_d_low[0]),
			malformed_ranges,
			sizeof(malformed_ranges) / (2 * sizeof(malformed_ranges[0])), 0,
			&result) == -1);
	assert(errno == EINVAL);
}

static void
test_u_literal_negated_range_class_exec(void)
{
	static const uint16_t pair_a_b_d_low_z[] = {
		0xD834, 0xDF06, 'A', 'B', 'D', 0xDF06, 'Z'
	};
	static const uint16_t pair_then_high_c_b[] = {
		0xD834, 0xDF06, 0xD834, 'C', 'B'
	};
	static const uint16_t pair_only[] = {0xD834, 0xDF06};
	static const uint16_t b_to_d_and_low_ranges[] = {
		'B', 'D', 0xDF06, 0xDF06
	};
	static const uint16_t high_and_c_ranges[] = {
		0xD834, 0xD834, 'C', 'C'
	};
	static const uint16_t surrogate_range[] = {0xD800, 0xDFFF};
	static const uint16_t malformed_ranges[] = {'D', 'B'};
	jsregex_exec_result_t result;
	jsregex_search_result_t search_result;

	assert(jsregex_exec_u_literal_negated_range_class_utf16(
			pair_a_b_d_low_z,
			sizeof(pair_a_b_d_low_z) / sizeof(pair_a_b_d_low_z[0]),
			b_to_d_and_low_ranges,
			sizeof(b_to_d_and_low_ranges) /
			(2 * sizeof(b_to_d_and_low_ranges[0])), 0, &result) == 0);
	assert(result.matched == 1);
	assert(result.start == 2);
	assert(result.end == 3);
	assert(result.slot_count == 1);

	assert(jsregex_search_u_literal_negated_range_class_utf16(
			pair_then_high_c_b,
			sizeof(pair_then_high_c_b) / sizeof(pair_then_high_c_b[0]),
			high_and_c_ranges,
			sizeof(high_and_c_ranges) /
			(2 * sizeof(high_and_c_ranges[0])), 0, &search_result) == 0);
	assert(search_result.matched == 1);
	assert(search_result.start == 4);
	assert(search_result.end == 5);

	assert(jsregex_search_u_literal_negated_range_class_utf16(pair_only,
			sizeof(pair_only) / sizeof(pair_only[0]), surrogate_range,
			sizeof(surrogate_range) / (2 * sizeof(surrogate_range[0])), 0,
			&search_result) == 0);
	assert(search_result.matched == 0);

	assert(jsregex_exec_u_literal_negated_range_class_utf16(
			pair_a_b_d_low_z,
			sizeof(pair_a_b_d_low_z) / sizeof(pair_a_b_d_low_z[0]),
			malformed_ranges,
			sizeof(malformed_ranges) / (2 * sizeof(malformed_ranges[0])), 0,
			&result) == -1);
	assert(errno == EINVAL);
}

static void
test_u_predefined_class_exec(void)
{
	static const uint16_t pair_then_digits[] = {
		0xD834, 0xDF06, 'A', '1', '2', 'B'
	};
	static const uint16_t whitespace_subject[] = {
		'A', 0x00A0, 'B', 0x2028, 'C'
	};
	static const uint16_t word_subject[] = {
		0xD834, 0xDF06, '!', 'A', '1', '_', 0xD834, '?'
	};
	static const uint16_t pair_only[] = {0xD834, 0xDF06};
	jsregex_exec_result_t result;
	jsregex_search_result_t search_result;

	assert(jsregex_search_u_predefined_class_utf16(pair_then_digits,
			sizeof(pair_then_digits) / sizeof(pair_then_digits[0]),
			JSREGEX_U_PREDEFINED_CLASS_DIGIT, 0, &search_result) == 0);
	assert(search_result.matched == 1);
	assert(search_result.start == 3);
	assert(search_result.end == 4);

	assert(jsregex_search_u_predefined_class_utf16(pair_then_digits,
			sizeof(pair_then_digits) / sizeof(pair_then_digits[0]),
			JSREGEX_U_PREDEFINED_CLASS_NOT_DIGIT, 0, &search_result) == 0);
	assert(search_result.matched == 1);
	assert(search_result.start == 2);
	assert(search_result.end == 3);

	assert(jsregex_exec_u_predefined_class_utf16(whitespace_subject,
			sizeof(whitespace_subject) / sizeof(whitespace_subject[0]),
			JSREGEX_U_PREDEFINED_CLASS_WHITESPACE, 0, &result) == 0);
	assert(result.matched == 1);
	assert(result.start == 1);
	assert(result.end == 2);
	assert(result.slot_count == 1);

	assert(jsregex_search_u_predefined_class_utf16(whitespace_subject,
			sizeof(whitespace_subject) / sizeof(whitespace_subject[0]),
			JSREGEX_U_PREDEFINED_CLASS_WHITESPACE, 2,
			&search_result) == 0);
	assert(search_result.matched == 1);
	assert(search_result.start == 3);
	assert(search_result.end == 4);

	assert(jsregex_search_u_predefined_class_utf16(whitespace_subject,
			sizeof(whitespace_subject) / sizeof(whitespace_subject[0]),
			JSREGEX_U_PREDEFINED_CLASS_NOT_WHITESPACE, 0,
			&search_result) == 0);
	assert(search_result.matched == 1);
	assert(search_result.start == 0);
	assert(search_result.end == 1);

	assert(jsregex_search_u_predefined_class_utf16(word_subject,
			sizeof(word_subject) / sizeof(word_subject[0]),
			JSREGEX_U_PREDEFINED_CLASS_WORD, 0, &search_result) == 0);
	assert(search_result.matched == 1);
	assert(search_result.start == 3);
	assert(search_result.end == 4);

	assert(jsregex_search_u_predefined_class_utf16(word_subject,
			sizeof(word_subject) / sizeof(word_subject[0]),
			JSREGEX_U_PREDEFINED_CLASS_NOT_WORD, 0, &search_result) == 0);
	assert(search_result.matched == 1);
	assert(search_result.start == 2);
	assert(search_result.end == 3);

	assert(jsregex_search_u_predefined_class_utf16(word_subject,
			sizeof(word_subject) / sizeof(word_subject[0]),
			JSREGEX_U_PREDEFINED_CLASS_NOT_WORD, 3,
			&search_result) == 0);
	assert(search_result.matched == 1);
	assert(search_result.start == 6);
	assert(search_result.end == 7);

	assert(jsregex_search_u_predefined_class_utf16(pair_only,
			sizeof(pair_only) / sizeof(pair_only[0]),
			JSREGEX_U_PREDEFINED_CLASS_WORD, 0, &search_result) == 0);
	assert(search_result.matched == 0);

	assert(jsregex_search_u_predefined_class_utf16(pair_only,
			sizeof(pair_only) / sizeof(pair_only[0]),
			JSREGEX_U_PREDEFINED_CLASS_NOT_WORD, 0, &search_result) == 0);
	assert(search_result.matched == 0);

	assert(jsregex_exec_u_predefined_class_utf16(word_subject,
			sizeof(word_subject) / sizeof(word_subject[0]),
			(jsregex_u_predefined_class_kind_t)99, 0, &result) == -1);
	assert(errno == EINVAL);
}

int
main(void)
{
	test_regex8_compile_exec();
	test_regex_search();
	test_regex_compile_exec();
	test_regex_named_groups();
	test_u_literal_surrogate_exec();
	test_u_literal_sequence_exec();
	test_u_literal_class_exec();
	test_u_literal_negated_class_exec();
	test_u_literal_range_class_exec();
	test_u_literal_negated_range_class_exec();
	test_u_predefined_class_exec();
	return 0;
}

#else

int
main(void)
{
	return 0;
}

#endif
