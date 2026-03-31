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

int
main(void)
{
	test_regex_search();
	return 0;
}

#else

int
main(void)
{
	return 0;
}

#endif
