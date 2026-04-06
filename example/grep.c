#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jsregex.h"
#include "utf8.h"

/*
 * Source JS: example/grep.js
 * Behavior: grep-like CLI over stdin and synchronous text files with JS regex
 * syntax, -F fixed mode, and common output-selection flags.
 * Lowering: production_slow_path
 * Host assumptions: argv, stdin/stdout/stderr, and sync text file reads are
 * bridged directly through libc; regex matching uses the jsmx regex backend.
 *
 * Local build example:
 * cc -I. -DJSMX_WITH_REGEX=1 -DJSMX_REGEX_BACKEND_PCRE2=1 \
 *   example/grep.c jsregex.c \
 *   -lpcre2-16 \
 *   -o /tmp/jsmx-grep
 *
 * Add local PCRE2 include/library search paths as needed.
 */

#if !JSMX_WITH_REGEX
#error "example/grep.c requires JSMX_WITH_REGEX=1"
#endif

typedef struct grep_options_s {
	int fixed;
	int ignore_case;
	int invert;
	int line_numbers;
	int count_only;
	int quiet;
	int force_filename;
	int no_filename;
	int suppress_errors;
	int line_regexp;
	size_t pattern_count;
	const char **patterns;
	size_t file_count;
	const char **files;
} grep_options_t;

typedef struct grep_matcher_s {
	jsregex_compiled_t compiled;
} grep_matcher_t;

typedef struct grep_source_result_s {
	size_t selected_count;
	int quiet_match;
} grep_source_result_t;

static const char *STDIN_LABEL = "(standard input)";

static const char *
grep_prog_name(const char *path)
{
	const char *slash;

	if (path == NULL) {
		return "grep";
	}
	slash = strrchr(path, '/');
	return slash != NULL ? slash + 1 : path;
}

static int
grep_usage(FILE *stream, const char *prog)
{
	fprintf(stream, "usage: %s [-FivncqHhsx] [-e pattern] [pattern] [file ...]\n",
			prog);
	return 2;
}

static void
grep_options_init(grep_options_t *options)
{
	memset(options, 0, sizeof(*options));
}

static void
grep_options_release(grep_options_t *options)
{
	free(options->patterns);
	free(options->files);
	memset(options, 0, sizeof(*options));
}

static int
grep_parse_args(int argc, char **argv, grep_options_t *options,
		const char *prog)
{
	int parse_options = 1;
	int i;

	options->patterns = calloc((size_t)argc ? (size_t)argc : 1u,
			sizeof(*options->patterns));
	options->files = calloc((size_t)argc ? (size_t)argc : 1u,
			sizeof(*options->files));
	if (options->patterns == NULL || options->files == NULL) {
		fprintf(stderr, "%s: out of memory\n", prog);
		return 2;
	}

	for (i = 1; i < argc; i++) {
		const char *arg = argv[i];

		if (parse_options && strcmp(arg, "--") == 0) {
			parse_options = 0;
			continue;
		}
		if (parse_options && arg[0] == '-' && arg[1] != '\0') {
			size_t j;

			if (strcmp(arg, "-e") == 0) {
				i++;
				if (i >= argc) {
					return grep_usage(stderr, prog);
				}
				options->patterns[options->pattern_count++] = argv[i];
				continue;
			}
			for (j = 1; arg[j] != '\0'; j++) {
				char flag = arg[j];

				if (flag == 'e') {
					if (arg[j + 1] != '\0') {
						options->patterns[options->pattern_count++] = arg + j + 1;
					} else {
						i++;
						if (i >= argc) {
							return grep_usage(stderr, prog);
						}
						options->patterns[options->pattern_count++] = argv[i];
					}
					break;
				}
				switch (flag) {
				case 'F':
					options->fixed = 1;
					break;
				case 'i':
					options->ignore_case = 1;
					break;
				case 'v':
					options->invert = 1;
					break;
				case 'n':
					options->line_numbers = 1;
					break;
				case 'c':
					options->count_only = 1;
					break;
				case 'q':
					options->quiet = 1;
					break;
				case 'H':
					options->force_filename = 1;
					break;
				case 'h':
					options->no_filename = 1;
					break;
				case 's':
					options->suppress_errors = 1;
					break;
				case 'x':
					options->line_regexp = 1;
					break;
				default:
					return grep_usage(stderr, prog);
				}
			}
			continue;
		}
		if (options->pattern_count == 0) {
			options->patterns[options->pattern_count++] = arg;
		} else {
			options->files[options->file_count++] = arg;
		}
	}

	if (options->pattern_count == 0) {
		return grep_usage(stderr, prog);
	}
	if (options->file_count == 0) {
		options->files[options->file_count++] = "-";
	}
	return 0;
}

