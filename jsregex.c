#include "jsregex.h"

#if JSMX_WITH_REGEX

#include <errno.h>

#if JSMX_REGEX_BACKEND_PCRE2

#define PCRE2_CODE_UNIT_WIDTH 16
#include <pcre2.h>

typedef struct jsregex_options_s {
	uint32_t compile_options;
	uint32_t match_options;
} jsregex_options_t;

static int
jsregex_parse_flags(const uint16_t *flags, size_t flags_len,
		jsregex_options_t *options_ptr)
{
	jsregex_options_t options = {0};
	int seen_g = 0;
	int seen_i = 0;
	int seen_m = 0;
	int seen_s = 0;
	int seen_u = 0;
	int seen_y = 0;
	int seen_d = 0;
	size_t i;

	if (options_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}

	for (i = 0; i < flags_len; i++) {
		switch (flags[i]) {
		case 'g':
			if (seen_g) {
				errno = EINVAL;
				return -1;
			}
			seen_g = 1;
			break;
		case 'i':
			if (seen_i) {
				errno = EINVAL;
				return -1;
			}
			seen_i = 1;
			options.compile_options |= PCRE2_CASELESS;
			break;
		case 'm':
			if (seen_m) {
				errno = EINVAL;
				return -1;
			}
			seen_m = 1;
			options.compile_options |= PCRE2_MULTILINE;
			break;
		case 's':
			if (seen_s) {
				errno = EINVAL;
				return -1;
			}
			seen_s = 1;
			options.compile_options |= PCRE2_DOTALL;
			break;
		case 'u':
			if (seen_u) {
				errno = EINVAL;
				return -1;
			}
			seen_u = 1;
			options.compile_options |= PCRE2_UTF | PCRE2_UCP;
			break;
		case 'y':
			if (seen_y) {
				errno = EINVAL;
				return -1;
			}
			seen_y = 1;
			options.match_options |= PCRE2_ANCHORED;
			break;
		case 'd':
			if (seen_d) {
				errno = EINVAL;
				return -1;
			}
			seen_d = 1;
			break;
		default:
			errno = ENOTSUP;
			return -1;
		}
	}

	*options_ptr = options;
	return 0;
}

int
jsregex_search_utf16(const uint16_t *subject, size_t subject_len,
		const uint16_t *pattern, size_t pattern_len,
		const uint16_t *flags, size_t flags_len,
		jsregex_search_result_t *result_ptr)
{
	jsregex_options_t options;
	pcre2_code *code = NULL;
	pcre2_match_data *match_data = NULL;
	PCRE2_SIZE *ovector;
	int error_code;
	PCRE2_SIZE error_offset;
	int rc;

	if ((subject_len > 0 && subject == NULL) ||
			(pattern_len > 0 && pattern == NULL) ||
			(flags_len > 0 && flags == NULL) ||
			result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsregex_parse_flags(flags, flags_len, &options) < 0) {
		return -1;
	}

	code = pcre2_compile((PCRE2_SPTR16)pattern, pattern_len,
			options.compile_options, &error_code, &error_offset, NULL);
	if (code == NULL) {
		(void)error_code;
		(void)error_offset;
		errno = EINVAL;
		return -1;
	}

	match_data = pcre2_match_data_create_from_pattern(code, NULL);
	if (match_data == NULL) {
		pcre2_code_free(code);
		errno = ENOMEM;
		return -1;
	}

	rc = pcre2_match(code, (PCRE2_SPTR16)subject, subject_len, 0,
			options.match_options, match_data, NULL);
	if (rc == PCRE2_ERROR_NOMATCH) {
		result_ptr->matched = 0;
		result_ptr->start = 0;
		result_ptr->end = 0;
		pcre2_match_data_free(match_data);
		pcre2_code_free(code);
		return 0;
	}
	if (rc < 0) {
		pcre2_match_data_free(match_data);
		pcre2_code_free(code);
		errno = EINVAL;
		return -1;
	}

	ovector = pcre2_get_ovector_pointer(match_data);
	result_ptr->matched = 1;
	result_ptr->start = (size_t)ovector[0];
	result_ptr->end = (size_t)ovector[1];

	pcre2_match_data_free(match_data);
	pcre2_code_free(code);
	return 0;
}

#else

#error "JSMX_WITH_REGEX requires a configured regex backend"

#endif

#endif
