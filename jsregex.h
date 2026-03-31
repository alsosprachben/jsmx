#ifndef JSREGEX_H
#define JSREGEX_H

#include <stddef.h>
#include <stdint.h>

#include "jsmx_config.h"

#if JSMX_WITH_REGEX

typedef struct jsregex_search_result_s {
	int matched;
	size_t start;
	size_t end;
} jsregex_search_result_t;

int jsregex_search_utf16(const uint16_t *subject, size_t subject_len,
		const uint16_t *pattern, size_t pattern_len,
		const uint16_t *flags, size_t flags_len,
		jsregex_search_result_t *result_ptr);

#endif

#endif