static int
grep_should_show_filename(const grep_options_t *options)
{
	if (options->no_filename) {
		return 0;
	}
	if (options->force_filename) {
		return 1;
	}
	return options->file_count > 1;
}

static const char *
grep_source_label(const char *path)
{
	return strcmp(path, "-") == 0 ? STDIN_LABEL : path;
}

static int
grep_slurp_stream(FILE *stream, uint8_t **buf_ptr, size_t *len_ptr)
{
	uint8_t *buf = NULL;
	size_t len = 0;
	size_t cap = 0;

	for (;;) {
		size_t chunk;

		if (len == cap) {
			size_t new_cap = cap == 0 ? 4096u : cap * 2u;
			uint8_t *new_buf = realloc(buf, new_cap);

			if (new_buf == NULL) {
				free(buf);
				errno = ENOMEM;
				return -1;
			}
			buf = new_buf;
			cap = new_cap;
		}
		chunk = fread(buf + len, 1, cap - len, stream);
		len += chunk;
		if (chunk == 0) {
			if (ferror(stream)) {
				free(buf);
				return -1;
			}
			break;
		}
	}

	*buf_ptr = buf;
	*len_ptr = len;
	return 0;
}

static int
grep_slurp_path(const char *path, uint8_t **buf_ptr, size_t *len_ptr)
{
	FILE *stream;
	int rc;

	if (strcmp(path, "-") == 0) {
		return grep_slurp_stream(stdin, buf_ptr, len_ptr);
	}

	stream = fopen(path, "rb");
	if (stream == NULL) {
		return -1;
	}
	rc = grep_slurp_stream(stream, buf_ptr, len_ptr);
	fclose(stream);
	return rc;
}

static int
grep_regex_escape_literal(const char *pattern, size_t pattern_len,
		char **out_ptr, size_t *out_len_ptr)
{
	static const char meta[] = "\\^$.*+?()[]{}|";
	size_t i;
	size_t out_len = 0;
	char *out;

	for (i = 0; i < pattern_len; i++) {
		if (strchr(meta, pattern[i]) != NULL) {
			out_len++;
		}
		out_len++;
	}
	out = malloc(out_len ? out_len : 1u);
	if (out == NULL) {
		errno = ENOMEM;
		return -1;
	}
	out_len = 0;
	for (i = 0; i < pattern_len; i++) {
		if (strchr(meta, pattern[i]) != NULL) {
			out[out_len++] = '\\';
		}
		out[out_len++] = pattern[i];
	}
	*out_ptr = out;
	*out_len_ptr = out_len;
	return 0;
}

static int
grep_build_pattern_source(const grep_options_t *options, const char *pattern,
		char **out_ptr, size_t *out_len_ptr)
{
	char *body = NULL;
	size_t body_len = 0;
	char *out;
	size_t out_len = 0;

	if (options->fixed) {
		if (grep_regex_escape_literal(pattern, strlen(pattern), &body,
				&body_len) < 0) {
			return -1;
		}
	} else {
		body_len = strlen(pattern);
		body = malloc(body_len ? body_len : 1u);
		if (body == NULL) {
			errno = ENOMEM;
			return -1;
		}
		if (body_len > 0) {
			memcpy(body, pattern, body_len);
		}
	}

	if (options->line_regexp) {
		static const char prefix[] = "^(?:";
		static const char suffix[] = ")$";

		out_len = sizeof(prefix) - 1 + body_len + sizeof(suffix) - 1;
		out = malloc(out_len ? out_len : 1u);
		if (out == NULL) {
			free(body);
			errno = ENOMEM;
			return -1;
		}
		memcpy(out, prefix, sizeof(prefix) - 1);
		memcpy(out + sizeof(prefix) - 1, body, body_len);
		memcpy(out + sizeof(prefix) - 1 + body_len, suffix, sizeof(suffix) - 1);
	} else {
		out = body;
		out_len = body_len;
		body = NULL;
	}

	free(body);
	*out_ptr = out;
	*out_len_ptr = out_len;
	return 0;
}

