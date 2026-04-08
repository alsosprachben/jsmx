#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "runtime_modules/shared/fs_sync.h"
#include "jsregex.h"
#include "jsval.h"

/*
 * Source JS: example/grep.js
 * Behavior: grep-like CLI over stdin, files, and recursive directories with JS
 * regex syntax, -F fixed mode, and common output-selection flags.
 * Lowering: production_slow_path
 * Host assumptions: argv, stdin/stdout/stderr, and the reusable sync fs
 * runtime module are bridged directly through libc; regex matching uses the
 * jsmx regex backend.
 *
 * Local build example:
 * cc -I. -DJSMX_WITH_REGEX=1 -DJSMX_REGEX_BACKEND_PCRE2=1 \
 *   example/grep.c jsnum.c jsval.c jsmethod.c jsregex.c jsmn.c jsurl.c \
 *   jsstr.c unicode.c \
 *   -lpcre2-8 -lpcre2-16 \
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
	int recursive;
	int recursive_follow;
	int binary_files;
	size_t pattern_count;
	const char **patterns;
	size_t file_count;
	const char **files;
} grep_options_t;

typedef enum grep_binary_files_mode_e {
	GREP_BINARY_FILES_BINARY = 0,
	GREP_BINARY_FILES_TEXT = 1,
	GREP_BINARY_FILES_WITHOUT_MATCH = 2
} grep_binary_files_mode_t;

typedef struct grep_matcher_s {
	jsregex8_compiled_t compiled;
} grep_matcher_t;

typedef struct grep_source_result_s {
	size_t selected_count;
	int quiet_match;
} grep_source_result_t;

typedef struct grep_js_state_s {
	uint8_t *buf;
	size_t cap;
	jsval_region_t region;
	jsval_t sources;
	jsval_t visited;
} grep_js_state_t;

static const char *STDIN_LABEL = "(standard input)";
static const char *PROGRAM_LABEL = "grep.js";

static const char *
grep_prog_name(const char *path)
{
	(void)path;
	return PROGRAM_LABEL;
}

static int
grep_usage(FILE *stream, const char *prog)
{
	fprintf(stream,
			"usage: %s [-FIaivncqHhRrsx] [--binary-files=TYPE] [-e pattern] [pattern] [file ...]\n",
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
grep_parse_binary_files_value(const char *value, int *mode_ptr)
{
	if (strcmp(value, "binary") == 0) {
		*mode_ptr = GREP_BINARY_FILES_BINARY;
		return 0;
	}
	if (strcmp(value, "text") == 0) {
		*mode_ptr = GREP_BINARY_FILES_TEXT;
		return 0;
	}
	if (strcmp(value, "without-match") == 0) {
		*mode_ptr = GREP_BINARY_FILES_WITHOUT_MATCH;
		return 0;
	}
	return -1;
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

			if (strncmp(arg, "--binary-files=", 15) == 0) {
				if (grep_parse_binary_files_value(arg + 15,
						&options->binary_files) < 0) {
					return grep_usage(stderr, prog);
				}
				continue;
			}

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
				case 'I':
					options->binary_files = GREP_BINARY_FILES_WITHOUT_MATCH;
					break;
				case 'a':
					options->binary_files = GREP_BINARY_FILES_TEXT;
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
				case 'r':
					options->recursive = 1;
					break;
				case 'R':
					options->recursive = 1;
					options->recursive_follow = 1;
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

static const char *
grep_source_label(const char *path)
{
	return strcmp(path, "-") == 0 ? STDIN_LABEL : path;
}

static int
grep_kind_is_searchable(runtime_fs_kind_t kind)
{
	return kind == RUNTIME_FS_KIND_FILE;
}

static int
grep_js_state_grow(grep_js_state_t *state)
{
	size_t new_cap;
	uint8_t *new_buf;

	if (state == NULL) {
		errno = EINVAL;
		return -1;
	}
	new_cap = state->cap == 0 ? 65536u : state->cap * 2u;
	if (new_cap <= state->cap) {
		errno = ENOMEM;
		return -1;
	}
	new_buf = realloc(state->buf, new_cap);
	if (new_buf == NULL) {
		errno = ENOMEM;
		return -1;
	}
	state->buf = new_buf;
	state->cap = new_cap;
	jsval_region_rebase(&state->region, state->buf, state->cap);
	return 0;
}

static int
grep_js_state_init(grep_js_state_t *state, size_t source_cap,
		size_t visited_cap)
{
	size_t initial_cap = 65536u;

	if (state == NULL) {
		errno = EINVAL;
		return -1;
	}
	memset(state, 0, sizeof(*state));
	state->buf = malloc(initial_cap);
	if (state->buf == NULL) {
		errno = ENOMEM;
		return -1;
	}
	state->cap = initial_cap;
	jsval_region_init(&state->region, state->buf, state->cap);
	source_cap = source_cap > 0 ? source_cap : 4u;
	visited_cap = visited_cap > 0 ? visited_cap : 4u;
	for (;;) {
		if (jsval_array_new(&state->region, source_cap, &state->sources) == 0) {
			break;
		}
		if (errno != ENOBUFS || grep_js_state_grow(state) < 0) {
			return -1;
		}
	}
	for (;;) {
		if (jsval_set_new(&state->region, visited_cap, &state->visited) == 0) {
			break;
		}
		if (errno != ENOBUFS || grep_js_state_grow(state) < 0) {
			return -1;
		}
	}
	return 0;
}

static void
grep_js_state_free(grep_js_state_t *state)
{
	if (state == NULL) {
		return;
	}
	free(state->buf);
	memset(state, 0, sizeof(*state));
}

static int
grep_js_string_new(grep_js_state_t *state, const char *text, jsval_t *value_ptr)
{
	size_t len;

	if (state == NULL || text == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	len = strlen(text);
	for (;;) {
		if (jsval_string_new_utf8(&state->region, (const uint8_t *)text, len,
				value_ptr) == 0) {
			return 0;
		}
		if (errno != ENOBUFS || grep_js_state_grow(state) < 0) {
			return -1;
		}
	}
}

static int
grep_js_array_grow(grep_js_state_t *state, jsval_t *array_ptr)
{
	size_t len;
	size_t new_cap;

	if (state == NULL || array_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	len = jsval_array_length(&state->region, *array_ptr);
	new_cap = len < 4 ? 4 : (len * 2);
	if (new_cap <= len) {
		new_cap = len + 1;
	}
	for (;;) {
		jsval_t grown;

		if (jsval_array_clone_dense(&state->region, *array_ptr, new_cap,
				&grown) == 0) {
			*array_ptr = grown;
			return 0;
		}
		if (errno != ENOBUFS || grep_js_state_grow(state) < 0) {
			return -1;
		}
	}
}

static int
grep_js_set_grow(grep_js_state_t *state, jsval_t *set_ptr)
{
	size_t len;
	size_t new_cap;

	if (state == NULL || set_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_set_size(&state->region, *set_ptr, &len) < 0) {
		return -1;
	}
	new_cap = len < 4 ? 4 : (len * 2);
	if (new_cap <= len) {
		new_cap = len + 1;
	}
	for (;;) {
		jsval_t grown;

		if (jsval_set_clone(&state->region, *set_ptr, new_cap, &grown) == 0) {
			*set_ptr = grown;
			return 0;
		}
		if (errno != ENOBUFS || grep_js_state_grow(state) < 0) {
			return -1;
		}
	}
}

static int
grep_js_sources_append(grep_js_state_t *state, const char *path)
{
	jsval_t value;

	if (grep_js_string_new(state, path, &value) < 0) {
		return -1;
	}
	for (;;) {
		if (jsval_array_push(&state->region, state->sources, value) == 0) {
			return 0;
		}
		if (errno != ENOBUFS || grep_js_array_grow(state, &state->sources) < 0) {
			return -1;
		}
	}
}

static int
grep_js_visited_add(grep_js_state_t *state, const char *realpath_value,
		int *added_ptr)
{
	jsval_t value;
	size_t before;
	size_t after;

	if (state == NULL || realpath_value == NULL || added_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (grep_js_string_new(state, realpath_value, &value) < 0) {
		return -1;
	}
	if (jsval_set_size(&state->region, state->visited, &before) < 0) {
		return -1;
	}
	for (;;) {
		if (jsval_set_add(&state->region, state->visited, value) == 0) {
			break;
		}
		if (errno != ENOBUFS || grep_js_set_grow(state, &state->visited) < 0) {
			return -1;
		}
	}
	if (jsval_set_size(&state->region, state->visited, &after) < 0) {
		return -1;
	}
	*added_ptr = after != before;
	return 0;
}

static const char *
grep_errno_text(int err)
{
	return strerror(err);
}

static int
grep_report_path_error(const grep_options_t *options, const char *prog,
		const char *path, int err)
{
	if (!options->suppress_errors) {
		fprintf(stderr, "%s: %s: %s\n", prog, path, grep_errno_text(err));
	}
	return 0;
}

static int
grep_expand_path(const grep_options_t *options, const char *prog,
		const char *path, int nested, grep_js_state_t *state,
		int *had_error_ptr);

static int
grep_expand_directory(const grep_options_t *options, const char *prog,
		const char *path, grep_js_state_t *state, int *had_error_ptr)
{
	runtime_fs_dir_list_t entries = {0};
	size_t i;

	if (runtime_fs_list_dir(path, &entries) < 0) {
		*had_error_ptr = 1;
		grep_report_path_error(options, prog, path, errno);
		return 0;
	}
	for (i = 0; i < entries.count; i++) {
		runtime_fs_dir_entry_t *entry = &entries.entries[i];
		runtime_fs_kind_t effective_kind =
			entry->is_symlink ? entry->target_kind : entry->kind;

		if (effective_kind == RUNTIME_FS_KIND_DIRECTORY) {
			if (entry->is_symlink && !options->recursive_follow) {
				continue;
			}
			if (grep_expand_path(options, prog, entry->path, 1, state,
					had_error_ptr) < 0) {
				runtime_fs_dir_list_free(&entries);
				return -1;
			}
		} else if (grep_kind_is_searchable(effective_kind)) {
			if (grep_js_sources_append(state, entry->path) < 0) {
				runtime_fs_dir_list_free(&entries);
				return -1;
			}
		}
	}
	runtime_fs_dir_list_free(&entries);
	return 0;
}

static int
grep_expand_path(const grep_options_t *options, const char *prog,
		const char *path, int nested, grep_js_state_t *state,
		int *had_error_ptr)
{
	runtime_fs_path_info_t info;
	runtime_fs_kind_t effective_kind;

	if (strcmp(path, "-") == 0) {
		return grep_js_sources_append(state, path);
	}
	if (runtime_fs_classify_path(path, &info) < 0) {
		*had_error_ptr = 1;
		grep_report_path_error(options, prog, path, errno);
		return 0;
	}

	effective_kind = info.is_symlink ? info.target_kind : info.kind;
	if (effective_kind == RUNTIME_FS_KIND_DIRECTORY) {
		if (!options->recursive) {
			*had_error_ptr = 1;
			grep_report_path_error(options, prog, path, EISDIR);
			return 0;
		}
		if (info.is_symlink && !options->recursive_follow) {
			if (!nested) {
				*had_error_ptr = 1;
				grep_report_path_error(options, prog, path, EISDIR);
			}
			return 0;
		}
		if (options->recursive_follow) {
			char *resolved = NULL;
			int added = 0;

			if (runtime_fs_realpath_copy(path, &resolved) < 0) {
				*had_error_ptr = 1;
				grep_report_path_error(options, prog, path, errno);
				return 0;
			}
			if (grep_js_visited_add(state, resolved, &added) < 0) {
				free(resolved);
				return -1;
			}
			free(resolved);
			if (!added) {
				return 0;
			}
		}
		return grep_expand_directory(options, prog, path, state, had_error_ptr);
	}
	if (!grep_kind_is_searchable(effective_kind)) {
		if (!nested) {
			*had_error_ptr = 1;
			grep_report_path_error(options, prog, path, EINVAL);
		}
		return 0;
	}

	return grep_js_sources_append(state, path);
}

static int
grep_expand_sources(const grep_options_t *options, const char *prog,
		grep_js_state_t *state, int *had_error_ptr)
{
	size_t i;
	int rc = 0;

	for (i = 0; i < options->file_count; i++) {
		if (grep_expand_path(options, prog, options->files[i], 0, state,
				had_error_ptr) < 0) {
			rc = -1;
			break;
		}
	}
	return rc;
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
	if (strcmp(path, "-") == 0) {
		return grep_slurp_stream(stdin, buf_ptr, len_ptr);
	}
	return runtime_fs_read_text_file(path, buf_ptr, len_ptr);
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
grep_compile_matchers(const grep_options_t *options, const char *prog,
		grep_matcher_t **matchers_ptr)
{
	grep_matcher_t *matchers;
	size_t i;
	uint8_t flag_units[1];
	size_t flag_len = 0;

	if (options->ignore_case) {
		flag_units[0] = (uint8_t)'i';
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

		if (grep_build_pattern_source(options, options->patterns[i],
				&pattern_utf8, &pattern_utf8_len) < 0) {
			fprintf(stderr, "%s: out of memory\n", prog);
			goto fail;
		}
		if (jsregex8_compile_utf8_jit((const uint8_t *)pattern_utf8,
				pattern_utf8_len,
				flag_len > 0 ? flag_units : NULL, flag_len,
				&matchers[i].compiled) < 0) {
			fprintf(stderr, "%s: invalid regex '%s': %s\n", prog,
					options->patterns[i], strerror(errno));
			free(pattern_utf8);
			goto fail;
		}
		free(pattern_utf8);
	}

	*matchers_ptr = matchers;
	return 0;

fail:
	while (i > 0) {
		i--;
		jsregex8_release(&matchers[i].compiled);
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
		jsregex8_release(&matchers[i].compiled);
	}
	free(matchers);
}

static int
grep_line_matches(const grep_options_t *options, const grep_matcher_t *matchers,
		const uint8_t *line, size_t line_len, int *matched_ptr)
{
	size_t i;

	*matched_ptr = 0;
	for (i = 0; i < options->pattern_count; i++) {
		jsregex8_exec_result_t exec_result;
		size_t offsets[2];

		if (jsregex8_exec_utf8(&matchers[i].compiled, line, line_len,
				0, offsets, 2, &exec_result) < 0) {
			return -1;
		}
		if (exec_result.matched) {
			*matched_ptr = 1;
			break;
		}
	}
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
grep_emit_binary_match(const char *prog, const char *label)
{
	return fprintf(stderr, "%s: %s: binary file matches\n", prog, label) < 0
		? -1 : 0;
}

static int
grep_buffer_has_nul(const uint8_t *text, size_t text_len)
{
	size_t i;

	for (i = 0; i < text_len; i++) {
		if (text[i] == 0) {
			return 1;
		}
	}
	return 0;
}

static int
grep_process_text(const grep_options_t *options, const grep_matcher_t *matchers,
		const char *label, int show_filename, const uint8_t *text,
		size_t text_len, int emit_lines, grep_source_result_t *result_ptr)
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
				if (emit_lines && !options->count_only &&
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

static int
grep_process_source(const grep_options_t *options, const grep_matcher_t *matchers,
		const char *prog, const char *label, int show_filename,
		const uint8_t *text, size_t text_len, grep_source_result_t *result_ptr)
{
	int is_binary = grep_buffer_has_nul(text, text_len);

	if (is_binary && options->binary_files == GREP_BINARY_FILES_WITHOUT_MATCH) {
		memset(result_ptr, 0, sizeof(*result_ptr));
		if (options->count_only && !options->quiet) {
			if (grep_emit_count(label, show_filename, 0) < 0) {
				return -1;
			}
		}
		return 0;
	}
	if (grep_process_text(options, matchers, label, show_filename, text, text_len,
			!is_binary || options->binary_files == GREP_BINARY_FILES_TEXT,
			result_ptr) < 0) {
		return -1;
	}
	if (is_binary && options->binary_files == GREP_BINARY_FILES_BINARY
			&& result_ptr->selected_count > 0 && !options->quiet
			&& !options->count_only) {
		if (grep_emit_binary_match(prog, label) < 0) {
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
	grep_js_state_t js_state;
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
	if (grep_js_state_init(&js_state,
			options.file_count > 0 ? options.file_count : 4u, 8u) < 0) {
		fprintf(stderr, "%s: out of memory\n", prog);
		grep_options_release(&options);
		return 2;
	}
	rc = grep_compile_matchers(&options, prog, &matchers);
	if (rc != 0) {
		grep_js_state_free(&js_state);
		grep_options_release(&options);
		return rc;
	}

	if (grep_expand_sources(&options, prog, &js_state, &had_error) < 0) {
		fprintf(stderr, "%s: out of memory\n", prog);
		grep_release_matchers(matchers, options.pattern_count);
		grep_js_state_free(&js_state);
		grep_options_release(&options);
		return 2;
	}

	show_filename = options.no_filename ? 0
		: options.force_filename ? 1
		: jsval_array_length(&js_state.region, js_state.sources) > 1;
	for (i = 0; i < jsval_array_length(&js_state.region, js_state.sources); i++) {
		jsval_t path_value;
		size_t path_len = 0;
		uint8_t *text = NULL;
		size_t text_len = 0;
		grep_source_result_t source_result;

		if (jsval_array_get(&js_state.region, js_state.sources, i,
				&path_value) < 0) {
			fprintf(stderr, "%s: processing failed: %s\n", prog,
					strerror(errno));
			grep_release_matchers(matchers, options.pattern_count);
			grep_js_state_free(&js_state);
			grep_options_release(&options);
			return 2;
		}
		if (jsval_string_copy_utf8(&js_state.region, path_value, NULL, 0,
				&path_len) < 0) {
			fprintf(stderr, "%s: processing failed: %s\n", prog,
					strerror(errno));
			grep_release_matchers(matchers, options.pattern_count);
			grep_js_state_free(&js_state);
			grep_options_release(&options);
			return 2;
		}
		{
			uint8_t path_buf[path_len + 1u];
			const char *path;

			if (jsval_string_copy_utf8(&js_state.region, path_value, path_buf,
					path_len, NULL) < 0) {
				fprintf(stderr, "%s: processing failed: %s\n", prog,
						strerror(errno));
				grep_release_matchers(matchers, options.pattern_count);
				grep_js_state_free(&js_state);
				grep_options_release(&options);
				return 2;
			}
			path_buf[path_len] = '\0';
			path = (const char *)path_buf;

			if (grep_slurp_path(path, &text, &text_len) < 0) {
				had_error = 1;
				if (!options.suppress_errors) {
					fprintf(stderr, "%s: %s: %s\n", prog, path,
							strerror(errno));
				}
				continue;
			}
			if (grep_process_source(&options, matchers, prog,
					grep_source_label(path), show_filename, text, text_len,
					&source_result) < 0) {
				free(text);
				fprintf(stderr, "%s: processing failed: %s\n", prog,
						strerror(errno));
				grep_release_matchers(matchers, options.pattern_count);
				grep_js_state_free(&js_state);
				grep_options_release(&options);
				return 2;
			}
			free(text);
			if (source_result.selected_count > 0) {
				had_selected = 1;
			}
			if (source_result.quiet_match) {
				grep_release_matchers(matchers, options.pattern_count);
				grep_js_state_free(&js_state);
				grep_options_release(&options);
				return 0;
			}
		}
	}

	grep_release_matchers(matchers, options.pattern_count);
	grep_js_state_free(&js_state);
	grep_options_release(&options);
	if (had_error) {
		return 2;
	}
	return had_selected ? 0 : 1;
}
