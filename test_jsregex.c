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

int
main(void)
{
	test_regex_search();
	test_regex_compile_exec();
	return 0;
}

#else

int
main(void)
{
	return 0;
}

#endif