static int
grep_utf8_to_utf16(const uint8_t *src, size_t src_len, uint16_t **buf_ptr,
		size_t *len_ptr)
{
	const uint8_t *cursor = src;
	const uint8_t *stop = src + src_len;
	uint16_t *buf;
	uint16_t *out;

	buf = malloc(sizeof(uint16_t) * (src_len ? src_len : 1u));
	if (buf == NULL) {
		errno = ENOMEM;
		return -1;
	}
	out = buf;
	while (cursor < stop) {
		uint32_t codepoint = 0;
		int width = 0;

		UTF8_CHAR(cursor, stop, &codepoint, &width);
		if (width == 0) {
			codepoint = 0xFFFDu;
			width = 1;
		} else if (width < 0) {
			codepoint = 0xFFFDu;
			width = -width;
		}
		cursor += width;
		if (codepoint < 0x10000u) {
			*out++ = (uint16_t)codepoint;
		} else {
			codepoint -= 0x10000u;
			*out++ = (uint16_t)(0xD800u | ((codepoint >> 10) & 0x3FFu));
			*out++ = (uint16_t)(0xDC00u | (codepoint & 0x3FFu));
		}
	}

	*buf_ptr = buf;
	*len_ptr = (size_t)(out - buf);
	return 0;
}

static int
grep_compile_matchers(const grep_options_t *options, const char *prog,
		grep_matcher_t **matchers_ptr)
{
	grep_matcher_t *matchers;
	size_t i;
	uint16_t flag_units[1];
	size_t flag_len = 0;

	if (options->ignore_case) {
		flag_units[0] = (uint16_t)'i';
		flag_len = 1;
	}

	matchers = calloc(options->pattern_count, sizeof(*matchers));
	if (matchers == NULL) {
		fprintf(stderr, "%s: out of memory\n", prog);
		return 2;
	}

	for (i = 0; i < options->pattern_count; i++) {
		char *pattern_utf8 = NULL;
		size_t pattern_utf8_len = 0;
		uint16_t *pattern_utf16 = NULL;
		size_t pattern_utf16_len = 0;

		if (grep_build_pattern_source(options, options->patterns[i],
				&pattern_utf8, &pattern_utf8_len) < 0) {
			fprintf(stderr, "%s: out of memory\n", prog);
			goto fail;
		}
		if (grep_utf8_to_utf16((const uint8_t *)pattern_utf8, pattern_utf8_len,
				&pattern_utf16, &pattern_utf16_len) < 0) {
			fprintf(stderr, "%s: out of memory\n", prog);
			free(pattern_utf8);
			goto fail;
		}
		if (jsregex_compile_utf16_jit(pattern_utf16, pattern_utf16_len,
				flag_len > 0 ? flag_units : NULL, flag_len,
				&matchers[i].compiled) < 0) {
			fprintf(stderr, "%s: invalid regex '%s': %s\n", prog,
					options->patterns[i], strerror(errno));
			free(pattern_utf8);
			free(pattern_utf16);
			goto fail;
		}
		free(pattern_utf8);
		free(pattern_utf16);
	}

	*matchers_ptr = matchers;
	return 0;

fail:
	while (i > 0) {
		i--;
		jsregex_release(&matchers[i].compiled);
	}
	free(matchers);
	return 2;
}

static void
grep_release_matchers(grep_matcher_t *matchers, size_t pattern_count)
{
	size_t i;

	if (matchers == NULL) {
		return;
	}
	for (i = 0; i < pattern_count; i++) {
		jsregex_release(&matchers[i].compiled);
	}
	free(matchers);
}

static int
grep_line_matches(const grep_options_t *options, const grep_matcher_t *matchers,
		const uint8_t *line, size_t line_len, int *matched_ptr)
{
	uint16_t *line_utf16 = NULL;
	size_t line_utf16_len = 0;
	size_t i;

	if (grep_utf8_to_utf16(line, line_len, &line_utf16, &line_utf16_len) < 0) {
		return -1;
	}
	*matched_ptr = 0;
	for (i = 0; i < options->pattern_count; i++) {
		jsregex_exec_result_t exec_result;
		size_t offsets[2];

		if (jsregex_exec_utf16(&matchers[i].compiled, line_utf16, line_utf16_len,
				0, offsets, 2, &exec_result) < 0) {
			free(line_utf16);
			return -1;
		}
		if (exec_result.matched) {
			*matched_ptr = 1;
			break;
		}
	}
	free(line_utf16);
	return 0;
}

