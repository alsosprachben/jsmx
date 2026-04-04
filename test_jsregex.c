#include <assert.h>
#include <errno.h>
#include <stdint.h>

#include "jsregex.h"

#if JSMX_WITH_REGEX

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

int
main(void)
{
	test_regex_search();
	test_regex_compile_exec();
	test_u_literal_surrogate_exec();
	test_u_literal_sequence_exec();
	test_u_literal_class_exec();
	test_u_literal_negated_class_exec();
	return 0;
}

#else

int
main(void)
{
	return 0;
}

#endif