static int
grep_emit_line(const char *label, int show_filename, int line_numbers,
		size_t line_number, const uint8_t *line, size_t line_len)
{
	if (show_filename && fprintf(stdout, "%s:", label) < 0) {
		return -1;
	}
	if (line_numbers && fprintf(stdout, "%zu:", line_number) < 0) {
		return -1;
	}
	if (line_len > 0 && fwrite(line, 1, line_len, stdout) != line_len) {
		return -1;
	}
	if (fputc('\n', stdout) == EOF) {
		return -1;
	}
	return 0;
}

static int
grep_emit_count(const char *label, int show_filename, size_t count)
{
	if (show_filename) {
		return fprintf(stdout, "%s:%zu\n", label, count) < 0 ? -1 : 0;
	}
	return fprintf(stdout, "%zu\n", count) < 0 ? -1 : 0;
}

static int
grep_process_text(const grep_options_t *options, const grep_matcher_t *matchers,
		const char *label, int show_filename, const uint8_t *text,
		size_t text_len, grep_source_result_t *result_ptr)
{
	size_t start = 0;
	size_t line_number = 1;
	size_t i;

	memset(result_ptr, 0, sizeof(*result_ptr));
	for (i = 0; i <= text_len; i++) {
		int is_break = (i == text_len) || (text[i] == '\n');

		if (!is_break) {
			continue;
		}
		if (i < text_len || start < text_len) {
			int matched = 0;
			int selected;

			if (grep_line_matches(options, matchers, text + start, i - start,
					&matched) < 0) {
				return -1;
			}
			selected = options->invert ? !matched : matched;
			if (selected) {
				result_ptr->selected_count++;
				if (options->quiet) {
					result_ptr->quiet_match = 1;
					return 0;
				}
				if (!options->count_only &&
						grep_emit_line(label, show_filename,
							options->line_numbers, line_number,
							text + start, i - start) < 0) {
					return -1;
				}
			}
		}
		if (i < text_len) {
			start = i + 1;
			line_number++;
		}
	}
	if (options->count_only && !options->quiet) {
		if (grep_emit_count(label, show_filename,
				result_ptr->selected_count) < 0) {
			return -1;
		}
	}
	return 0;
}

int
main(int argc, char **argv)
{
	const char *prog = grep_prog_name(argc > 0 ? argv[0] : NULL);
	grep_options_t options;
	grep_matcher_t *matchers = NULL;
	int show_filename;
	int had_selected = 0;
	int had_error = 0;
	size_t i;
	int rc;

	grep_options_init(&options);
	rc = grep_parse_args(argc, argv, &options, prog);
	if (rc != 0) {
		grep_options_release(&options);
		return rc;
	}
	rc = grep_compile_matchers(&options, prog, &matchers);
	if (rc != 0) {
		grep_options_release(&options);
		return rc;
	}

	show_filename = grep_should_show_filename(&options);
	for (i = 0; i < options.file_count; i++) {
		const char *path = options.files[i];
		uint8_t *text = NULL;
		size_t text_len = 0;
		grep_source_result_t source_result;

		if (grep_slurp_path(path, &text, &text_len) < 0) {
			had_error = 1;
			if (!options.suppress_errors) {
				fprintf(stderr, "%s: %s: %s\n", prog, path, strerror(errno));
			}
			continue;
		}
		if (grep_process_text(&options, matchers, grep_source_label(path),
				show_filename, text, text_len, &source_result) < 0) {
			free(text);
			fprintf(stderr, "%s: processing failed: %s\n", prog,
					strerror(errno));
			grep_release_matchers(matchers, options.pattern_count);
			grep_options_release(&options);
			return 2;
		}
		free(text);
		if (source_result.selected_count > 0) {
			had_selected = 1;
		}
		if (source_result.quiet_match) {
			grep_release_matchers(matchers, options.pattern_count);
			grep_options_release(&options);
			return 0;
		}
	}

	grep_release_matchers(matchers, options.pattern_count);
	grep_options_release(&options);
	if (had_error) {
		return 2;
	}
	return had_selected ? 0 : 1;
}
